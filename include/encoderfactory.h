
#pragma once

#ifdef HAVE_X264   
#include "x264encoder.h"
#endif
#ifdef HAVE_X265
#include "x265encoder.h"
#endif
#ifdef HAVE_VPX   
#include "vpxencoder.h"
#endif

class EncoderFactory {
    public:
    static Encoder* Create(int format, int width, int height, int fps, const std::map<std::string,std::string> & opt, int verbose) {
        Encoder* encoder = NULL;
        switch (format) {
#ifdef HAVE_X265         
            case V4L2_PIX_FMT_HEVC: encoder = new X265Encoder(width, height, fps,  opt, verbose); break;
#endif            
#ifdef HAVE_X264           
            case V4L2_PIX_FMT_H264: encoder = new X264Encoder(width, height, fps,  opt, verbose); break;
#endif            
#ifdef HAVE_VPX            
            case V4L2_PIX_FMT_VP8: encoder = new VpxEncoder(width, height, format,  opt, verbose); break;
            case V4L2_PIX_FMT_VP9: encoder = new VpxEncoder(width, height, format,  opt, verbose); break;
#endif            
        }
        return encoder;
    }
};