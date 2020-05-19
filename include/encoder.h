

#pragma once

class Encoder {
    public:
        virtual ~Encoder() {}

        virtual void convertEncodeWrite(const char* buffer, unsigned int rsize, int format, V4l2Output* videoOutput) = 0;
};

