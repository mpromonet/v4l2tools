/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress_jpeg.cpp
** 
** Read YUYC from a V4L2 capture -> compress in JPEG using libjpeg -> write to a V4L2 output device
**
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include <jpeglib.h>

int stop=0;

/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

/* ---------------------------------------------------------------------------
**  convert yuyv -> jpeg
** -------------------------------------------------------------------------*/
unsigned long yuyv2jpeg(char* image_buffer, unsigned int width, unsigned int height, unsigned int quality)
{
	struct jpeg_error_mgr jerr;
	struct jpeg_compress_struct cinfo;	
	jpeg_create_compress(&cinfo);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;	
	cinfo.in_color_space = JCS_YCbCr; 
	cinfo.err = jpeg_std_error(&jerr);
	
	unsigned char* dest = NULL;
	unsigned long  destsize = 0;
	jpeg_mem_dest(&cinfo, &dest, &destsize);
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);

	unsigned char bufline[cinfo.image_width * 3]; 
	while (cinfo.next_scanline < cinfo.image_height) 
	{ 
		// convert line from YUYV -> YUV
		for (unsigned int i = 0; i < cinfo.image_width; i += 2) 
		{ 
			unsigned int base = cinfo.next_scanline*cinfo.image_width * 2 ;
			bufline[i*3  ] = image_buffer[base + i*2  ]; 
			bufline[i*3+1] = image_buffer[base + i*2+1]; 
			bufline[i*3+2] = image_buffer[base + i*2+3]; 
			bufline[i*3+3] = image_buffer[base + i*2+2]; 
			bufline[i*3+4] = image_buffer[base + i*2+1]; 
			bufline[i*3+5] = image_buffer[base + i*2+3]; 
		} 
		JSAMPROW row = bufline; 
		jpeg_write_scanlines(&cinfo, &row, 1); 
	}
	jpeg_finish_compress(&cinfo);
	if (dest != NULL)
	{
		if (destsize < width*height*2)
		{
			memcpy(image_buffer, dest, destsize);
		}
		else
		{
			LOG(WARN) << "Buffer to small size:" << width*height*2 << " " << destsize; 
		}
		free(dest);
	}
	jpeg_destroy_compress(&cinfo);
	
	return destsize;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *in_devname = "/dev/video0";	
	const char *out_devname = "/dev/video1";	
	int width = 640;
	int height = 480;	
	int fps = 25;	
	int quality = 99;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
	
	int c = 0;
	while ((c = getopt (argc, argv, "h" "W:H:F:" "rw" "q:")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			
			// capture options
			case 'W':	width = atoi(optarg); break;
			case 'H':	height = atoi(optarg); break;
			case 'F':	fps = atoi(optarg); break;
			case 'r':	ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			
			// output options
			case 'w':	ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
			
			// JPEG options
			case 'q':	quality = atoi(optarg); break;

			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v               : verbose " << std::endl;
				std::cout << "\t -vv              : very verbose " << std::endl;
				std::cout << "\t -W width         : V4L2 capture width (default "<< width << ")" << std::endl;
				std::cout << "\t -H height        : V4L2 capture height (default "<< height << ")" << std::endl;
				std::cout << "\t -F fps           : V4L2 capture framerate (default "<< fps << ")" << std::endl;
				std::cout << "\t -r               : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w               : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				
				std::cout << "\tcompressor options" << std::endl;
				std::cout << "\t -q <quality>     : JPEG quality" << std::endl;
				
				std::cout << "\t source_device    : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				std::cout << "\t dest_device      : V4L2 output device (default "<< out_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		in_devname = argv[optind];
		optind++;
	}	
	if (optind<argc)
	{
		out_devname = argv[optind];
		optind++;
	}	
		
	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname,V4L2_PIX_FMT_YUYV,width,height,fps,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param, ioTypeIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		// init V4L2 output interface
		V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_JPEG, videoCapture->getWidth(), videoCapture->getHeight(), 0, verbose);
		V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{		
			timeval tv;
			
			LOG(NOTICE) << "Start Compressing " << in_devname << " to " << out_devname; 					
			signal(SIGINT,sighandler);
			while (!stop) 
			{
				tv.tv_sec=1;
				tv.tv_usec=0;
				int ret = videoCapture->isReadable(&tv);
				if (ret == 1)
				{
					char buffer[videoCapture->getBufferSize()];
					int rsize = videoCapture->read(buffer, sizeof(buffer));
					if (rsize == -1)
					{
						LOG(NOTICE) << "stop " << strerror(errno); 
						stop=1;					
					}
					else
					{						
						// compress
						rsize = yuyv2jpeg(buffer, videoOutput->getWidth(),  videoOutput->getHeight(), quality);							

						int wsize = videoOutput->write(buffer, rsize);
						LOG(DEBUG) << "Copied " << rsize << " " << wsize; 
					}
				}
				else if (ret == -1)
				{
					LOG(NOTICE) << "stop " << strerror(errno); 
					stop=1;
				}
			}
			delete videoOutput;
		}
		
		delete videoCapture;
	}
	
	return 0;
}
