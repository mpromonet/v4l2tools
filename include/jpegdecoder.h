/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** jpegdecoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "libyuv.h"
#include "logger.h"
#include "codecfactory.h"

#include <jpeglib.h>

class V4l2Output;

class JpegDecoder : public Codec {
	public:
		JpegDecoder(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose) 
			: Codec(informat, width, height), m_outformat(outformat) {	

			jpeg_create_decompress(&m_cinfo);
			m_cinfo.err = jpeg_std_error(&m_jerr);

			m_i420buffer = new unsigned char [width*height*3/2];
		}

		void convertAndWrite(const char* buffer, unsigned int rsize, V4l2Output* videoOutput) {
                uint8 *i420_p0 = m_i420buffer;
                uint8 *i420_p1 = i420_p0 + m_width * m_height;
                uint8 *i420_p2 = i420_p1 + m_width * m_height / 2;

				jpeg_mem_src(&m_cinfo, (unsigned char*)buffer, rsize);	
				jpeg_read_header(&m_cinfo, TRUE);
				LOG(INFO) << "width:" << m_cinfo.image_width << " height:" << m_cinfo.image_height << " num_components:" << m_cinfo.num_components; 
				m_cinfo.out_color_space = JCS_YCbCr;

				jpeg_start_decompress(&m_cinfo);

				unsigned char bufline[m_cinfo.image_width *  m_cinfo.num_components]; 
				while (m_cinfo.output_scanline < m_cinfo.output_height) 
				{ 
					JSAMPROW row = bufline; 
					jpeg_read_scanlines(&m_cinfo, &row, 1);
					for (unsigned int i = 0; i < m_cinfo.image_width; ++i) 
					{ 
						i420_p0[m_cinfo.output_scanline*m_cinfo.image_width + i]       = bufline[i*3  ]; 
						i420_p1[(m_cinfo.output_scanline*m_cinfo.image_width/2 + i)/2] = bufline[i*3+1]; 
						i420_p2[(m_cinfo.output_scanline*m_cinfo.image_width/2 + i)/2] = bufline[i*3+2];  
					} 
				}
				jpeg_finish_decompress(&m_cinfo);

                char outBuffer[videoOutput->getBufferSize()];
                libyuv::ConvertFromI420(i420_p0, m_width,
                                        i420_p1, (m_width + 1) / 2,
                                        i420_p2, (m_width + 1) / 2,
                                        (uint8 *)outBuffer, 0,
                                        m_width, m_height,
                                        m_outformat);

                int wsize = videoOutput->write((char *)outBuffer,videoOutput->getBufferSize());
                LOG(DEBUG) << "Copied size:" << wsize;

		}			
						
		~JpegDecoder() {
				jpeg_destroy_decompress(&m_cinfo);
				delete [] m_i420buffer;
		}				

	private:
		struct jpeg_error_mgr 			m_jerr;
		struct jpeg_decompress_struct 	m_cinfo;	
		unsigned char * 				m_i420buffer;
		int 							m_outformat;

	public:
		static const bool 				registration;		
};

const bool JpegDecoder::registration = CodecFactory::get().registerDecoder(V4L2_PIX_FMT_JPEG, CodecCreator<JpegDecoder>::Create);

