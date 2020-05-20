/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress_omx.cpp
** 
** Read YUV420 from a V4L2 capture -> compress in H264 using OMX -> write to a V4L2 output device
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
#include "bcm_host.h"
#include "ilclient.h"
}

#include "libyuv.h"

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include "encode_omx.h"

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
	int bandwidth = 10000000;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
	int openflags = O_RDWR | O_NONBLOCK;
	OMX_VIDEO_AVCPROFILETYPE profile = OMX_VIDEO_AVCProfileHigh;
	OMX_VIDEO_AVCLEVELTYPE level = OMX_VIDEO_AVCLevel4;
	
	int c = 0;
	while ((c = getopt (argc, argv, "hv::" "rw" "Bb:p:l:")) != -1)
	{
		switch (c)
		{
			case 'v':   verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'r':   ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			case 'w':   ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
                        case 'B':   openflags = O_RDWR; break;			
			case 'b':   bandwidth = atoi(optarg); break;	
			case 'p':   profile = decodeProfile(optarg); break;	
			case 'l':   level = decodeLevel(optarg); break;	
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				std::cout << "\t -r            : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;

				std::cout << "\t -p profile    : H264 profile (default "<< profile << ")" << std::endl;
				std::cout << "\t -l level      : H264 level (default "<< level << ")" << std::endl;
								
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
	V4L2DeviceParameters param(in_devname,0,0,0,0,verbose, openflags);
	V4l2Capture* videoCapture = V4l2Capture::create(param, ioTypeIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		bool needconvert = (V4L2_PIX_FMT_YUV420 != videoCapture->getFormat());
		int width = videoCapture->getWidth();
		int height = videoCapture->getHeight();	
		
		// init V4L2 output interface
		V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_H264, width, height, 0, verbose, openflags);
		V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{		
			LOG(NOTICE) << "Start Capturing from " << in_devname; 
			bcm_host_init();
			
			OMX_BUFFERHEADERTYPE *      buf = NULL;
			OMX_BUFFERHEADERTYPE *      out = NULL;
			COMPONENT_T *               video_encode = NULL;

			ILCLIENT_T *client = encode_init(&video_encode);
			if (client)
			{
				encode_config_input(video_encode, width, height, 30, OMX_COLOR_FormatYUV420PackedPlanar);
				encode_config_output(video_encode, OMX_VIDEO_CodingAVC, bandwidth, profile, level);

				encode_config_activate(video_encode);		
				
                                // intermediate I420 image
                                uint8 i420_p0[width*height];
                                uint8 i420_p1[width*height/2];
                                uint8 i420_p2[width*height/2];
				
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
						buf = ilclient_get_input_buffer(video_encode, 200, 0);
						if (buf != NULL)
						{
							/* fill it */
							if (needconvert) {
								int bufferSize = videoCapture->getBufferSize();
								char inbuffer[bufferSize];
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
										libyuv::kRotate0,  videoCapture->getFormat());

									libyuv::ConvertFromI420(i420_p0, width,
											i420_p1, width/2,
											i420_p2, width/2,
											(uint8*)buf->pBuffer, 0,
											width, height,
											V4L2_PIX_FMT_YUV420);
									buf->nFilledLen = width*height*3/2;
								}			
							} else {
								int rsize = videoCapture->read((char*)buf->pBuffer, buf->nAllocLen);
								LOG(DEBUG) << "read size:" << rsize << " buffer size:" << buf->nAllocLen; 
								buf->nFilledLen = rsize;
							}								
							

							if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) != OMX_ErrorNone)
							{
								fprintf(stderr, "Error emptying buffer!\n");
							}
						}
							
						out = ilclient_get_output_buffer(video_encode, 201, 0);
						if (out != NULL)
						{
							size_t sz = videoOutput->write((char*)out->pBuffer, out->nFilledLen);
							if (sz != out->nFilledLen) {
								LOG(WARN) << "fwrite: Error emptying buffer:" << sz << " " << out->nFilledLen; 
							} else {
								LOG(DEBUG) << "Writing frame size:" << sz; 
							}
							out->nFilledLen = 0;

							OMX_ERRORTYPE r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
							if (r != OMX_ErrorNone) {
								printf("Error sending buffer for filling: %x\n", r);
							}					
						} else {
							LOG(DEBUG) << "Not getting it "; 
						}
						
					}
					else if (ret == -1)
					{
						LOG(NOTICE) << "stop error:" << strerror(errno); 
						stop=1;
					}
				}
				
				encode_deactivate(video_encode);
				encode_deinit(video_encode, client);			
			}
			delete videoOutput;
		}
		
		delete videoCapture;
	}
	
	return 0;
}
