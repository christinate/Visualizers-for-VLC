// Run the actual settings reader without creating an Electron window.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const source=fs.readFileSync(path.join(__dirname,'../browser/main.cjs'),'utf8');
const reader=source.slice(source.indexOf('function settingsFor('),source.indexOf('function live('));
const settingsFor=vm.runInNewContext(reader+';settingsFor',{fs,path,process:{platform:'win32'},loadJson:file=>JSON.parse(fs.readFileSync(file,'utf8'))});
const profile=path.join(__dirname,'../build/settings-test-'+process.pid);fs.mkdirSync(profile,{recursive:true});
assert.equal(settingsFor(profile,'geiss').hdr,false);
fs.writeFileSync(path.join(profile,'settings.ini'),'[milkdrop]\nfullscreen=1\n[geiss]\nwidth=1280\nheight=720\nfps=30\nfullscreen=1\nhdr=0\npeak=900\n');
let value=settingsFor(profile,'geiss');assert.equal(value.width,1280);assert.equal(value.height,720);assert.equal(value.fps,30);assert.equal(value.fullscreen,true);assert.equal(value.hdr,false);assert.equal(value.peak,900);
fs.writeFileSync(path.join(profile,'geiss.json'),JSON.stringify({fullscreen:false,width:800,supersampling:2,hdr:false}));
value=settingsFor(profile,'geiss');assert.equal(value.fullscreen,false);assert.equal(value.width,800);assert.equal(value.supersampling,2);
console.log(JSON.stringify({result:'pass',checks:['experimental HDR defaults off','old INI display settings migrate','zero is false','JSON preferences take precedence','supersampling setting']}));
