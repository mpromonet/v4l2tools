/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** cudaencoder.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "encoderfactory.h"

#include <cuda.h>
#include "nvEncodeAPI.h"

inline bool check(int e, int iLine, const char *szFile) {
    if (e < 0) {
        LOG(ERROR) << "General error " << e << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}

#define ck(call) check(call, __LINE__, __FILE__)

class CudaEncoder : public Encoder {
    public:
        CudaEncoder(int outformat, int informat, int width, int height, const std::map<std::string,std::string> & opt, int verbose)
            : Encoder(informat, width, height) {
            ck(cuInit(0));
            int nGpu = 0;
            ck(cuDeviceGetCount(&nGpu));  
            std::cout << "Nb GPU: " << nGpu << std::endl;          

            int iGpu = 0;
            CUdevice cuDevice = 0;
            ck(cuDeviceGet(&cuDevice, iGpu));
            char szDeviceName[80];
            ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
            std::cout << "GPU in use: " << szDeviceName << std::endl;
            ck(cuCtxCreate(&m_cuContext, 0, cuDevice));
            ck(cuCtxPopCurrent(&m_cuContext));

            // create api
            uint32_t version = 0;
            ck(NvEncodeAPIGetMaxSupportedVersion(&version));
            uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
            if (currentVersion > version)
            {
                std::cout << "Current Driver Version does not support this NvEncodeAPI version, please upgrade driver:" << NV_ENC_ERR_INVALID_VERSION << std::endl;
            }
            m_nvenc = { NV_ENCODE_API_FUNCTION_LIST_VER };
            ck(NvEncodeAPICreateInstance(&m_nvenc));

            // create encoder
            NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS encodeSessionExParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
            encodeSessionExParams.device = m_cuContext;
            encodeSessionExParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
            encodeSessionExParams.apiVersion = NVENCAPI_VERSION;
            ck(m_nvenc.nvEncOpenEncodeSessionEx(&encodeSessionExParams, &m_hEncoder));

            // init encoder
            NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
            NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
            initializeParams.encodeConfig = &encodeConfig;
            initializeParams.encodeGUID        = NV_ENC_CODEC_H264_GUID;
            initializeParams.presetGUID        = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
            initializeParams.encodeWidth       = m_width;
            initializeParams.encodeHeight      = m_height;

            NV_ENC_PRESET_CONFIG presetcfg = { NV_ENC_PRESET_CONFIG_VER };
            presetcfg.presetCfg.version = NV_ENC_CONFIG_VER;
            ck(m_nvenc.nvEncGetEncodePresetConfig(m_hEncoder, initializeParams.encodeGUID, initializeParams.presetGUID, &presetcfg));
            memcpy(&encodeConfig, &presetcfg, sizeof(NV_ENC_CONFIG));

            ck(m_nvenc.nvEncInitializeEncoder(m_hEncoder, &initializeParams));

            // create inputbuffer
            m_inputBuffer.version    = NV_ENC_CREATE_INPUT_BUFFER_VER;
            m_inputBuffer.width      = m_width;
            m_inputBuffer.height     = m_height;
            m_inputBuffer.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
            m_inputBuffer.bufferFmt  = NV_ENC_BUFFER_FORMAT_IYUV;
            ck(m_nvenc.nvEncCreateInputBuffer(m_hEncoder,&m_inputBuffer));

            // create outputbuffer
            m_outputBuffer.version    = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
            m_outputBuffer.size       = 2 * 1024 * 1024;  // No idea why
            m_outputBuffer.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
            ck(m_nvenc.nvEncCreateBitstreamBuffer(m_hEncoder, &m_outputBuffer));
        }

        virtual ~CudaEncoder() {
            m_nvenc.nvEncDestroyEncoder(m_hEncoder);
            cuCtxDestroy(m_cuContext);
        }

        virtual void convertEncodeWrite(const char* buffer, unsigned int rsize, V4l2Output* videoOutput) {

            // fill inputbuffer
            NV_ENC_LOCK_INPUT_BUFFER inputbufferlocker = { NV_ENC_LOCK_INPUT_BUFFER_VER };
            inputbufferlocker.inputBuffer = m_inputBuffer.inputBuffer;
            ck(m_nvenc.nvEncLockInputBuffer(m_hEncoder, &inputbufferlocker));
            memcpy((void*)inputbufferlocker.bufferDataPtr, buffer, rsize);
            ck(m_nvenc.nvEncUnlockInputBuffer(m_hEncoder, &inputbufferlocker));

            // encode
            NV_ENC_PIC_PARAMS picParams = {};
            picParams.version = NV_ENC_PIC_PARAMS_VER;
            picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
            picParams.inputBuffer = m_inputBuffer.inputBuffer;
            picParams.outputBitstream = m_outputBuffer.bitstreamBuffer;
            NVENCSTATUS nvStatus = m_nvenc.nvEncEncodePicture(m_hEncoder, &picParams);

            // retrieve encoded data from outputbuffer
            NV_ENC_LOCK_BITSTREAM outputbufferlocker = { NV_ENC_LOCK_BITSTREAM_VER };
            outputbufferlocker.outputBitstream = m_outputBuffer.bitstreamBuffer;
            m_nvenc.nvEncLockBitstream(m_hEncoder, &outputbufferlocker);
            int wsize = videoOutput->write((char*)outputbufferlocker.bitstreamBufferPtr, outputbufferlocker.bitstreamSizeInBytes);
            LOG(DEBUG) << "Copied " << rsize << " " << wsize;           
            m_nvenc.nvEncUnlockBitstream(m_hEncoder, &outputbufferlocker);

        }

    private:
        CUcontext m_cuContext;
        NV_ENCODE_API_FUNCTION_LIST m_nvenc;
        void *m_hEncoder = nullptr;
        NV_ENC_CREATE_INPUT_BUFFER m_inputBuffer;
        NV_ENC_CREATE_BITSTREAM_BUFFER m_outputBuffer;

	public:
		static const bool registration;        
};

const bool CudaEncoder::registration = EncoderFactory::get().registerEncoder(V4L2_PIX_FMT_H264, EncoderCreator<CudaEncoder>::Create);
