// SPDX-License-Identifier: GPL-2.0
/* Bounded V4L2 encoder client: formats, HCODEC ownership, timestamps,
 * KEYFRAME, drain and busy-format checks, plus deterministic raw references.
 * usage: encode-test DEVICE STREAM REF WIDTH HEIGHT FRAMES NV12|NV21|YU12 QUALITY JPEG|H264
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct mapped { unsigned char *data; unsigned int size; };
static int fd;
static struct mapped in[VIDEO_MAX_PLANES], out;
static struct v4l2_pix_format_mplane pix;
static unsigned int w, h, planar, vu;

static void die(const char *what) { perror(what); exit(1); }
static void ctl(unsigned long req, void *arg) { if (ioctl(fd, req, arg)) die("ioctl"); }
static void ready(void) {
	struct pollfd p = { .fd = fd, .events = POLLIN | POLLOUT | POLLPRI };
	if (poll(&p, 1, 10000) <= 0) die("poll timeout");
}
static void setup(unsigned int type, struct mapped *planes, unsigned int n) {
	struct v4l2_requestbuffers r = { .count=1, .type=type, .memory=V4L2_MEMORY_MMAP };
	struct v4l2_plane p[VIDEO_MAX_PLANES] = {0};
	struct v4l2_buffer b = { .type=type, .memory=V4L2_MEMORY_MMAP, .length=n, .m.planes=p };
	ctl(VIDIOC_REQBUFS, &r);
	ctl(VIDIOC_QUERYBUF, &b);
	for (unsigned int i=0; i<n; i++) {
		planes[i].size=p[i].length;
		planes[i].data=mmap(NULL,p[i].length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,p[i].m.mem_offset);
		if (planes[i].data==MAP_FAILED) die("mmap");
	}
}
static void queue(unsigned int type, unsigned int n, unsigned int frame) {
	struct v4l2_plane p[VIDEO_MAX_PLANES] = {0};
	struct v4l2_buffer b = { .type=type, .memory=V4L2_MEMORY_MMAP, .length=n, .m.planes=p,
		.timestamp={ .tv_sec=frame, .tv_usec=1234 } };
	for (unsigned int i=0; i<n; i++) {
		p[i].length=type==V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? in[i].size : out.size;
		if (type==V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) p[i].bytesused=p[i].length;
	}
	ctl(VIDIOC_QBUF,&b);
}
static struct v4l2_buffer dequeue(unsigned int type, unsigned int n, struct v4l2_plane *p) {
	struct v4l2_buffer b={ .type=type, .memory=V4L2_MEMORY_MMAP, .length=n, .m.planes=p };
	for (int tries=0; tries<100; tries++) {
		if (!ioctl(fd, VIDIOC_DQBUF, &b)) return b;
		if (errno!=EAGAIN) die("DQBUF");
		ready(); usleep(1000);
	}
	errno=ETIMEDOUT;die("DQBUF");return b;
}
static void fill(unsigned int frame, FILE *ref) {
	for (unsigned int i=0; i<pix.num_planes; i++) memset(in[i].data,128,in[i].size);
	for (unsigned int y=0;y<h;y++) {
		unsigned char *row=in[0].data+y*pix.plane_fmt[0].bytesperline;
		for (unsigned int x=0;x<w;x++) row[x]=(x*173/w+y*47/h+frame*11)%224+16;
		if (fwrite(row,1,w,ref)!=w) die("reference Y");
	}
	for (unsigned int y=0;y<h/2;y++) {
		unsigned char *row=in[1].data+y*pix.plane_fmt[1].bytesperline;
		unsigned char *vrow=planar?in[2].data+y*pix.plane_fmt[2].bytesperline:NULL;
		for (unsigned int x=0;x<w/2;x++) {
			unsigned char u=68+x*120/(w/2), v=68+y*120/(h/2);
			if (planar) { row[x]=u; vrow[x]=v; }
			else { row[2*x+vu]=u; row[2*x+!vu]=v; }
		}
		/* Reference is always NV12, independent of negotiated source. */
		for (unsigned int x=0;x<w/2;x++) {
			unsigned char uv[2]={ planar?row[x]:row[2*x+vu],planar?vrow[x]:row[2*x+!vu] };
			if (fwrite(uv,1,2,ref)!=2) die("reference UV");
		}
	}
}
int main(int argc,char **argv) {
	if (argc!=10) { fprintf(stderr,"usage: %s DEVICE STREAM REF W H FRAMES NV12|NV21|YU12 QUALITY JPEG|H264\n",argv[0]);return 2; }
	w=strtoul(argv[4],NULL,10);h=strtoul(argv[5],NULL,10);
	unsigned int frames=strtoul(argv[6],NULL,10),quality=strtoul(argv[8],NULL,10);
	/* argv[9] is JPEG or H264, for regression with this same client. */
	int jpeg=!strcmp(argv[9],"JPEG");
	if (!w||!h||(w&1)||(h&1)||!frames||quality<1||quality>100) return 2;
	planar=!strcmp(argv[7],"YU12");vu=!strcmp(argv[7],"NV21");
	FILE *stream=fopen(argv[2],"wb"),*ref=fopen(argv[3],"wb");
	if (!stream||!ref) die("fopen");
	fd=open(argv[1],O_RDWR|O_NONBLOCK);if(fd<0)die("open");
	int other=open(argv[1],O_RDWR|O_NONBLOCK);
	if(other>=0||errno!=EBUSY) { fprintf(stderr,"HCODEC ownership failed\n");return 1; }
	struct v4l2_format f={ .type=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE };
	f.fmt.pix_mp.width=w;f.fmt.pix_mp.height=h;
	f.fmt.pix_mp.pixelformat=planar?V4L2_PIX_FMT_YUV420M:vu?V4L2_PIX_FMT_NV21M:V4L2_PIX_FMT_NV12M;
	ctl(VIDIOC_S_FMT,&f);pix=f.fmt.pix_mp;
	f.type=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;f.fmt.pix_mp.pixelformat=jpeg?V4L2_PIX_FMT_JPEG:V4L2_PIX_FMT_H264;
	ctl(VIDIOC_S_FMT,&f);
	if (f.fmt.pix_mp.pixelformat!=(jpeg?V4L2_PIX_FMT_JPEG:V4L2_PIX_FMT_H264)) return 1;
	struct v4l2_control c={ .id=V4L2_CID_JPEG_COMPRESSION_QUALITY,.value=quality };ctl(VIDIOC_S_CTRL,&c);
	setup(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,in,pix.num_planes);
	setup(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,&out,1);
	if(ioctl(fd,VIDIOC_S_FMT,&f)!=-1||errno!=EBUSY) {fprintf(stderr,"busy S_FMT failed\n");return 1;}
	struct v4l2_event_subscription sub={ .type=V4L2_EVENT_EOS };ctl(VIDIOC_SUBSCRIBE_EVENT,&sub);
	unsigned int type=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;ctl(VIDIOC_STREAMON,&type);
	type=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;ctl(VIDIOC_STREAMON,&type);
	for(unsigned int frame=0;frame<frames;frame++) {
		fill(frame,ref);queue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,1,frame);
		queue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,pix.num_planes,frame);
		struct v4l2_plane p[VIDEO_MAX_PLANES]={0};
		struct v4l2_buffer b=dequeue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,1,p);
		if((b.flags&V4L2_BUF_FLAG_ERROR)||!p[0].bytesused||p[0].bytesused>out.size||
		   b.timestamp.tv_sec!=frame||b.timestamp.tv_usec!=1234||
		   (jpeg&&!(b.flags&V4L2_BUF_FLAG_KEYFRAME))) {fprintf(stderr,"invalid capture frame %u flags %#x size %u\n",frame,b.flags,p[0].bytesused);return 1;}
		if(fwrite(out.data,1,p[0].bytesused,stream)!=p[0].bytesused)die("stream write");
		fprintf(stderr,"frame %u bytes %u flags %#x\n",frame,p[0].bytesused,b.flags);
		dequeue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,pix.num_planes,p);
	}
	queue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,1,frames);
	struct v4l2_encoder_cmd cmd={ .cmd=V4L2_ENC_CMD_STOP };ctl(VIDIOC_ENCODER_CMD,&cmd);
	struct v4l2_plane p[VIDEO_MAX_PLANES]={0};
	struct v4l2_buffer b=dequeue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,1,p);
	if(!(b.flags&V4L2_BUF_FLAG_LAST)||p[0].bytesused) {fprintf(stderr,"drain failed\n");return 1;}
	struct v4l2_event event;ctl(VIDIOC_DQEVENT,&event);if(event.type!=V4L2_EVENT_EOS)return 1;
	type=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;ctl(VIDIOC_STREAMOFF,&type);
	type=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;ctl(VIDIOC_STREAMOFF,&type);
	close(fd);fclose(stream);fclose(ref);fprintf(stderr,"PASS: %u frames, ownership, busy format, timestamps, EOS\n",frames);return 0;
}
