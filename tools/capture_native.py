#!/usr/bin/env python3
"""Capture a bounded native main-menu run on an existing X11 display.

Example: xvfb-run -a python3 tools/capture_native.py --data /path/to/game
Requires ImageMagick's import command. Assets are opened read-only.
"""
import argparse
import os
from pathlib import Path
import subprocess
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path, default=root / 'build/linux/wackyraces')
parser.add_argument('--data', type=Path, required=True)
parser.add_argument('--lang', type=int, default=0)
parser.add_argument('--disc', type=Path, help='disc root with Movies/intro.avi (startup mode)')
parser.add_argument('--startup', action='store_true', help='capture legal.avi, both startup cards and the intro; verify card timing/pixels (requires Pillow)')
parser.add_argument('--output', type=Path, default=root / 'build/linux/main-menu.png')
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.unlink(missing_ok=True)
log = args.output.with_suffix('.log')
with log.open('w') as stream:
    command = [str(args.exe.resolve()), '--data', str(args.data), '--lang', str(args.lang)]
    if args.disc:
        command += ['--disc', str(args.disc)]
    # Startup mode stops once the intro is reached instead of waiting 78 s.
    command += ['--frames', '100000'] if args.startup else ['--frames', '120', '--skip-startup']
    # Automated runs stay off the user's speakers.
    process = subprocess.Popen(command, env=dict(os.environ, WPC_AUDIO='silent'),
                               stdout=stream, stderr=subprocess.STDOUT)
    try:
        # Original order (0x4acad0): legal.avi, L2, L3, then Movies/intro.avi.
        stages = [('Playing movie legal.avi', args.output.with_name(args.output.stem + '-legal.png')),
                  ('Presenting L2.bmp', args.output.with_name(args.output.stem + '-l2.png')),
                  ('Presenting L3.bmp', args.output.with_name(args.output.stem + '-l3.png')),
                  ('Movies/intro.avi', args.output.with_name(args.output.stem + '-intro.png'))] if args.startup else \
                 [('Presenting main menu', args.output)]
        observations = []
        for marker, output in stages:
            stage = marker.split(' ', 1)[1] if marker.startswith('Presenting ') else marker
            deadline = time.monotonic() + 25
            while marker not in log.read_text():
                if process.poll() is not None:
                    raise RuntimeError(f'Native launch exited early ({process.returncode}): {log.read_text()}')
                if time.monotonic() > deadline:
                    raise RuntimeError(f'Native {stage} startup timeout')
                time.sleep(0.02)
            observations.append(time.monotonic())
            # Movies: let a few frames play before capturing.
            time.sleep(1.0 if 'avi' in marker else 0.15)
            subprocess.run(['import', '-window', 'Wacky Races', str(output.resolve())],
                           check=True, timeout=10)
            if args.startup and stage.endswith('.bmp'):
                from PIL import Image, ImageChops
                original = next(p for p in args.data.iterdir() if p.name.lower() == stage.lower())
                with Image.open(original) as source, Image.open(output) as capture:
                    if source.size != capture.size or ImageChops.difference(source.convert('RGB'), capture.convert('RGB')).getbbox():
                        raise RuntimeError(f'Presented {stage} pixels differ from the original BMP')
        if args.startup:
            for duration in (observations[2]-observations[1], observations[3]-observations[2]):
                if not 1.8 <= duration <= 2.2:
                    raise RuntimeError(f'Startup card duration {duration:.3f}s differs from recovered two seconds')
            print('Both startup cards match original BMP pixels; two-second transitions verified')
            process.terminate()
        elif process.wait(timeout=15):
            raise RuntimeError(f'Native run failed: {log.read_text()}')
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
print(log.read_text(), end='')
print(f'Capture: {args.output}')
