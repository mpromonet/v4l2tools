/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress_x265.cpp
** 
** Read YUYV from a V4L2 capture -> compress in H265 -> write to a V4L2 output device
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

extern "C" 
{
	#include "x265.h"
}

#include "libyuv.h"

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

int stop=0;
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
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
	int c = 0;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
	
	while ((c = getopt (argc, argv, "hW:H:P:F:v::rw")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'W':	width = atoi(optarg); break;
			case 'H':	height = atoi(optarg); break;
			case 'F':	fps = atoi(optarg); break;
			
			case 'r':	ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			case 'w':	ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				
				std::cout << "\t -W width      : V4L2 capture width (default "<< width << ")" << std::endl;
				std::cout << "\t -H height     : V4L2 capture height (default "<< height << ")" << std::endl;
				std::cout << "\t -F fps        : V4L2 capture framerate (default "<< fps << ")" << std::endl;				
				std::cout << "\t -r            : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;				
				
				
				std::cout << "\t source_device : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				std::cout << "\t dest_device   : V4L2 capture device (default "<< out_devname << ")" << std::endl;
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
	int format = V4L2_PIX_FMT_YUYV;
	V4L2DeviceParameters param(in_devname,format,width,height,fps,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param, ioTypeIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		// init V4L2 output interface
		V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_HEVC, videoCapture->getWidth(), videoCapture->getHeight(), 0, verbose);
		V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{		
			LOG(NOTICE) << "Start Capturing from " << in_devname; 
			x265_param param;
			x265_param_default(&param);
			if (verbose>1)
			{
				param.logLevel = X265_LOG_DEBUG;
			}
			param.sourceWidth = width;
			param.sourceHeight = height;
			param.fpsNum = fps;
			param.fpsDenom = 1;
			param.bframes = 0;
			param.bRepeatHeaders = 1;
			param.internalCsp=X265_CSP_I420;
						
			
			x265_encoder* encoder = x265_encoder_open(&param);
			if (!encoder)
			{
				LOG(WARN) << "Cannot create X265 encoder for device:" << in_devname; 
			}
			else
			{		
				x265_picture *pic_in = x265_picture_alloc();
				x265_picture_init(&param, pic_in);
				char buff[width*height*3/2];
				pic_in->planes[0]=buff;
				pic_in->planes[1]=buff+width*height;
				pic_in->planes[2]=buff+width*height*5/4;
				
				x265_picture* pic_out = x265_picture_alloc();
				
				timeval tv;
				timeval refTime;
				timeval curTime;
				
				LOG(NOTICE) << "Start Compressing " << in_devname << " to " << out_devname; 					
				signal(SIGINT,sighandler);
				while (!stop) 
				{
					tv.tv_sec=1;
					tv.tv_usec=0;
					int ret = videoCapture->isReadable(&tv);
					if (ret == 1)
					{
						gettimeofday(&refTime, NULL);	
						char buffer[videoCapture->getBufferSize()];
						int rsize = videoCapture->read(buffer, sizeof(buffer));
						
						gettimeofday(&curTime, NULL);												
						timeval captureTime;
						timersub(&curTime,&refTime,&captureTime);
						refTime = curTime;
						
						ConvertToI420((const uint8*)buffer, rsize,
							(uint8*)pic_in->planes[0], width,
							(uint8*)pic_in->planes[1], width/2,
							(uint8*)pic_in->planes[2], width/2,
							0, 0,
							width, height,
							width, height,
							libyuv::kRotate0, libyuv::FOURCC_YUY2);

						gettimeofday(&curTime, NULL);												
						timeval convertTime;
						timersub(&curTime,&refTime,&convertTime);
						refTime = curTime;
						
						x265_nal* nals = NULL;
						uint32_t i_nals = 0;
						int frame_size = x265_encoder_encode(encoder, &nals, &i_nals, pic_in, pic_out);
						
						gettimeofday(&curTime, NULL);												
						timeval endodeTime;
						timersub(&curTime,&refTime,&endodeTime);
						refTime = curTime;
						
						if (frame_size >= 0)
						{
							for (uint32_t i=0; i < i_nals; ++i)
							{
								int wsize = videoOutput->write((char*)nals[i].payload, nals[i].sizeBytes);
								LOG(INFO) << "Copied " << i+1 << "/" << i_nals << " size:" << wsize; 					
							}
						}
						
						gettimeofday(&curTime, NULL);												
						timeval writeTime;
						timersub(&curTime,&refTime,&writeTime);
						refTime = curTime;

						LOG(DEBUG) << "dts:" << pic_out->dts << " captureTime:" << (captureTime.tv_sec*1000+captureTime.tv_usec/1000) 
								<< " convertTime:" << (convertTime.tv_sec*1000+convertTime.tv_usec/1000)					
								<< " endodeTime:" << (endodeTime.tv_sec*1000+endodeTime.tv_usec/1000)
								<< " writeTime:" << (writeTime.tv_sec*1000+writeTime.tv_usec/1000); 					
						
					}
					else if (ret == -1)
					{
						LOG(NOTICE) << "stop error:" << strerror(errno); 
						stop=1;
					}
				}
				
				x265_picture_free(pic_in);
				x265_picture_free(pic_out);
				x265_encoder_close(encoder);
			}
			delete videoOutput;			
		}
		delete videoCapture;
	}
	
	return 0;
}
