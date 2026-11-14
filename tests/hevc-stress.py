#!/usr/bin/env python3
import pathlib,subprocess,time,json
D=pathlib.Path('/root/venc-deploy')
results=[]
try:
 subprocess.run(['insmod',str(D/'meson8b-runtime.ko')],check=True)
 subprocess.run(['insmod',str(D/'meson-vdec.ko')],check=True)
 for i in range(10):
  args=[str(D/'ffmpeg'),'-nostdin','-hide_banner','-v','info','-c:v','hevc_v4l2m2m','-num_capture_buffers','8','-i',str(D/'1080p.mp4'),'-pix_fmt','nv12','-f','framemd5','-y',f'/tmp/hevc-stress-{i}.md5']
  start=time.monotonic()
  with (D/f'hevc-stress-{i}.log').open('w') as log:
   r=subprocess.run(args,stderr=log,stdout=log,timeout=60)
  assert r.returncode==0,r.returncode
  hashes=[x.rsplit(',',1)[-1].strip() for x in pathlib.Path(f'/tmp/hevc-stress-{i}.md5').read_text().splitlines() if x and not x.startswith('#')]
  assert len(hashes)==30,len(hashes)
  if i:assert hashes==results[0]['hashes']
  results.append({'iteration':i,'seconds':time.monotonic()-start,'hashes':hashes})
  print('PASS',i,len(hashes),flush=True)
 (D/'hevc-stress-results.json').write_text(json.dumps(results,indent=2))
 print('DONE',flush=True)
finally:
 subprocess.run(['rmmod','meson_vdec']);subprocess.run(['rmmod','meson8b_runtime'])
