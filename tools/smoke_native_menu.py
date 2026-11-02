#!/usr/bin/env python3
r"""Exercise the installed game's menu graph through native X11 input.

Run inside an X11 session, e.g. xvfb-run -a python3 tools/smoke_native_menu.py
--data /path/to/Wacky\ Races. ImageMagick captures remain in build/linux/.
"""
import argparse
import os
import ctypes as C
import ctypes.util
from pathlib import Path
import subprocess
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--disc', type=Path, help='Course archive root; also verifies world rendering and return')
parser.add_argument('--exe', type=Path, default=root / 'build/linux/wackyraces')
parser.add_argument('--output', type=Path, default=root / 'build/linux/menu-smoke')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
log = args.output / 'navigation.log'

class Key(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('root', C.c_ulong),
                ('subwindow', C.c_ulong), ('time', C.c_ulong), ('x', C.c_int),
                ('y', C.c_int), ('x_root', C.c_int), ('y_root', C.c_int),
                ('state', C.c_uint), ('keycode', C.c_uint), ('same_screen', C.c_int)]
class Event(C.Union):
    _fields_ = [('key', Key), ('pad', C.c_long * 24)]

x = C.CDLL(ctypes.util.find_library('X11'))
x.XOpenDisplay.argtypes = [C.c_char_p]; x.XOpenDisplay.restype = C.c_void_p
x.XDefaultRootWindow.argtypes = [C.c_void_p]; x.XDefaultRootWindow.restype = C.c_ulong
x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
x.XFree.argtypes = [C.c_void_p]
x.XKeysymToKeycode.argtypes = [C.c_void_p,C.c_ulong]; x.XKeysymToKeycode.restype = C.c_uint
x.XSendEvent.argtypes = [C.c_void_p,C.c_ulong,C.c_int,C.c_long,C.POINTER(Event)]
x.XSync.argtypes = [C.c_void_p,C.c_int]
x.XCloseDisplay.argtypes = [C.c_void_p]
display = x.XOpenDisplay(None)
if not display:
    raise RuntimeError('No X11 display; run with xvfb-run -a')

def find_window():
    base = x.XDefaultRootWindow(display)
    parent = C.c_ulong(); queried_root = C.c_ulong(); children = C.POINTER(C.c_ulong)(); count = C.c_uint()
    x.XQueryTree(display,base,C.byref(queried_root),C.byref(parent),C.byref(children),C.byref(count))
    result = 0
    try:
        for i in range(count.value):
            name = C.c_void_p()
            if x.XFetchName(display,children[i],C.byref(name)) and name.value:
                if C.string_at(name) == b'Wacky Races':
                    result = children[i]
                x.XFree(name)
    finally:
        if children:
            x.XFree(children)
    return result

with log.open('w') as stream:
    command=[str(args.exe.resolve()), '--data', str(args.data), '--lang', '3',
             '--skip-startup', '--frames', '1800']
    if args.disc:
        command+=['--disc',str(args.disc)]
    # Automated runs stay off the user's speakers.
    process = subprocess.Popen(command, env=dict(os.environ, WPC_AUDIO='silent'), stdout=stream,stderr=subprocess.STDOUT)
    try:
        def wait_for(text, start=0):
            deadline = time.monotonic() + 15
            while text not in log.read_text()[start:]:
                if process.poll() is not None:
                    raise RuntimeError(f'Game exited early: {log.read_text()}')
                if time.monotonic() > deadline:
                    raise RuntimeError(f'Timed out waiting for {text}: {log.read_text()}')
                time.sleep(.02)
            time.sleep(.1)
        wait_for('Presenting main menu')
        window = find_window()
        if not window:
            raise RuntimeError('Native game window not found')
        keys = {'Enter':0xff0d,'Escape':0xff1b,'Up':0xff52,'Down':0xff54,'Left':0xff51,'Right':0xff53}
        def press(key, expected=None, closes=False):
            start = len(log.read_text())
            event = Event(); event.key.display = display; event.key.window = window
            event.key.root = x.XDefaultRootWindow(display); event.key.same_screen = 1
            event.key.keycode = x.XKeysymToKeycode(display,keys[key])
            for kind, mask in (((2,1),) if closes else ((2,1),(3,2))):
                event.key.type = kind
                if not x.XSendEvent(display,window,False,mask,C.byref(event)):
                    raise RuntimeError('XSendEvent failed')
                x.XSync(display,False); time.sleep(.08)
            if expected:
                wait_for(expected,start)
        def capture(name):
            subprocess.run(['import','-window','Wacky Races',str((args.output/name).resolve())],check=True,timeout=10)
        press('Enter','page 0xa,'); capture('player-count.png')
        press('Enter','page 0x13,'); capture('race-mode.png')
        press('Enter','page 0x1c,')
        # Satanas et Diabolo (row 0) is locked, so the page opens on Dingo.
        capture('dingo-vehicle.png')
        time.sleep(2.0); capture('dingo-vehicle-rotated.png')
        press('Enter','page 0x6,'); capture('environment.png')
        press('Enter','page 0x17b,'); capture('course.png')
        press('Down')
        press('Enter','Race request: mode 0, players 1, vehicle 3, environment 0, course 1')
        if args.disc:
            wait_for('Loaded course:')
            capture('course-world.png')
            # Verify a rendered world rather than only the successful load log.
            from PIL import Image
            image=Image.open(args.output/'course-world.png').convert('RGB')
            if len(image.getcolors(image.width*image.height) or [])<100:
                raise RuntimeError('Course capture does not contain rendered texture detail')
            press('Escape','Returned from race to menu')
            # Load a second time to exercise GPU allocation and teardown while
            # the menu preserves its selected course and vehicle state.
            press('Enter','Loaded course:')
            press('Escape','Returned from race to menu')
        else:
            wait_for('Course load failed:')
        press('Escape','page 0x6,')
        press('Escape','page 0x1c, selection 1')
        press('Escape','page 0x13,')
        press('Escape','page 0xa,')
        press('Escape','page 0x4,')
        press('Up')
        press('Enter','page 0x251, selection 1'); capture('quit-no-default.png')
        press('Enter','page 0x4,')
        press('Enter','page 0x251, selection 1')
        press('Up'); press('Enter', closes=True)
        if process.wait(timeout=10):
            raise RuntimeError(f'Game failed: {log.read_text()}')
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
        x.XCloseDisplay(display)
print(log.read_text(), end='')
print('Native menu input/navigation/selection retention/quit smoke test passed')
