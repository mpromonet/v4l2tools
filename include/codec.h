/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** codec.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "V4l2Output.h"
class Codec {
    public:
        Codec(int format, int width, int height): m_informat(format), m_width(width), m_height(height) {}
        virtual ~Codec() {}

        virtual void convertAndWrite(const char* buffer, unsigned int rsize, V4l2Output* videoOutput) = 0;

    protected:
        int m_informat;
    	int m_width;
		int m_height;
};

