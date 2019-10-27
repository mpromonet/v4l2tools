/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2grab_h264.cpp
** 
** Grab raspberry screen -> compress in H264 using OMX -> write to a V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/videodev2.h>

extern "C"
{
#include "bcm_host.h"
#include "ilclient.h"
}

#include "V4l2Output.h"
#include "logger.h"

#include "encode_omx.h"

bool stop=false;
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop = true;
}

int take_snapshot(void *buffer, DISPMANX_DISPLAY_HANDLE_T display, DISPMANX_RESOURCE_HANDLE_T resource, DISPMANX_MODEINFO_T info, VC_RECT_T *rect, DISPMANX_TRANSFORM_T transform)
{
	int ret = 0;

	ret = vc_dispmanx_snapshot(display, resource, transform);
	assert(ret == 0);

	ret = vc_dispmanx_resource_read_data(resource, rect, buffer, info.width * 3);
	assert(ret == 0);

	return info.width * info.height * 3;
}

int main(int argc, char **argv) 
{
	const char *out_devname = "/dev/video1";
	int x = 0;
	int y = 0;	
	int width = 0;
	int height = 0;	
	int bandwidth = 10000000;
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
	int verbose = 0;
	
	int c = 0;
	while ((c = getopt (argc, argv, "hv::" "X:Y:W:H:" "w")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') { verbose++; };  break;
			case 'X':	x = atoi(optarg); break;
			case 'Y':	y = atoi(optarg); break;			
			case 'W':	width = atoi(optarg); break;
			case 'H':	height = atoi(optarg); break;			
			case 'w':	ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				
				std::cout << "\t -X x          : display capture x origin (default "<< x << ")" << std::endl;
				std::cout << "\t -Y y          : display capture y origin (default "<< y << ")" << std::endl;
				std::cout << "\t -W width      : display capture width (default "<< width << ")" << std::endl;
				std::cout << "\t -H height     : display capture height (default "<< height << ")" << std::endl;
				std::cout << "\t -w            : V4L2 output using write interface (default use memory mapped buffers)" << std::endl;				
								
				std::cout << "\t dest_device   : V4L2 output device (default "<< out_devname << ")" << std::endl;
				exit(0);
			}
			
		}		
	}
	if (optind<argc)
	{
		out_devname = argv[optind];
		optind++;
	}	
	
	bcm_host_init();
	
	uint32_t                    screen = 0;

	int status = 0;
	int framenumber = 0;

	int ret = 0;

	fprintf(stderr, "Open display[%i]...\n", screen );
	DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open( screen );

	DISPMANX_MODEINFO_T         info;
	ret = vc_dispmanx_display_get_info(display, &info);
	assert(ret == 0);
	fprintf(stderr, "Display is %d x %d\n", info.width, info.height );
	
	if (width == 0) {
		width = info.width;
	}
	if (height == 0) {
		height = info.height;
	}

	uint32_t                    vc_image_ptr;
	DISPMANX_RESOURCE_HANDLE_T resource = vc_dispmanx_resource_create( VC_IMAGE_BGR888, info.width, info.height, &vc_image_ptr);

	VC_RECT_T                   rect;
	ret = vc_dispmanx_rect_set(&rect, x, y, width, height);
	assert(ret == 0);
	
	// initialize log4cpp
	initLogger(verbose);
	
	V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_H264,  width, height, 0, verbose);
	V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
	if (videoOutput == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 output interface for device:" << argv[1]; 
		status = -1;
	}
	else
	{ 
		COMPONENT_T *               video_encode = NULL;

		ILCLIENT_T *client = encode_init(&video_encode);
		if (client)
		{
			encode_config_input(video_encode, width, height, 30, OMX_COLOR_Format24bitBGR888);
			encode_config_output(video_encode, OMX_VIDEO_CodingAVC, bandwidth, OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4);

			encode_config_activate(video_encode);

			fprintf(stderr, "looping for buffers...\n");
			
			while (!stop)
			{
				OMX_BUFFERHEADERTYPE *buf = ilclient_get_input_buffer(video_encode, 200, 0);
				if (buf != NULL)
				{
					/* fill it */
					buf->nFilledLen = take_snapshot(buf->pBuffer, display, resource, info, &rect, DISPMANX_NO_ROTATE);
					framenumber++;

					if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) != OMX_ErrorNone)
					{
						LOG(WARN) << "Error emptying buffer"; 
					}
				}
					
				OMX_BUFFERHEADERTYPE *out = ilclient_get_output_buffer(video_encode, 201, 0);
				if (out != NULL)
				{
					if (out->nFilledLen > 0)
					{
						size_t sz = videoOutput->write((char*)out->pBuffer, out->nFilledLen);
						if (sz != out->nFilledLen)
						{
							LOG(WARN) << "fwrite: Error emptying buffer:" << sz << "!=" << out->nFilledLen; 
						}
						else
						{
							LOG(DEBUG) << "Writing frame size:" << sz; 
						}
					}
					
					out->nFilledLen = 0;

					OMX_ERRORTYPE r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
					if (r != OMX_ErrorNone)
					{
						LOG(WARN) << "Error filling buffer:" << r ; 
					}
				}

			} 
			
			fprintf(stderr, "Exit\n");
		
			encode_deactivate(video_encode);
			encode_deinit(video_encode, client);		
		}	
		delete videoOutput;
	}
	
	return status;
}
