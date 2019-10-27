/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** compress_omx.cpp
** 
** Helper to use omx compressor
**
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

extern "C"
{
#include "bcm_host.h"
#include "ilclient.h"
}

#include <fstream>

ILCLIENT_T * encode_init(COMPONENT_T **video_encode)
{
	ILCLIENT_T *client = ilclient_init();
	if (client == NULL)
	{
		return NULL;
	}

	if (OMX_Init() != OMX_ErrorNone)
	{
		ilclient_destroy(client);
		return NULL;
	}

	// create video_encode
	int omx_return = ilclient_create_component(client, video_encode, "video_encode",
				(ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS |
				ILCLIENT_ENABLE_INPUT_BUFFERS |
				ILCLIENT_ENABLE_OUTPUT_BUFFERS));
	if (omx_return != 0)
	{
		fprintf(stderr, "ilclient_create_component() for video_encode failed with %x!\n", omx_return);
		ilclient_destroy(client);
		return NULL;
	}

	return client;
}

static void print_def(OMX_PARAM_PORTDEFINITIONTYPE def)
{
	fprintf(stderr, "Port %u: %s %u/%u %u %u %s,%s,%s %ux%u %ux%u @%u %u\n",
	  def.nPortIndex,
	  def.eDir == OMX_DirInput ? "in" : "out",
	  def.nBufferCountActual,
	  def.nBufferCountMin,
	  def.nBufferSize,
	  def.nBufferAlignment,
	  def.bEnabled ? "enabled" : "disabled",
	  def.bPopulated ? "populated" : "not pop.",
	  def.bBuffersContiguous ? "contig." : "not cont.",
	  def.format.video.nFrameWidth,
	  def.format.video.nFrameHeight,
	  def.format.video.nStride,
	  def.format.video.nSliceHeight,
	  def.format.video.xFramerate, def.format.video.eColorFormat);
}

bool encode_config_input(COMPONENT_T* handle, int32_t width, int32_t height, int32_t framerate, OMX_COLOR_FORMATTYPE colorFormat)
{
	OMX_ERRORTYPE omx_return;

	// get current settings of video_encode component from port 200
	OMX_PARAM_PORTDEFINITIONTYPE def;
	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = 200;

	if (OMX_GetParameter(ILC_GET_HANDLE(handle), OMX_IndexParamPortDefinition, &def) != OMX_ErrorNone)
	{
		fprintf(stderr, "%s:%d: OMX_GetParameter() for video_encode port 200 failed!\n", __FUNCTION__, __LINE__);
		return NULL;
	}
	
	print_def(def);
	
	def.format.video.nFrameWidth = width;
	def.format.video.nFrameHeight = height;
	def.format.video.xFramerate = framerate << 16;
	def.format.video.nSliceHeight = ALIGN_UP(def.format.video.nFrameHeight, 16);
	def.format.video.nStride = def.format.video.nFrameWidth;
	def.format.video.eColorFormat = colorFormat;

	print_def(def);
	
	omx_return = OMX_SetParameter(ILC_GET_HANDLE(handle), OMX_IndexParamPortDefinition, &def);
	if (omx_return != OMX_ErrorNone)
	{
		fprintf(stderr, "%s:%d: OMX_SetParameter() for video_encode port 200 failed with %x!\n", __FUNCTION__, __LINE__, omx_return);
		return false;
	}
	return true;
}

