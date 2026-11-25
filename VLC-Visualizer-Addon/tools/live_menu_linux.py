"""Inspect or press one accessibility action in a VLC/native-settings test.

Development-only Linux helper. --session is the session.json written when
launching a private VLC test; this never selects another process by title.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import ctypes
import ctypes.util
import re
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, required=True)
    parser.add_argument('--path', nargs='*', default=[])
    parser.add_argument('--press', action='store_true')
    parser.add_argument('--action', help='Exact observed action name when an object has multiple actions')
    parser.add_argument('--dismiss-menu', action='store_true', help='Dismiss this VLC popup with Escape after verifying active X11 ownership')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    session = json.loads(args.session.read_text())
    pid = session['pid']
    process = Path('/proc')/str(pid)
    executable = Path(os.readlink(process/'exe'))
    valid = executable.name == 'vlc'
    if session.get('kind') == 'settings':
        command = (process/'cmdline').read_bytes().split(b'\0')
        valid = (executable == Path(session['suite'])/'native/VisualizerNative' and
                 b'--settings' in command and str(session['profile']).encode() in command)
    if process.stat().st_uid != os.getuid() or not valid:
        raise RuntimeError('The recorded process is not this user\'s expected VLC/settings test')
    os.environ['DISPLAY'] = ':0'
    os.environ['DBUS_SESSION_BUS_ADDRESS'] = 'unix:path=/run/user/'+str(os.getuid())+'/bus'
    import gi
    gi.require_version('Atspi', '2.0')
    from gi.repository import Atspi
    desktop = Atspi.get_desktop(0)
    matches = []
    for n in range(desktop.get_child_count()):
        app = desktop.get_child_at_index(n)
        try:
            if app.get_process_id() == pid:
                matches.append(app)
        except Exception:
            pass
    if len(matches) != 1:
        raise RuntimeError('Expected exactly one accessibility app for the recorded PID')
    root = matches[0]
    if args.dismiss_menu:
        if args.press or args.path or session.get('kind') == 'settings':
            raise RuntimeError('Dismiss one VLC menu separately from other actions')
        lines = subprocess.check_output(['wmctrl', '-lp'], text=True).splitlines()
        windows = [row.split(None, 4) for row in lines if len(row.split(None, 4)) >= 4 and row.split(None, 4)[2] == str(pid)]
        if len(windows) != 1:
            raise RuntimeError('Expected one X11 test VLC window')
        window = windows[0][0]
        subprocess.run(['wmctrl', '-ia', window], check=True)
        until = time.monotonic()+2
        while True:
            active = subprocess.check_output(['xprop', '-root', '_NET_ACTIVE_WINDOW'], text=True)
            ids = re.findall(r'0x[0-9a-fA-F]+', active)
            if ids and int(ids[-1], 16) == int(window, 16):
                break
            if time.monotonic() > until:
                raise RuntimeError('Test VLC did not become the active X11 window')
            time.sleep(.05)
        x11 = ctypes.CDLL(ctypes.util.find_library('X11'))
        xt = ctypes.CDLL(ctypes.util.find_library('Xtst'))
        x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x11.XOpenDisplay.restype = ctypes.c_void_p
        display = x11.XOpenDisplay(b':0')
        if not display:
            raise RuntimeError('Cannot open the test X display')
        x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
        x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        x11.XGetInputFocus.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int)]
        xt.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        try:
            focus, revert = ctypes.c_ulong(), ctypes.c_int()
            x11.XGetInputFocus(display, ctypes.byref(focus), ctypes.byref(revert))
            if focus.value != int(window, 16):
                owner = subprocess.check_output(['xprop', '-id', hex(focus.value), '_NET_WM_PID'], text=True)
                actual = re.findall(r'=\s*(\d+)', owner)
                if not actual or int(actual[-1]) != pid:
                    raise RuntimeError('Actual keyboard focus is not owned by test VLC; no input sent')
            key = x11.XKeysymToKeycode(display, 0xff1b)
            if not key or not xt.XTestFakeKeyEvent(display, key, 1, 0) or not xt.XTestFakeKeyEvent(display, key, 0, 0):
                raise RuntimeError('Could not send the test Escape key')
            x11.XSync(display, 0)
        finally:
            x11.XCloseDisplay(display)

    def walk(obj, depth=0):
        if depth > 10:
            return
        yield obj, depth
        for n in range(min(obj.get_child_count(), 128)):
            yield from walk(obj.get_child_at_index(n), depth+1)

    target = root
    for name in args.path:
        candidates = [obj for obj, depth in walk(target) if depth > 0 and obj.get_name() == name]
        if len(candidates) != 1:
            raise RuntimeError('Expected one menu object for '+repr(name)+', found '+str(len(candidates)))
        target = candidates[0]
    report = {'pid': pid, 'path': args.path, 'pressed': False,
              'dismissed_menu': args.dismiss_menu,
              'helper_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    if args.press:
        action = target.get_action_iface()
        if action is None:
            raise RuntimeError('Inspect the object\'s action list before selecting an action')
        names = [action.get_action_name(n) for n in range(action.get_n_actions())]
        indexes = [n for n, name in enumerate(names) if name == args.action] if args.action else list(range(len(names)))
        if len(indexes) != 1:
            raise RuntimeError('Select one exact action from '+repr(names))
        index = indexes[0]
        report['action'] = names[index]
        report['pressed'] = bool(action.do_action(index))
        if not report['pressed']:
            raise RuntimeError('The accessibility action was refused')
    rows = []
    for obj, depth in walk(target):
        if len(rows) >= 1024:
            raise RuntimeError('Accessibility tree exceeds the test budget')
        action = obj.get_action_iface()
        rows.append({'depth': depth, 'role': obj.get_role_name(), 'name': obj.get_name(),
                     'actions': [action.get_action_name(n) for n in range(action.get_n_actions())] if action else []})
    report['tree'] = rows
    with args.report.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
