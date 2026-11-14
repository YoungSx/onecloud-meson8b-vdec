#!/usr/bin/env python3
import subprocess,time,json,pathlib,resource
D=pathlib.Path('/root/venc-deploy');F='/usr/local/bin/ffmpeg'
def run(args,name):
 start=time.monotonic();r0=resource.getrusage(resource.RUSAGE_CHILDREN)
 with open(D/(name+'.log'),'w') as f:subprocess.run(args,stdout=f,stderr=f,check=True,timeout=90)
 r1=resource.getrusage(resource.RUSAGE_CHILDREN)
 return {'name':name,'wall':time.monotonic()-start,'cpu':r1.ru_utime+r1.ru_stime-r0.ru_utime-r0.ru_stime}
subprocess.run(['insmod',str(D/'meson-vdec.ko')],check=True)
subprocess.run(['insmod',str(D/'meson-venc.ko')],check=True)
results=[]
try:
 for i in range(5):
  results.append(run([F,'-nostdin','-hide_banner','-v','info','-c:v','h264_v4l2m2m','-num_capture_buffers','7','-i',str(D/'baseline.mp4'),'-an','-c:v','h264_v4l2m2m','-b:v','0','-fps_mode','passthrough','-f','h264','-y',f'/tmp/concurrent-{i}.h264'],f'concurrent-{i}'))
  run([F,'-nostdin','-hide_banner','-v','info','-i',f'/tmp/concurrent-{i}.h264','-f','null','-'],f'concurrent-decode-{i}')
  print(results[-1],flush=True)
 pathlib.Path(D/'concurrent-results.json').write_text(json.dumps(results,indent=2))
 print('DONE',flush=True)
finally:
 subprocess.run(['rmmod','meson_venc']);subprocess.run(['rmmod','meson_vdec'])
