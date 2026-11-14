#!/usr/bin/env python3
import os,subprocess,pathlib,json
D=pathlib.Path('/root/venc-deploy')
fw=pathlib.Path('/lib/firmware/meson/venc/meson8b_jpeg_enc.bin')
original=fw.read_bytes()
try:
 subprocess.run(['insmod',str(D/'meson-venc.ko')],check=True)
 for label, blob in [('truncated',original[:256]),('missing',None)]:
  if blob is None: fw.unlink()
  else: fw.write_bytes(blob)
  result=subprocess.run(['timeout','30',str(D/'encode-test'),'/dev/video0',f'/tmp/{label}.mjpg',f'/tmp/{label}.nv12','320','240','1','NV12','90','JPEG'],capture_output=True,text=True)
  print(label,result.returncode,result.stderr,flush=True)
  assert result.returncode not in (0,124)
 fw.write_bytes(original)
 subprocess.run(['timeout','30',str(D/'encode-test'),'/dev/video0','/tmp/jpeg-recovery.mjpg','/tmp/jpeg-recovery.nv12','320','240','2','NV12','90','JPEG'],check=True)
 print('firmware error recovery PASS',flush=True)
finally:
 fw.write_bytes(original)
 subprocess.run(['rmmod','meson_venc'])
