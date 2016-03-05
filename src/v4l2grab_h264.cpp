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

#include "encode_omx.h"

#define NUMFRAMES 300000


int take_snapshot(void *buffer, DISPMANX_DISPLAY_HANDLE_T display, DISPMANX_RESOURCE_HANDLE_T resource, DISPMANX_MODEINFO_T info, VC_RECT_T *rect)
{
	DISPMANX_TRANSFORM_T transform = DISPMANX_NO_ROTATE;
	int ret = 0;

	ret = vc_dispmanx_snapshot(display, resource, transform);
	assert(ret == 0);

	ret = vc_dispmanx_resource_read_data(resource, rect, buffer, info.width * 3);
	assert(ret == 0);

	return info.width * info.height * 3;
}

int main(int argc, char **argv) 
{
	if (argc < 2) 
	{
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "Record to file: %s <filename>\n", argv[0]);
		exit(1);
	}

	bcm_host_init();
	
	DISPMANX_DISPLAY_HANDLE_T   display;
	DISPMANX_RESOURCE_HANDLE_T  resource;
	DISPMANX_MODEINFO_T         info;
	VC_RECT_T                   rect;
	uint32_t                    screen = 0;
	uint32_t                    vc_image_ptr;
	OMX_BUFFERHEADERTYPE *      buf = NULL;
	OMX_BUFFERHEADERTYPE *      out = NULL;
	COMPONENT_T *               video_encode = NULL;

	int status = 0;
	int framenumber = 0;

	int ret = 0;

	fprintf(stderr, "Open display[%i]...\n", screen );
	display = vc_dispmanx_display_open( screen );

	ret = vc_dispmanx_display_get_info(display, &info);
	assert(ret == 0);
	fprintf(stderr, "Display is %d x %d\n", info.width, info.height );
	resource = vc_dispmanx_resource_create( VC_IMAGE_BGR888,
		   info.width,
		   info.height,
		   &vc_image_ptr );

	fprintf(stderr, "VC image ptr: 0x%X\n", vc_image_ptr);

	ret = vc_dispmanx_rect_set(&rect, 0, 0, info.width, info.height);
	assert(ret == 0);
	
	
	V4L2DeviceParameters outparam(argv[1], V4L2_PIX_FMT_H264,  info.width, info.height, 0, true);
	V4l2Output* outDev = V4l2Output::createNew(outparam);	

	ILCLIENT_T *client = encode_init(&video_encode);
	if (client)
	{
		encode_config_input(video_encode, info.width, info.height, 30, OMX_COLOR_Format24bitBGR888);
		encode_config_output(video_encode, OMX_VIDEO_CodingAVC, 10000000);

		encode_config_activate(video_encode);

		fprintf(stderr, "looping for buffers...\n");
		do
		{
			buf = ilclient_get_input_buffer(video_encode, 200, 0);
			if (buf != NULL)
			{
				/* fill it */
				buf->nFilledLen = take_snapshot(buf->pBuffer, display, resource, info, &rect);
				framenumber++;

				if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) != OMX_ErrorNone)
				{
					fprintf(stderr, "Error emptying buffer!\n");
				}
			}
				
			out = ilclient_get_output_buffer(video_encode, 201, 0);
			if (out != NULL)
			{
				OMX_ERRORTYPE r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
				if (r != OMX_ErrorNone)
				{
					fprintf(stderr, "Error filling buffer: %x\n", r);
				}
				if (out->nFilledLen > 0)
				{
					size_t sz = outDev->write((char*)out->pBuffer, out->nFilledLen);
					if (sz != out->nFilledLen)
					{
						fprintf(stderr, "fwrite: Error emptying buffer: %d!\n", sz);
					}
					else
					{
//						fprintf(stderr, "Writing frame size:%d %d/%d\n", sz ,framenumber, NUMFRAMES);
					}
				}
				
				out->nFilledLen = 0;
			}

		} while (framenumber < NUMFRAMES);
		
		fprintf(stderr, "Exit\n");
		
		encode_deactivate(video_encode);
		encode_deinit(video_encode, client);		
	}	
	delete outDev;
	
	return status;
}
