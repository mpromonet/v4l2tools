


#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <fuse/cuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <linux/kdev_t.h>
#include <linux/videodev2.h>

static void v4l2_open(fuse_req_t req, struct fuse_file_info *fi)
{
	fprintf(stderr, "v4l2_open\n");
	fuse_reply_open(req, fi);
}

static void v4l2_read(fuse_req_t req, size_t size, off_t off,
		      struct fuse_file_info *fi)
{
	fprintf(stderr, "v4l2_read\n");	
	
	fuse_reply_buf(req, NULL, 0);
}

static void v4l2_write(fuse_req_t req, const char *buf, size_t size, off_t off,
		       struct fuse_file_info *fi)
{
	fprintf(stderr, "v4l2_write\n");

	fuse_reply_write(req, size);
}

static void v4l2_ioctl(fuse_req_t req, int cmd, void *arg,
		       struct fuse_file_info *fi, unsigned int flags,
		       const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	fprintf(stderr, "v4l2_ioctl\n");

	if (flags & FUSE_IOCTL_COMPAT) {
			fuse_reply_err(req, ENOSYS);
			return;
	}

	static struct v4l2_capability cap;
	memset(&cap,0,sizeof(cap));
	cap.capabilities = V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_VIDEO_OUTPUT|V4L2_CAP_READWRITE;
	strcpy(cap.driver, "v4l2_cuse");
	
	static struct v4l2_format     fmt;
	memset(&fmt,0,sizeof(fmt));
	fmt.type  = V4L2_CAP_VIDEO_OUTPUT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.width = 320;
	fmt.fmt.pix.height = 200;
	fmt.fmt.pix.sizeimage = fmt.fmt.pix.width * fmt.fmt.pix.width * 2;
		
	switch (cmd) {
		case VIDIOC_QUERYCAP:
			fprintf(stderr, "VIDIOC_QUERYCAP\n");
			if (!out_bufsz) {
				struct iovec iov = { arg, sizeof(cap) };
				fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
			} else {
				fuse_reply_ioctl(req, 0, &cap, sizeof(cap));		
			}
			break;
			
		case VIDIOC_G_FMT:
			fprintf(stderr, "VIDIOC_G_FMT\n");
			if (!out_bufsz) {
				struct iovec iov = { arg, sizeof(fmt) };
				fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
			} else {
				fuse_reply_ioctl(req, 0, &fmt, sizeof(fmt));		
			}
			break;
			
		case VIDIOC_S_FMT:
			fprintf(stderr, "VIDIOC_S_FMT\n");
			if (!in_bufsz || !out_bufsz) {
				struct iovec iov_r = { arg, sizeof(fmt) };
				struct iovec iov_w = { arg, sizeof(fmt) };
				fuse_reply_ioctl_retry(req, &iov_r, 1, &iov_w, 1);
			} else {
				struct v4l2_format *in_fmt = (struct v4l2_format*)in_buf;
				memcpy(&fmt, in_fmt, sizeof(fmt));
				fuse_reply_ioctl(req, 0, &fmt, sizeof(fmt));		
			}			
			break;
			
		default:
			fuse_reply_err(req, EINVAL);
			break;
	}
}

static struct cuse_lowlevel_ops v4l2_oper = {
	.open		= v4l2_open,
	.read		= v4l2_read,
	.write		= v4l2_write,
	.ioctl		= v4l2_ioctl,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	char dev_name[128] = "DEVNAME=video10";
	const char *dev_info_argv[] = { dev_name };
	struct cuse_info ci;

	memset(&ci, 0, sizeof(ci));
	ci.dev_major = 0;
	ci.dev_minor = 0;
	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	return cuse_lowlevel_main(args.argc, args.argv, &ci, &v4l2_oper, NULL);
}
