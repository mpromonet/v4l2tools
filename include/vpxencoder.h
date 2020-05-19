/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** vpxencoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "libyuv.h"
#include "logger.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "encoder.h"

class V4l2Output;

class VpxEncoder : public Encoder {
	public:
		VpxEncoder(int width, int height, int format, const std::map<std::string,std::string> & opt, int verbose) 
			: m_width(width)
			, m_height(height)
            , m_frame_cnt(0) {


			if(!vpx_img_alloc(&m_input, VPX_IMG_FMT_I420, width, height, 1))
			{
				LOG(WARN) << "vpx_img_alloc"; 
			}

			const vpx_codec_iface_t* algo = getAlgo(format);
			vpx_codec_enc_cfg_t  cfg;
			if (vpx_codec_enc_config_default(algo, &cfg, 0) != VPX_CODEC_OK)
			{
				LOG(WARN) << "vpx_codec_enc_config_default"; 
			}

			cfg.g_w = width;
			cfg.g_h = height;	

			std::map<std::string,std::string>::iterator cbr = opt.find("CBR");
			if (cbr != opt.end()) {
                cfg.rc_end_usage = VPX_CBR;
                cfg.rc_target_bitrate = std::stoi(cbr->second);
            }            
			std::map<std::string,std::string>::iterator vbr = opt.find("VBR");
			if (cbr != opt.end()) {
                cfg.rc_end_usage = VPX_VBR;
                cfg.rc_target_bitrate = std::stoi(vbr->second);
            }   			
			
			if(vpx_codec_enc_init(&m_codec, algo, &cfg, 0))    
			{
				LOG(WARN) << "vpx_codec_enc_init"; 
			}
		}

        const vpx_codec_iface_t* getAlgo(int format)
        {
            const vpx_codec_iface_t* algo = NULL;
            switch (format)
            {
                case V4L2_PIX_FMT_VP8 : algo = vpx_codec_vp8_cx(); break;
                case V4L2_PIX_FMT_VP9 : algo = vpx_codec_vp9_cx(); break;
            }
            return algo;
        }        

		void convertEncodeWrite(const char* buffer, unsigned int rsize, int format, V4l2Output* videoOutput) {

                libyuv::ConvertToI420((const uint8*)buffer, rsize,
                    m_input.planes[0], m_width,
                    m_input.planes[1], m_width/2,
                    m_input.planes[2], m_width/2,
                    0, 0,
                    m_width, m_height,
                    m_width, m_height,
                    libyuv::kRotate0, format);

                int flags=0;          
                if(vpx_codec_encode(&m_codec, &m_input, m_frame_cnt++ , 1, flags, VPX_DL_REALTIME))    
                {					
                    LOG(WARN) << "vpx_codec_encode: " << vpx_codec_error(&m_codec) << "(" << vpx_codec_error_detail(&m_codec) << ")";
                }
                
                vpx_codec_iter_t iter = NULL;
                const vpx_codec_cx_pkt_t *pkt;
                while( (pkt = vpx_codec_get_cx_data(&m_codec, &iter)) ) 
                {
                    if (pkt->kind==VPX_CODEC_CX_FRAME_PKT)
                    {
                        int wsize = videoOutput->write((char*)pkt->data.frame.buf, pkt->data.frame.sz);
                        LOG(DEBUG) << "Copied " << rsize << " " << wsize; 
                    }
                    else
                    {
                        break;
                    }
                }
		}			
						
		~VpxEncoder() {
            vpx_img_free(&m_input);
		}				

	private:
		vpx_codec_ctx_t m_codec;
        vpx_image_t     m_input;
		int m_width;
		int m_height;
        int m_frame_cnt;
};