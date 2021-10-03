/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** codecfacotry.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <map>

#include "codec.h"

typedef Codec* (*codecCreator)(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose);

template<typename T> class CodecCreator {
        public:
                static Codec* Create(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose) {
                        return new T(outformat, informat, width, height, opt, verbose);
                }
};

class CodecFactory {
    public:
        Codec* Create(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose) {
                Codec* convertor = NULL;
                auto itDecoder = m_registryDecoder.find(informat);
                if (itDecoder != std::end(m_registryEncoder)) {
                        convertor = itDecoder->second(outformat, informat, width, height, opt, verbose);
                } else {
                        auto itEncoder = m_registryEncoder.find(outformat);
                        if (itEncoder == std::end(m_registryEncoder)) {
                                itEncoder = m_registryEncoder.find(0);
                        }
                        if (itEncoder != std::end(m_registryEncoder)) {
                                convertor = itEncoder->second(outformat, informat, width, height, opt, verbose);
                        }
                }
                return convertor;
        }

        std::list<int> SupportedFormat() {
                std::list<int> formatList;
                for (auto it : m_registryEncoder) {
                        formatList.push_back(it.first);
                }
                return formatList;      
        }

        static CodecFactory & get() {
                static CodecFactory instance;
                return instance;
        }

        bool registerEncoder(int format, codecCreator creator) {
                m_registryEncoder[format] = creator;
                return true;
        }

        bool registerDecoder(int format, codecCreator creator) {
                m_registryDecoder[format] = creator;
                return true;
        }
    private:
        std::map<int, codecCreator> m_registryEncoder;
        std::map<int, codecCreator> m_registryDecoder;
};
