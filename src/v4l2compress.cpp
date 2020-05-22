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
#include <sys/ioctl.h>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include "encoderfactory.h"

// -----------------------------------------
//    capture, compress, output 
// -----------------------------------------
int compress(V4l2Capture* videoCapture, const std::string& out_devname, V4l2Access::IoType ioTypeOut, int outformat, const std::map<std::string,std::string>& opt, int & stop, int verbose=0) {
	int ret = 0;

	// init V4L2 output interface
	int width = videoCapture->getWidth();
	int height = videoCapture->getHeight();		
	V4L2DeviceParameters outparam(out_devname.c_str(), outformat, width, height, 0, verbose);
	V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
	if (videoOutput == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		ret = -1;
	}
	else
	{		
		Encoder* encoder = EncoderFactory::Create(outformat, width, height, opt, verbose);
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

	return ret;
}

