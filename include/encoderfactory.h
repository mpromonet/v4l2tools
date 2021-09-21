/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** encoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <map>

#include "encoder.h"

typedef Encoder* (*encoderCreator)(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose);

template<typename T> class EncoderCreator {
        public:
                static Encoder* Create(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose) {
                        return new T(outformat, informat, width, height, opt, verbose);
                }
};

class EncoderFactory {
    public:
        Encoder* Create(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose) {
                Encoder* encoder = NULL;
                auto it = m_registry.find(outformat);
                if (it == std::end(m_registry)) {
                        it = m_registry.find(0);
                }
                if (it != std::end(m_registry)) {
                        encoder = it->second(outformat, informat, width, height, opt, verbose);
                }
                return encoder;
        }

        std::list<int> SupportedFormat() {
                std::list<int> formatList;
                for (auto it : m_registry) {
                        formatList.push_back(it.first);
                }
                return formatList;      
        }

        static EncoderFactory & get() {
                static EncoderFactory instance;
                return instance;
        }

        bool registerEncoder(int format, encoderCreator creator) {
                m_registry[format] = creator;
                return true;
        }

    private:
        std::map<int, encoderCreator> m_registry;
};
