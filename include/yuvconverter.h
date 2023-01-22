/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** yuvconverter.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "libyuv.h"
#include "codecfactory.h"

class YuvConverter : public Codec
{
public:
        YuvConverter(int outformat, int informat, int width, int height, const std::map<std::string, std::string> &opt, int verbose)
            : Codec(informat, width, height), m_outformat(outformat)
        {
                m_i420 = new uint8_t[width * height * 2];
        }

        ~YuvConverter()
        {
                delete[] m_i420;
        }

        void convertAndWrite(const char *buffer, unsigned int rsize, V4l2Output *videoOutput)
        {
                uint8_t *i420_p0 = m_i420;
                uint8_t *i420_p1 = i420_p0 + m_width * m_height;
                uint8_t *i420_p2 = i420_p1 + m_width * m_height / 2;

                libyuv::ConvertToI420((const uint8_t *)buffer, rsize,
                                      i420_p0, m_width,
                                      i420_p1, (m_width + 1) / 2,
                                      i420_p2, (m_width + 1) / 2,
                                      0, 0,
                                      m_width, m_height,
                                      m_width, m_height,
                                      libyuv::kRotate0, m_informat);

                char outBuffer[videoOutput->getBufferSize()];
                libyuv::ConvertFromI420(i420_p0, m_width,
                                        i420_p1, (m_width + 1) / 2,
                                        i420_p2, (m_width + 1) / 2,
                                        (uint8_t *)outBuffer, 0,
                                        m_width, m_height,
                                        m_outformat);

                int wsize = videoOutput->write(outBuffer, sizeof(outBuffer));
                LOG(DEBUG) << "Copied " << rsize << " " << wsize;
        }

private:
        uint8_t *m_i420;
        int m_outformat;

public:
        static const bool registration;
};

const bool YuvConverter::registration = CodecFactory::get().registerEncoder(0, CodecCreator<YuvConverter>::Create);
