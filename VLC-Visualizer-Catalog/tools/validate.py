"""Validate compatibility metadata and the checked-in manifest-schema subset.

No packages/network are needed. Unsupported schema keywords fail explicitly;
this is not advertised as a general JSON Schema implementation.
"""
from pathlib import Path
import json
import re
from urllib.parse import urlsplit
ROOT=Path(__file__).resolve().parents[1]
schema=json.loads((ROOT/'schemas/manifest.schema.json').read_text(encoding='utf-8'))

def require(condition,message):
    if not condition:raise ValueError(message)

def manifest(value,schema=schema):
    supported={'$schema','title','description','type','const','enum','minLength','maxLength','pattern','required','properties','additionalProperties','allOf','if','then'}
    def check_schema(rules):
        require(isinstance(rules,dict) and not set(rules)-supported,'Unsupported schema keywords')
        for child in rules.get('properties',{}).values():check_schema(child)
        for child in rules.get('allOf',[]):check_schema(child)
        for name in ('if','then'):
            if name in rules:check_schema(rules[name])
    check_schema(schema)
    def validate(field,rules,location):
        require(not set(rules)-supported,'Unsupported schema keywords at '+location)
        if 'const' in rules:require(type(field) is type(rules['const']) and field==rules['const'],'Unsupported '+location)
        if 'enum' in rules:require(any(type(field) is type(item) and field==item for item in rules['enum']),'Unsupported '+location)
        if 'type' in rules:
            types={'object':dict,'string':str}
            require(rules['type'] in types,'Unsupported schema type at '+location)
            require(isinstance(field,types[rules['type']]),'Invalid '+location+' type')
        if isinstance(field,str):
            require(rules.get('minLength',0)<=len(field)<=rules.get('maxLength',2**30),'Invalid '+location+' length')
            if 'pattern' in rules:require(re.search(rules['pattern'],field) is not None,'Invalid '+location+' pattern')
        if isinstance(field,dict):
            require(set(rules.get('required',[]))<=set(field),'Required '+location+' field is missing')
            properties=rules.get('properties',{})
            if rules.get('additionalProperties') is False:require(set(field)<=set(properties),'Unknown '+location+' field')
            for name,child in properties.items():
                if name in field:validate(field[name],child,location+'.'+name)
        for child in rules.get('allOf',[]):validate(field,child,location)
        if 'if' in rules:
            try:validate(field,rules['if'],location)
            except ValueError:pass
            else:validate(field,rules.get('then',{}),location)
    validate(value,schema,'manifest')
    require(len(value['name'].encode('utf-8'))<=128,'Name exceeds the runtime UTF-8 limit')

manifest(json.loads((ROOT/'examples/example-visualizer/manifest.json').read_text(encoding='utf-8')))
manifest(json.loads((ROOT/'examples/manifest-native.json').read_text(encoding='utf-8')),json.loads((ROOT/'schemas/manifest-v2.schema.json').read_text(encoding='utf-8')))
manifest(json.loads((ROOT/'examples/manifest-webgpu.json').read_text(encoding='utf-8')),json.loads((ROOT/'schemas/manifest-v3.schema.json').read_text(encoding='utf-8')))
native_schema=json.loads((ROOT/'schemas/manifest-v4.schema.json').read_text(encoding='utf-8'))
native_example=json.loads((ROOT/'examples/manifest-dawn.json').read_text(encoding='utf-8'))
manifest(native_example,native_schema)
for system,architecture,extension in [('windows','x86_64','.dll'),('macos','arm64','.so'),('linux','x86_64','.so')]:
    manifest(dict(native_example,platform=system,architecture=architecture,entrypoint='audio_color'+extension),native_schema)
invalid=[{'api_version':3},{'dawn_revision':'0'*40},{'assets':'../outside'},{'assets':'C:/outside'},{'assets':'assets\\outside'},
         {'entrypoint':'../other.dll'},{'entrypoint':'/outside.dll'},{'entrypoint':'audio_color.so'},{'entrypoint':'audio_color.dll\u0000suffix'},
         {'architecture':'arm64'},{'classic':{'entrypoint':'classic.so','adapter':'opengl'}},{'unknown':True},{'settings':[]},{'name':'x'*129}]
for change in invalid:
    try:manifest(dict(native_example,**change),native_schema)
    except ValueError:pass
    else:raise ValueError('Invalid native example was accepted: '+str(change))
for field in native_schema['required']:
    changed=dict(native_example);del changed[field]
    try:manifest(changed,native_schema)
    except ValueError:pass
    else:raise ValueError('Missing native field was accepted: '+field)

data=json.loads((ROOT/'visualizers.json').read_text(encoding='utf-8'))
require(data['schema_version']==1 and data['addon_api_version']==1,'Unsupported catalog')
require(data['addon_api_versions']==[1,2,3],'Unsupported native catalog APIs')
development=data['development']
require(development['api_version']==4 and development['dawn_revision']==native_example['dawn_revision'],'Unsupported development runtime')
require(development['installers_available'] is False,'Do not advertise unreleased native installers')
seen=set()
for entry in data['visualizers']:
    require(re.fullmatch(r'[a-z0-9_-]{1,64}',entry['id']) is not None,'Invalid catalog ID')
    require(entry['id'] not in seen,'Duplicate catalog ID')
    seen.add(entry['id'])
    require(entry['architecture']=='x86_64' and entry['api_version'] in (1,3),'Unsupported catalog ABI')
    require(entry['native_api_version'] in (2,3) and entry['architectures']==['x86_64','arm64'],'Unsupported native ABI')
    require(entry['status'] in ('release-candidate','tested','deprecated'),'Invalid status')
    require(bool(entry['name']) and bool(entry['version']),'Missing catalog name/version')
    current=entry['development']
    require(current['windows_api_version']==(1 if entry['id']=='milkdrop' else 4),'Invalid development Windows API')
    require(current['macos_linux_api_version']==(2 if entry['id']=='milkdrop' else 4),'Invalid development native API')
    require(current['validation']=='local-dispatch-generated-pcm; real-player/release-qa-pending','Do not overstate development validation')
    for field in ('repository','installer_url'):
        if entry[field] is not None:
            url=urlsplit(entry[field]);require(url.scheme=='https' and bool(url.netloc),'Expected HTTPS URL')
for entry in development.get('additional_visualizers', []):
    identifier=entry['id']
    require(re.fullmatch(r'[a-z0-9_-]{1,64}',identifier) is not None and identifier not in seen,'Invalid or duplicate development ID')
    require(bool(entry['name']) and bool(entry['project']) and entry['api_version']==development['api_version'],'Invalid development scene')
    require(set(entry['platforms'])=={'windows-x86_64','linux-x86_64','macos-arm64'},'Unexpected development platform set')
    require(entry['configuration']==identifier+'.ini','Unexpected scene configuration filename')
    seen.add(identifier)
print(f'Validated {len(seen)} preserved/development catalog entries, APIs 1-4 examples, three native targets and {len(invalid)+len(native_schema["required"])} native rejection cases')

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,action='append',default=[],help='Also validate a source or generated package manifest')
    for file in parser.parse_args().manifest:
        value=json.loads(file.read_text(encoding='utf-8'))
        version=value['schema_version']
        require(type(version) is int and version in (1,2,3,4),'Unsupported manifest schema')
        rules=json.loads((ROOT/'schemas'/('manifest.schema.json' if version==1 else f'manifest-v{version}.schema.json')).read_text(encoding='utf-8'))
        manifest(value,rules)
        print('Validated package manifest: '+str(file))
