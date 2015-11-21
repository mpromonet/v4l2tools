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
bool encode_config_output(COMPONENT_T* handle, OMX_VIDEO_CODINGTYPE codec, uint32_t bitrate);
bool encode_config_activate(COMPONENT_T* handle);
void encode_deactivate(COMPONENT_T* handle);
void encode_deinit(COMPONENT_T *handle, ILCLIENT_T *client);

#endif
