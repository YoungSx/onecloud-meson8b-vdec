#!/usr/bin/env python3
"""Five bounded VDEC1->HCODEC sessions on the pinned 60-frame Baseline sample.

Run detached with wdt-guard.py. Module loading is unwound even if the
second probe fails; software decode checks frame count after every output.
"""
import json
import pathlib
import re
import resource
import subprocess
import time

DEPLOY = pathlib.Path('/root/venc-deploy')
FFMPEG = '/usr/local/bin/ffmpeg'


def run(args, name):
    start = time.monotonic()
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    with (DEPLOY / (name + '.log')).open('w') as output:
        subprocess.run(args, stdout=output, stderr=output, check=True, timeout=90)
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    return {'name': name, 'wall': time.monotonic() - start,
            'cpu': after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime}


def main():
    modules = []
    results = []
    try:
        for module in ('meson-vdec', 'meson-venc'):
            subprocess.run(['insmod', str(DEPLOY / (module + '.ko'))], check=True)
            modules.append(module.replace('-', '_'))
        for index in range(5):
            result = run([
                FFMPEG, '-nostdin', '-hide_banner', '-v', 'info',
                '-c:v', 'h264_v4l2m2m', '-num_capture_buffers', '7',
                '-i', str(DEPLOY / 'baseline.mp4'), '-an',
                '-c:v', 'h264_v4l2m2m', '-b:v', '0', '-fps_mode', 'passthrough',
                '-f', 'h264', '-y', f'/tmp/concurrent-{index}.h264',
            ], f'concurrent-{index}')
            name = f'concurrent-decode-{index}'
            run([FFMPEG, '-nostdin', '-hide_banner', '-v', 'info',
                 '-i', f'/tmp/concurrent-{index}.h264', '-f', 'null', '-'], name)
            frames = re.findall(r'frame=\s*(\d+)', (DEPLOY / (name + '.log')).read_text())
            if not frames or int(frames[-1]) != 60:
                raise RuntimeError('transcode output did not decode all 60 frames')
            results.append(result)
            print(result, flush=True)
        (DEPLOY / 'concurrent-results.json').write_text(json.dumps(results, indent=2))
        print('DONE', flush=True)
    finally:
        for module in reversed(modules):
            subprocess.run(['rmmod', module], check=True)


if __name__ == '__main__':
    main()
