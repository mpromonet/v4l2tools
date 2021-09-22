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

            // create api
            m_nvenc = { NV_ENCODE_API_FUNCTION_LIST_VER };
            NvEncodeAPICreateInstance(&m_nvenc);

            // create encoder
            NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS encodeSessionExParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
            encodeSessionExParams.device = m_cuContext;
            encodeSessionExParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
            encodeSessionExParams.apiVersion = NVENCAPI_VERSION;
            m_nvenc.nvEncOpenEncodeSessionEx(&encodeSessionExParams, &m_hEncoder);

            // init encoder
            NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
            NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
            initializeParams.encodeConfig = &encodeConfig;
            m_nvenc.nvEncInitializeEncoder(m_hEncoder, &initializeParams);
        }

        virtual ~CudaEncoder() {
            m_nvenc.nvEncDestroyEncoder(m_hEncoder);
            cuCtxDestroy(m_cuContext);
        }

        virtual void convertEncodeWrite(const char* buffer, unsigned int rsize, V4l2Output* videoOutput) {

            cuCtxPushCurrent(m_cuContext);
            /*
            CUDA_MEMCPY2D m = { 0 };
            m.srcMemoryType = CU_MEMORYTYPE_HOST;
            m.srcHost = buffer;
            m.srcPitch = srcPitch;
            m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
            m.dstDevice = pDstFrame;
            m.dstPitch = dstPitch;
            m.WidthInBytes = NvEncoder::GetWidthInBytes(pixelFormat, width);
            m.Height = m_height;
            cuMemcpy2D(&m);
            cuCtxPopCurrent(NULL);

            NV_ENC_PIC_PARAMS picParams = {};
            picParams.version = NV_ENC_PIC_PARAMS_VER;
            picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
            picParams.inputBuffer = inputBuffer;
            picParams.bufferFmt = GetPixelFormat();
            picParams.inputWidth = GetEncodeWidth();
            picParams.inputHeight = GetEncodeHeight();
            picParams.outputBitstream = outputBuffer;
            NVENCSTATUS nvStatus = m_nvenc.nvEncEncodePicture(m_hEncoder, &picParams);
            */
        }

    private:
        CUcontext m_cuContext;
        NV_ENCODE_API_FUNCTION_LIST m_nvenc;
        void *m_hEncoder = nullptr;

	public:
		static const bool registration;        
};

const bool CudaEncoder::registration = EncoderFactory::get().registerEncoder(V4L2_PIX_FMT_H264, EncoderCreator<CudaEncoder>::Create);
