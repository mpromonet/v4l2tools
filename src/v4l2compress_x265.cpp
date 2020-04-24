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

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include "x265encoder.h"

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
	int fps = 25;	
	int c = 0;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
	int rc_method = X265_RC_ABR;
	int rc_value = 0;	
	
	while ((c = getopt (argc, argv, "hF:v::rw")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'F':	fps = atoi(optarg); break;
			
			case 'r':	ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			case 'w':	ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
			
			case 'q':	rc_method = X265_RC_CQP; rc_value = atoi(optarg); break;	
			case 'f':	rc_method = X265_RC_CRF;  rc_value = atof(optarg); break;	

			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				
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
	V4L2DeviceParameters param(in_devname,0,0,0,0,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param, ioTypeIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		// init V4L2 output interface
		int width = videoCapture->getWidth();
		int height = videoCapture->getHeight();
		V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_HEVC, width, height, 0, verbose);		
		V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{		
			LOG(NOTICE) << "Start Capturing from " << in_devname; 

			X265Encoder* encoder = new X265Encoder(width, height, fps, rc_method, rc_value, verbose);
			if (!encoder)
			{
				LOG(WARN) << "Cannot create X265 encoder for device:" << in_devname; 
			}
			else
			{						
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
						
						encoder->convertEncodeWrite(buffer, rsize,videoCapture->getFormat(), videoOutput);

						gettimeofday(&curTime, NULL);												
						timeval endodeTime;
						timersub(&curTime,&refTime,&endodeTime);
						refTime = curTime;

						LOG(DEBUG) << " captureTime:" << (captureTime.tv_sec*1000+captureTime.tv_usec/1000) 
								<< " endodeTime:" << (endodeTime.tv_sec*1000+endodeTime.tv_usec/1000); 							
					}
					else if (ret == -1)
					{
						LOG(NOTICE) << "stop error:" << strerror(errno); 
						stop=true;
					}
				}
				
				delete encoder;
			}
			delete videoOutput;			
		}
		delete videoCapture;
	}
	
	return 0;
}
