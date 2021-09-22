/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress.cpp
** 
** Read YUV from a V4L2 capture -> compress -> write to a V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <signal.h>

#include <iostream>
#include <map>

#include "logger.h"

#include "V4l2Access.h"
#include "V4l2Capture.h"

#include "encoderfactory.h"

#ifdef HAVE_X264   
#include "x264encoder.h"
#endif
#ifdef HAVE_X265
#include "x265encoder.h"
#endif
#ifdef HAVE_VPX   
#include "vpxencoder.h"
#endif
#ifdef HAVE_JPEG  
#include "jpegencoder.h"
#endif
#include "yuvconverter.h"

// -----------------------------------------
//    capture, convert, output 
// -----------------------------------------
int convert(V4l2Capture* videoCapture, const std::string& out_devname, V4l2IoType ioTypeOut, int outformat, const std::map<std::string,std::string>& opt, int & stop, int verbose=0) {
	int ret = 0;

	// init V4L2 output interface
	int width = videoCapture->getWidth();
	int height = videoCapture->getHeight();		
	V4L2DeviceParameters outparam(out_devname.c_str(), outformat, width, height, 0, ioTypeOut, verbose);
	V4l2Output* videoOutput = V4l2Output::create(outparam);
	if (videoOutput == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		ret = -1;
	}
	else
	{		
		int informat = videoCapture->getFormat();
		Encoder* encoder = EncoderFactory::get().Create(outformat, informat, width, height, opt, verbose);
		if (!encoder)
		{
			LOG(WARN) << "Cannot create encoder " << V4l2Device::fourcc(outformat); 
		}
		else
		{						
			timeval tv;
			timeval refTime;
			timeval curTime;

			LOG(NOTICE) << "Start Compressing to " << out_devname;  					
			
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
					
					encoder->convertEncodeWrite(buffer, rsize, videoOutput);

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

	return ret;
}

/* ---------------------------------------------------------------------------
**  end condition
** -------------------------------------------------------------------------*/
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
	std::map<std::string,std::string> opt;
	opt["VBR"] = "1000";
	std::string strformat = "VP80";
	opt["GOP"] = "25";
	
	while ((c = getopt (argc, argv, "hv::rw" "f:" "C:V:Q:F:G:q:d:")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			
			case 'f':	strformat      = optarg; break;

			// parameters for VPx/H26x
			case 'G':	opt["GOP"] = optarg; break;
			case 'C':	opt["CBR"] = optarg; break;	
			case 'V':	opt["VBR"] = optarg; break;	
			case 'Q':	opt["RC_CQP"] = optarg; break;	
			case 'F':	opt["RC_CRF"] = optarg; break;				

			// parameters for JPEG
			case 'q':	opt["QUALITY"] = optarg; break;
			case 'd':	opt["DRI"] = optarg; break;	
			
			case 'r':	ioTypeIn  = IOTYPE_READWRITE; break;			
			case 'w':	ioTypeOut = IOTYPE_READWRITE; break;	
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v                   : verbose " << std::endl;
				std::cout << "\t -vv                  : very verbose " << std::endl;

				std::cout << "\t -C bitrate           : target CBR bitrate" << std::endl;
				std::cout << "\t -V bitrate           : target VBR bitrate" << std::endl;
				std::cout << "\t -f format            : format (default is VP80) ( supported: ";
				for (int format : EncoderFactory::get().SupportedFormat()) {
					std::cout << V4l2Device::fourcc(format) << " ";
				}
				std::cout << ")" << std::endl;

				std::cout << "\t -r                   : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w                   : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t source_device        : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				std::cout << "\t dest_device          : V4L2 capture device (default "<< out_devname << ")" << std::endl;
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

	int outformat = V4l2Device::fourcc(strformat.c_str());
		
	signal(SIGINT,sighandler);	

	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname,0,0,0,0,ioTypeIn,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param);
	
	int ret = 0;
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
		ret = -1;
	}
	else
	{
		ret = convert(videoCapture, out_devname, ioTypeOut, outformat, opt, stop, verbose);
		delete videoCapture;
	}

	return ret;
}
