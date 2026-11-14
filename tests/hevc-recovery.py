#!/usr/bin/env python3
import pathlib,subprocess
D=pathlib.Path('/root/venc-deploy')
fw=pathlib.Path('/lib/firmware/meson/vdec/meson8b_hevc.bin');saved=fw.read_bytes()
def decode(name,source='small.mp4'):
 with (D/(name+'.log')).open('w') as log:
  result=subprocess.run(['timeout','-k','5','15',str(D/'ffmpeg'),'-nostdin','-v','info','-c:v','hevc_v4l2m2m','-num_capture_buffers','8','-i',str(D/source),'-pix_fmt','nv12','-f','framemd5','-y',f'/tmp/{name}.md5'],stdout=log,stderr=log)
 frames=[s for s in pathlib.Path(f'/tmp/{name}.md5').read_text().splitlines() if s and not s.startswith('#')] if pathlib.Path(f'/tmp/{name}.md5').exists() else []
 print(name,result.returncode,len(frames),flush=True)
 return result.returncode,len(frames)
try:
 subprocess.run(['insmod',str(D/'meson8b-runtime.ko')],check=True);subprocess.run(['insmod',str(D/'meson-vdec.ko')],check=True)
 fw.write_bytes(saved[:100]);assert decode('hevc-truncated')[0]!=0
 fw.write_bytes(saved);assert decode('hevc-after-fw')==(0,10)
 original=(D/'small.hevc').read_bytes();(D/'corrupt.hevc').write_bytes(original[:len(original)//2]+b'\x00'*2048+original[len(original)//2+2048:])
 decode('hevc-corrupt','corrupt.hevc')
 assert decode('hevc-after-corrupt')==(0,10)
 assert decode('hevc-final-1080','1080p.mp4')==(0,30)
 assert decode('hevc-final-crop','crop.mp4')==(0,10)
 print('DONE',flush=True)
finally:
 fw.write_bytes(saved)
 subprocess.run(['rmmod','meson_vdec']);subprocess.run(['rmmod','meson8b_runtime'])
