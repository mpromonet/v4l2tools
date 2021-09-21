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

typedef Encoder* (*encoderCreator)(int format, int width, int height, const std::map<std::string,std::string> & opt, int verbose);

template<typename T> class EncoderCreator {
        public:
                static Encoder* Create(int format, int width, int height, const std::map<std::string,std::string> & opt, int verbose) {
                        return new T(format, width, height, opt, verbose);
                }

                static const bool registration;
};

class EncoderFactory {
    public:
        Encoder* Create(int format, int width, int height, const std::map<std::string,std::string> & opt, int verbose) {
                Encoder* encoder = NULL;
                auto it = m_registry.find(format);
                if (it != std::end(m_registry)) {
                        encoder = it->second(format, width, height, opt, verbose);
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