bool encode_config_output(COMPONENT_T* handle, OMX_VIDEO_CODINGTYPE codec, OMX_U32 bitrate, OMX_VIDEO_AVCPROFILETYPE profile, OMX_VIDEO_AVCLEVELTYPE level)
{
	OMX_ERRORTYPE omx_return;
	
	// set format
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = 201;
	format.eCompressionFormat = codec;

	fprintf(stderr, "OMX_SetParameter(OMX_IndexParamVideoPortFormat) for video_encode:201...\n");
	omx_return = OMX_SetParameter(ILC_GET_HANDLE(handle), OMX_IndexParamVideoPortFormat, &format);
	if (omx_return != OMX_ErrorNone)
	{
		fprintf(stderr, "%s:%d: OMX_SetParameter() for video_encode port 201 failed with %x!\n", __FUNCTION__, __LINE__, omx_return);
		return false;
	}

	// ask to repeat SPS/PPS
	OMX_CONFIG_PORTBOOLEANTYPE config;
	config.nSize = sizeof(config);
	config.nVersion.nVersion = OMX_VERSION;
	config.nPortIndex = 201;
	config.bEnabled = OMX_TRUE;
	fprintf(stderr, "OMX_SetParameter(OMX_IndexParamBrcmVideoAVCInlineHeaderEnable) for video_encode:201...\n");
	omx_return = OMX_SetParameter(ILC_GET_HANDLE(handle), OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &config);
	if (omx_return != OMX_ErrorNone)
	{
		fprintf(stderr, "%s:%d: OMX_SetParameter() for video_encode port 201 failed with %x!\n", __FUNCTION__, __LINE__, omx_return);
		return false;
	}	
	
	// set current bitrate 
	OMX_VIDEO_PARAM_BITRATETYPE bitrateType;
	memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
	bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
	bitrateType.nVersion.nVersion = OMX_VERSION;
	bitrateType.eControlRate = OMX_Video_ControlRateVariable;
	bitrateType.nTargetBitrate = bitrate;
	bitrateType.nPortIndex = 201;

	omx_return = OMX_SetParameter(ILC_GET_HANDLE(handle), OMX_IndexParamVideoBitrate, &bitrateType);
	if (omx_return != OMX_ErrorNone)
	{
		fprintf(stderr, "%s:%d: OMX_SetParameter() for bitrate for video_encode port 201 failed with %x!\n", __FUNCTION__, __LINE__, omx_return);
		return false;
	}
   
	// set profile & level
        OMX_VIDEO_PARAM_PROFILELEVELTYPE profileLevel;
        memset(&profileLevel, 0, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        profileLevel.nSize = sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE);
        profileLevel.nVersion.nVersion = OMX_VERSION;
        profileLevel.nPortIndex = 201;
        profileLevel.eProfile = profile;
        profileLevel.eLevel = level;

        fprintf(stderr, "OMX_SetParameter(OMX_IndexParamVideoProfileLevelCurrent) for video_encode:201...\n");
        omx_return = OMX_SetParameter(ILC_GET_HANDLE(handle), OMX_IndexParamVideoProfileLevelCurrent, &profileLevel);
        if (omx_return != OMX_ErrorNone)
        {
                fprintf(stderr, "%s:%d: OMX_SetParameter() for video_encode port 201 failed with %x!\n", __FUNCTION__, __LINE__, omx_return);
                return false;
        }

	return true;
}

bool encode_config_activate(COMPONENT_T* handle)
{
	fprintf(stderr, "encode to idle...\n");
	if (ilclient_change_component_state(handle, OMX_StateIdle) == -1)
	{
		fprintf(stderr, "%s:%d: ilclient_change_component_state(video_encode, OMX_StateIdle) failed", __FUNCTION__, __LINE__);
		return false;
	}

	fprintf(stderr, "enabling port buffers for 200...\n");
	if (ilclient_enable_port_buffers(handle, 200, NULL, NULL, NULL) != 0)
	{
		fprintf(stderr, "enabling port buffers for 200 failed!\n");
		return false;
	}

	fprintf(stderr, "enabling port buffers for 201...\n");
	if (ilclient_enable_port_buffers(handle, 201, NULL, NULL, NULL) != 0)
	{
		fprintf(stderr, "enabling port buffers for 201 failed!\n");
		return false;
	}

	fprintf(stderr, "encode to executing...\n");
	ilclient_change_component_state(handle, OMX_StateExecuting);

	return true;
}

void encode_deactivate(COMPONENT_T* handle)
{
	fprintf(stderr, "disabling port buffers for 200\n");
	ilclient_disable_port_buffers(handle, 200, NULL, NULL, NULL);
	fprintf(stderr, "disabling port buffers for 201\n");
	ilclient_disable_port_buffers(handle, 201, NULL, NULL, NULL);
}

void encode_deinit(COMPONENT_T *handle, ILCLIENT_T *client)
{
	fprintf(stderr, "deinit..\n");
        COMPONENT_T *list[] = {handle , NULL};

	ilclient_state_transition(list, OMX_StateIdle);
	ilclient_state_transition(list, OMX_StateLoaded);

	ilclient_cleanup_components(list);

	OMX_Deinit();

	ilclient_destroy(client);
}

