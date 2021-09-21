/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** jpegencoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "libyuv.h"
#include "logger.h"
#include "encoder.h"

#include <jpeglib.h>

class V4l2Output;

class JpegEncoder : public Encoder {
	public:
		JpegEncoder(int format, int width, int height, const std::map<std::string,std::string> & opt, int verbose) 
			: m_width(width)
			, m_height(height) {	

			jpeg_create_compress(&m_cinfo);
			m_cinfo.image_width = width;
			m_cinfo.image_height = height;
			m_cinfo.input_components = 3;	
			m_cinfo.in_color_space = JCS_YCbCr; 
			m_cinfo.err = jpeg_std_error(&m_jerr);

			jpeg_set_defaults(&m_cinfo);
			std::map<std::string,std::string>::const_iterator quality = opt.find("QUALITY");
			if (quality != opt.end()) {
				int value = std::stoi(quality->second);
				jpeg_set_quality(&m_cinfo, value, TRUE);
			}
			std::map<std::string,std::string>::const_iterator dri = opt.find("DRI");
			if (dri != opt.end()) {
				int value = std::stoi(dri->second);
				m_cinfo.restart_interval = value;
			}						

			m_i420buffer = new unsigned char [width*height*3/2];
		}

		void convertEncodeWrite(const char* buffer, unsigned int rsize, int format, V4l2Output* videoOutput) {
				unsigned char * buffer_y = m_i420buffer;
				unsigned char * buffer_u = buffer_y + m_width*m_height;
				unsigned char * buffer_v = buffer_u + m_width*m_height/4;

				libyuv::ConvertToI420((const uint8*)buffer, rsize,
						buffer_y, m_width,
						buffer_u, (m_width+1)/2,
						buffer_v, (m_width+1)/2,
						0, 0,
						m_width, m_height,
						m_width, m_height,
						libyuv::kRotate0, format);

				unsigned char* dest = NULL;
				unsigned long  destsize = 0;
				jpeg_mem_dest(&m_cinfo, &dest, &destsize);	

				jpeg_start_compress(&m_cinfo, TRUE);

				unsigned char bufline[m_cinfo.image_width *  m_cinfo.num_components]; 
				while (m_cinfo.next_scanline < m_cinfo.image_height) 
				{ 
					for (unsigned int i = 0; i < m_cinfo.image_width; ++i) 
					{ 
						bufline[i*3  ] = buffer_y[m_cinfo.next_scanline*m_cinfo.image_width + i]; 
						bufline[i*3+1] = buffer_u[(m_cinfo.next_scanline*m_cinfo.image_width/2 + i)/2]; 
						bufline[i*3+2] = buffer_v[(m_cinfo.next_scanline*m_cinfo.image_width/2 + i)/2];  
					} 
					JSAMPROW row = bufline; 
					jpeg_write_scanlines(&m_cinfo, &row, 1); 
				}
				jpeg_finish_compress(&m_cinfo);
						
                int wsize = videoOutput->write((char *)dest,destsize);
                LOG(DEBUG) << "Copied size:" << wsize;

				free(dest);										
		}			
						
		~JpegEncoder() {
				jpeg_destroy_compress(&m_cinfo);
				delete [] m_i420buffer;
		}				

	private:
		struct jpeg_error_mgr m_jerr;
		struct jpeg_compress_struct m_cinfo;	
		unsigned char * m_i420buffer;
		int m_width;
		int m_height;		

	public:
		static const bool registration;		
};

const bool JpegEncoder::registration = EncoderFactory::get().registerEncoder(V4L2_PIX_FMT_JPEG, EncoderCreator<JpegEncoder>::Create);

