/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2copy.cpp
** 
** Copy from a V4L2 capture device to an other V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "libyuv.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

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
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *in_devname = "/dev/video0";	
	const char *out_devname = "/dev/video1";	
	int c = 0;
	bool useMmapIn = true;
	bool useMmapOut = true;
	std::string outFormatStr = "YU12";
	
	while ((c = getopt (argc, argv, "hv::" "o" "rw")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				std::cout << "\t -o <format>   : output YUV format" << std::endl;
				std::cout << "\t -r            : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t source_device : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				std::cout << "\t dest_device   : V4L2 capture device (default "<< out_devname << ")" << std::endl;
				exit(0);
			}
			case 'r':	useMmapIn = false; break;					
			case 'w':	useMmapOut = false; break;
			case 'o':       outFormatStr = optarg ; break;
			default:
				std::cout << "option :" << c << " is unknown" << std::endl;
				break;
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
	// complete the fourcc
	while (outFormatStr.size() < 4)
	{
		outFormatStr.append(" ");
	}
	
	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname, 0, 0, 0, 0,verbose);
	V4l2Capture* videoCapture = V4l2DeviceFactory::CreateVideoCapure(param, useMmapIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		int informat =  videoCapture->getFormat();
		int width    =  videoCapture->getWidth();		
		int height   =  videoCapture->getHeight();
				
		// init V4L2 output interface
		V4L2DeviceParameters outparam(out_devname, videoCapture->getFormat(), videoCapture->getWidth(), videoCapture->getHeight(), 0,verbose);
		V4l2Output* videoOutput = V4l2DeviceFactory::CreateVideoOutput(outparam, useMmapOut);
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{	
			// check output size
			if ( (videoOutput->getWidth() != width) || (videoOutput->getHeight() != height) )
			{
				LOG(WARN) << "Cannot rescale input:" << videoCapture->getWidth()  << "x" << videoCapture->getHeight() << " output:" << videoOutput->getWidth()  << "x" << videoOutput->getHeight(); 
			}
			else
			{
				int outformat =  v4l2_fourcc(outFormatStr[0],outFormatStr[1],outFormatStr[2],outFormatStr[3]);
				
				// intermediate I420 image
				uint8 i420_p0[width*height];
				uint8 i420_p1[width*height/2];
				uint8 i420_p2[width*height/2];
				
				if (videoCapture->captureStart() == false)
				{
					LOG(WARN) << "Cannot start capture from device:" << in_devname; 
				}
				else
				{			
					fd_set fdset;
					FD_ZERO(&fdset);
					timeval tv;
					
					LOG(NOTICE) << "Start Copying from " << in_devname << " to " << out_devname; 
					signal(SIGINT,sighandler);				
					while (!stop) 
					{
						tv.tv_sec=1;
						tv.tv_usec=0;
						FD_SET(videoCapture->getFd(), &fdset);
						int ret = select(videoCapture->getFd()+1, &fdset, NULL, NULL, &tv);
						if (ret == 1)
						{
							char inbuffer[videoCapture->getBufferSize()];
							int rsize = videoCapture->read(inbuffer, sizeof(inbuffer));
							if (rsize == -1)
							{
								LOG(NOTICE) << "stop " << strerror(errno); 
								stop=1;					
							}
							else
							{
								libyuv::ConvertToI420((const uint8*)inbuffer, rsize,
									i420_p0, width,
									i420_p1, width/2,
									i420_p2, width/2,
									0, 0,
									width, height,
									width, height,
									libyuv::kRotate0, informat);

								char outBuffer[videoOutput->getBufferSize()];
								libyuv::ConvertFromI420(i420_p0, width,
										i420_p1, width/2,
										i420_p2, width/2,
										(uint8*)outBuffer, sizeof(outBuffer),
										width, height,
										outformat);
								
								
								int wsize = videoOutput->write(outBuffer, sizeof(outBuffer));
								LOG(DEBUG) << "Copied " << rsize << " " << wsize; 
							}
						}
						else if (ret == -1)
						{
							LOG(NOTICE) << "stop " << strerror(errno); 
							stop=1;
						}
					}
				}
			}
			delete videoOutput;
		}
		delete videoCapture;
	}
	
	return 0;
}
