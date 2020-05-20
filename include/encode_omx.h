/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** encode_omx.h
**
** Helper to encode using OMX
**
** -------------------------------------------------------------------------*/

#ifndef ENCODE_OMX_H
#define ENCODE_OMX_H

ILCLIENT_T * encode_init(COMPONENT_T **video_encode);
bool encode_config_input(COMPONENT_T* handle, int32_t width, int32_t height, int32_t framerate, OMX_COLOR_FORMATTYPE colorFormat);
bool encode_config_output(COMPONENT_T* handle, OMX_VIDEO_CODINGTYPE codec, uint32_t bitrate, OMX_VIDEO_AVCPROFILETYPE profile, OMX_VIDEO_AVCLEVELTYPE level);
bool encode_config_activate(COMPONENT_T* handle);
void encode_deactivate(COMPONENT_T* handle);
void encode_deinit(COMPONENT_T *handle, ILCLIENT_T *client);

OMX_VIDEO_AVCPROFILETYPE decodeProfile(const char* profile)
{
	OMX_VIDEO_AVCPROFILETYPE profileType = OMX_VIDEO_AVCProfileMax;
	if (strcmp(profile,"Baseline")==0) {
		profileType = OMX_VIDEO_AVCProfileBaseline;
	}
	else if (strcmp(profile,"Main")==0) {
		profileType = OMX_VIDEO_AVCProfileMain;
	}
	else if (strcmp(profile,"Extended")==0) {
		profileType = OMX_VIDEO_AVCProfileExtended;
	}
	else if (strcmp(profile,"High")==0) {
		profileType = OMX_VIDEO_AVCProfileHigh;
	}
	return profileType;
}

OMX_VIDEO_AVCLEVELTYPE decodeLevel(const char* level)
{
    OMX_VIDEO_AVCLEVELTYPE levelType = OMX_VIDEO_AVCLevel4;
    if (strcmp(level,"1")==0) {
		levelType = OMX_VIDEO_AVCLevel1;
	} else if (strcmp(level,"1b")==0) {
		levelType = OMX_VIDEO_AVCLevel1b;
	} else if (strcmp(level,"1.1")==0) {
		levelType = OMX_VIDEO_AVCLevel11;
	} else if (strcmp(level,"1.2")==0) {
		levelType = OMX_VIDEO_AVCLevel12;
	} else if (strcmp(level,"1.3")==0) {
		levelType = OMX_VIDEO_AVCLevel13;
	} else if (strcmp(level,"2")==0) {
		levelType = OMX_VIDEO_AVCLevel2;
	} else if (strcmp(level,"2.1")==0) {
		levelType = OMX_VIDEO_AVCLevel21;
	} else if (strcmp(level,"2.2")==0) {
		levelType = OMX_VIDEO_AVCLevel22;
	} else if (strcmp(level,"3")==0) {
		levelType = OMX_VIDEO_AVCLevel3;
	} else if (strcmp(level,"3.1")==0) {
		levelType = OMX_VIDEO_AVCLevel31;
	} else if (strcmp(level,"3.2")==0) {
		levelType = OMX_VIDEO_AVCLevel32;
	} else if (strcmp(level,"4")==0) {
		levelType = OMX_VIDEO_AVCLevel4;
	} else if (strcmp(level,"4.1")==0) {
		levelType = OMX_VIDEO_AVCLevel41;
	} else if (strcmp(level,"4.2")==0) {
		levelType = OMX_VIDEO_AVCLevel42;
	} else if (strcmp(level,"5")==0) {
		levelType = OMX_VIDEO_AVCLevel5;
	} else if (strcmp(level,"5.1")==0) {
		levelType = OMX_VIDEO_AVCLevel51;
	}
	return levelType;
}

#endif
