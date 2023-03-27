/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2detect.cpp
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

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv/ml.h>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>

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
	V4l2IoType ioTypeIn  = IOTYPE_MMAP;
	V4l2IoType ioTypeOut = IOTYPE_MMAP;
	std::string outFormatStr = "YU12";
	
	while ((c = getopt (argc, argv, "hv::" "o:" "rw")) != -1)
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
			case 'r':	ioTypeIn  = IOTYPE_READWRITE; break;			
			case 'w':	ioTypeOut = IOTYPE_READWRITE; break;	
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
	

	const char *cascade_name = "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml";
	cv::CascadeClassifier cascade;
	cascade.load(cascade_name);
  
    
	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname, v4l2_fourcc('Y', 'U', 'Y', 'V'), 640, 480, 0, ioTypeIn, verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param);
	
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
		int outformat =  v4l2_fourcc('B', 'G', 'R', '3'); //v4l2_fourcc(outFormatStr[0],outFormatStr[1],outFormatStr[2],outFormatStr[3]);
		V4L2DeviceParameters outparam(out_devname, outformat, videoCapture->getWidth(), videoCapture->getHeight(), 0, ioTypeOut, verbose);
		V4l2Output* videoOutput = V4l2Output::create(outparam);
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
				// intermediate I420 image
				uint8_t i420[width*height*2];
				uint8_t* i420_p0=i420;
				uint8_t* i420_p1=&i420[width*height];
				uint8_t* i420_p2=&i420[width*height*3/2];								
				
				timeval tv;
				
				LOG(NOTICE) << "Start Copying from " << in_devname << " to " << out_devname; 
				signal(SIGINT,sighandler);				
				while (!stop) 
				{
					tv.tv_sec=1;
					tv.tv_usec=0;
					int ret = videoCapture->isReadable(&tv);
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
							libyuv::ConvertToI420((const uint8_t*)inbuffer, rsize,
								i420_p0, width,
								i420_p1, width/2,
								i420_p2, width/2,
								0, 0,
								width, height,
								width, height,
								libyuv::kRotate0, informat);

							
							char outBuffer[width*height*3];
							libyuv::ConvertFromI420(i420_p0, width,
									i420_p1, width/2,
									i420_p2, width/2,
									(uint8_t*)outBuffer, 0,
									width, height, 
									outformat);
							
							

							cv::Mat input(width, height, CV_8UC3, outBuffer);
                                                        std::vector<cv::Rect> faces;
                                                        cascade.detectMultiScale( input, faces, 1.11, 4, 0);
							LOG(NOTICE) << "faces " << faces.size(); 
							for (cv::Rect r : faces) 
							{
								LOG(NOTICE) << r.x << "x" << r.y << " -> " << r.x+r.width << "x" << r.y+r.height ;
								cv::rectangle( input, r, cv::Scalar(255,0,0) );
							}
							
							int wsize = videoOutput->write((char*)input.data, width*height*3);
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
			delete videoOutput;
		}
		delete videoCapture;
	}
	
	return 0;
}
