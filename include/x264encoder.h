/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** x264encoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <string>
#include <map>

#include "libyuv.h"
#include "logger.h"
#include "encoder.h"

class V4l2Output;
extern "C" 
{
	#include "x264.h"
}

class X264Encoder : public Encoder {
	public:
		X264Encoder(int format, int width, int height, const std::map<std::string,std::string> & opt, int verbose) 
			: m_encoder(NULL)
			, m_width(width)
			, m_height(height) {

			x264_param_t param;
			x264_param_default_preset(&param, "ultrafast", "zerolatency");
			if (verbose>1)
			{
				param.i_log_level = X264_LOG_DEBUG;
			}
			param.i_threads = 1;
			param.i_width = width;
			param.i_height = height;
			param.i_bframe = 0;
			param.b_repeat_headers = 1;

			std::map<std::string,std::string>::const_iterator keyint = opt.find("GOP");
			if (keyint != opt.end()) {
				int value = std::stoi(keyint->second);	
				param.i_keyint_min = value;
				param.i_keyint_max = value;						
			}

			std::map<std::string,std::string>::const_iterator rc_qcp = opt.find("RC_CQP");
			if (rc_qcp != opt.end()) {
				int rc_value = std::stoi(rc_qcp->second);
				param.rc.i_rc_method = X264_RC_CQP;
				param.rc.i_qp_constant = rc_value;
				param.rc.i_qp_min = rc_value; 
				param.rc.i_qp_max = rc_value;
			}
			std::map<std::string,std::string>::const_iterator rc_crf = opt.find("RC_CRF");
			if (rc_crf != opt.end()) {	
				int rc_value = std::stoi(rc_crf->second);		
				param.rc.i_rc_method = X264_RC_CRF;
				param.rc.f_rf_constant = rc_value;
				param.rc.f_rf_constant_max = rc_value;
			}


			LOG(WARN) << "rc_method:" << param.rc.i_rc_method; 
			LOG(WARN) << "i_qp_constant:" << param.rc.i_qp_constant; 
			LOG(WARN) << "f_rf_constant:" << param.rc.f_rf_constant; 
			
			x264_picture_init( &m_pic_in );
			x264_picture_alloc(&m_pic_in, X264_CSP_I420, width, height);
			
			m_encoder = x264_encoder_open(&param);
			if (!m_encoder)
			{
				LOG(WARN) << "Cannot create X264 encoder"; 
			}			
		}

		void convertEncodeWrite(const char* buffer, unsigned int rsize, int format, V4l2Output* videoOutput) {

				libyuv::ConvertToI420((const uint8*)buffer, rsize,
						m_pic_in.img.plane[0], m_width,
						m_pic_in.img.plane[1], m_width/2,
						m_pic_in.img.plane[2], m_width/2,
						0, 0,
						m_width, m_height,
						m_width, m_height,
						libyuv::kRotate0, format);

					x264_nal_t* nals = NULL;
					int i_nals = 0;
					x264_encoder_encode(m_encoder, &nals, &i_nals, &m_pic_in, &m_pic_out);
										
					if (i_nals > 1) {
						int size = 0;
						for (int i=0; i < i_nals; ++i) {
							size+=nals[i].i_payload;
						}
						char buffer[size];
						char* ptr = buffer;
						for (int i=0; i < i_nals; ++i) {
							memcpy(ptr, nals[i].p_payload, nals[i].i_payload);									
							ptr+=nals[i].i_payload;
						}
						
						int wsize = videoOutput->write(buffer,size);
						LOG(INFO) << "Copied nbnal:" << i_nals << " size:" << wsize; 					
						
					} else if (i_nals == 1) {
						int wsize = videoOutput->write((char*)nals[0].p_payload, nals[0].i_payload);
						LOG(INFO) << "Copied size:" << wsize; 					
					}				
		}			
						
		~X264Encoder() {
				x264_picture_clean(&m_pic_in);
				x264_encoder_close(m_encoder);
		}				

	private:
		x264_t* m_encoder;
		x264_picture_t m_pic_in;
		x264_picture_t m_pic_out;
		int m_width;
		int m_height;
};
