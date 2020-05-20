/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** x265encoder.h
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
	#include "x265.h"
}

class X265Encoder : public Encoder {
	public:
		X265Encoder(int width, int height, int fps, const std::map<std::string,std::string> & opt, int verbose) 
            : m_encoder(NULL), m_pic_in(NULL), m_pic_out(NULL), m_buff(NULL)
            , m_width(width)
            , m_height(height) {

			x265_param param;
			x265_param_default_preset(&param, "ultrafast", "zerolatency");
			if (verbose>1)
			{
				param.logLevel = X265_LOG_DEBUG;
			}
			param.sourceWidth = width;
			param.sourceHeight = height;
			param.fpsNum = fps;
			param.fpsDenom = 1;
			param.bframes = 0;
			param.bRepeatHeaders = 1;
			param.keyframeMin = fps;
			param.keyframeMax = fps;						
			param.bOpenGOP = 0;

			std::map<std::string,std::string>::const_iterator rc_qcp = opt.find("RC_CQP");
			if (rc_qcp != opt.end()) {
				int rc_value = std::stoi(rc_qcp->second);
				param.rc.rateControlMode = X265_RC_CQP;
				param.rc.qp = rc_value;
			}			
			std::map<std::string,std::string>::const_iterator rc_crf = opt.find("RC_CRF");
			if (rc_crf != opt.end()) {	
				int rc_value = std::stoi(rc_crf->second);		
				param.rc.rateControlMode = X265_RC_CRF;
				param.rc.rfConstantMin = rc_value;
				param.rc.rfConstantMax = rc_value;
			}
			
            m_pic_in = x265_picture_alloc();
            x265_picture_init(&param, m_pic_in);
            m_buff= new char[width*height*3/2];
            m_pic_in->planes[0]=m_buff;
            m_pic_in->planes[1]=m_buff+width*height;
            m_pic_in->planes[2]=m_buff+width*height*5/4;
            
            m_pic_out = x265_picture_alloc();

			m_encoder = x265_encoder_open(&param);
			if (!m_encoder)
			{
				LOG(WARN) << "Cannot create X265 encoder"; 
			}
		}

		void convertEncodeWrite(const char* buffer, unsigned int rsize, int format, V4l2Output* videoOutput) {

				libyuv::ConvertToI420((const uint8*)buffer, rsize,
							(uint8*)m_pic_in->planes[0], m_width,
							(uint8*)m_pic_in->planes[1], m_width/2,
							(uint8*)m_pic_in->planes[2], m_width/2,
							0, 0,
							m_width, m_height,
							m_width, m_height,
							libyuv::kRotate0, format);

					x265_nal* nals = NULL;
					uint32_t i_nals = 0;
                    if (x265_encoder_encode(m_encoder, &nals, &i_nals, m_pic_in, m_pic_out) > 0) {
                        if (i_nals > 1) {
                            int size = 0;
                            for (int i=0; i < i_nals; ++i) {
                                size+=nals[i].sizeBytes;
                            }
                            char buffer[size];
                            char* ptr = buffer;
                            for (int i=0; i < i_nals; ++i) {
                                memcpy(ptr, nals[i].payload, nals[i].sizeBytes);	
                                ptr+=nals[i].sizeBytes;
                            }
                            
                            int wsize = videoOutput->write(buffer,size);
                            LOG(INFO) << "Copied nbnal:" << i_nals << " size:" << wsize; 					
                            
                        } else if (i_nals == 1) {
                            int wsize = videoOutput->write((char*)nals[0].payload, nals[0].sizeBytes);
                            LOG(INFO) << "Copied size:" << wsize; 					
                        }				
                    } else {
                        LOG(NOTICE) << "encoder error"; 
                    }
		}			
						
		~X265Encoder() {
                delete [] m_buff;
				x265_picture_free(m_pic_in);
				x265_picture_free(m_pic_out);
				x265_encoder_close(m_encoder);
		}				

	private:
		x265_encoder* m_encoder;
		x265_picture* m_pic_in;
		x265_picture* m_pic_out;
        char* m_buff;
		int m_width;
		int m_height;
};
