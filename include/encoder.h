/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** encoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "V4l2Output.h"
class Encoder {
    public:
        Encoder(int width, int height): m_width(width), m_height(height) {}
        virtual ~Encoder() {}

        virtual void convertEncodeWrite(const char* buffer, unsigned int rsize, int format, V4l2Output* videoOutput) = 0;

    protected:
    	int m_width;
		int m_height;
};

