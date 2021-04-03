/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress_jpeg.cpp
** 
** Read YUYC from a V4L2 capture -> compress in JPEG using libjpeg -> write to a V4L2 output device
**
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include <jpeglib.h>

int stop=0;

/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

/* ---------------------------------------------------------------------------
**  convert yuyv -> jpeg
** -------------------------------------------------------------------------*/
void jpeg2yuyv(unsigned char* jpegBuffer, unsigned int jpegSize, unsigned char *  & image_buffer, unsigned int & image_size)
{
	struct jpeg_error_mgr jerr;
	struct jpeg_decompress_struct cinfo;	
	jpeg_create_decompress(&cinfo);
	cinfo.err = jpeg_std_error(&jerr);
	
	jpeg_mem_src(&cinfo, jpegBuffer, jpegSize);	
	jpeg_read_header(&cinfo, TRUE);
	LOG(INFO) << "width:" << cinfo.image_width << " height:" << cinfo.image_height << " num_components:" << cinfo.num_components; 
	
	jpeg_start_decompress(&cinfo);
	
	image_size = cinfo.image_width * cinfo.image_height *  2;
	image_buffer = (unsigned char*) malloc(image_size);	
	
	unsigned char bufline[cinfo.image_width * cinfo.num_components]; 
	while (cinfo.output_scanline < cinfo.output_height) {
		int rowIndex = cinfo.output_scanline ;
		
		JSAMPROW row = bufline; 
		jpeg_read_scanlines(&cinfo, &row, 1);

		// convert line from YUV -> YUYV 
		unsigned int base = rowIndex*cinfo.image_width * 2;
		for (unsigned int i = 0; i < cinfo.image_width; i += 2) 
		{ 			
			image_buffer[base + i*2    ] = bufline[i*3 ]; 
			image_buffer[base + i*2+1] = (bufline[i*3+1] + bufline[i*3+4])/2; 
			image_buffer[base + i*2+2] = bufline[i*3+3];  
			image_buffer[base + i*2+3] = (bufline[i*3+2] + bufline[i*3+5])/2; 			
		} 
	}	
	
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *in_devname = "/dev/video0";	
	const char *out_devname = "/dev/video1";	
	int width = 640;
	int height = 480;	
	int fps = 25;	
	V4l2IoType ioTypeIn  = IOTYPE_MMAP;
	V4l2IoType ioTypeOut = IOTYPE_MMAP;
	
	int c = 0;
	while ((c = getopt (argc, argv, "h" "W:H:F:" "rw" )) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			
			// capture options
			case 'W':	width = atoi(optarg); break;
			case 'H':	height = atoi(optarg); break;
			case 'F':	fps = atoi(optarg); break;
			case 'r':	ioTypeIn  = IOTYPE_READWRITE; break;			
			
			// output options
			case 'w':	ioTypeOut = IOTYPE_READWRITE; break;	
			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v               : verbose " << std::endl;
				std::cout << "\t -vv              : very verbose " << std::endl;
				std::cout << "\t -W width         : V4L2 capture width (default "<< width << ")" << std::endl;
				std::cout << "\t -H height        : V4L2 capture height (default "<< height << ")" << std::endl;
				std::cout << "\t -F fps           : V4L2 capture framerate (default "<< fps << ")" << std::endl;
				std::cout << "\t -r               : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w               : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				
				std::cout << "\tcompressor options" << std::endl;
				std::cout << "\t -q <quality>     : JPEG quality" << std::endl;
				
				std::cout << "\t source_device    : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				std::cout << "\t dest_device      : V4L2 output device (default "<< out_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		in_devname = argv[optind];
		optind++;
	}	
	if (optind<argc)
	{
		out_devname = argv[optind];
		optind++;
	}	
		
	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname,V4L2_PIX_FMT_JPEG,width,height,fps,ioTypeIn,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		// init V4L2 output interface
		V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_YUYV, videoCapture->getWidth(), videoCapture->getHeight(), 0, ioTypeOut, verbose);
		V4l2Output* videoOutput = V4l2Output::create(outparam);
		if (videoOutput == NULL)
		{	
			LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
		}
		else
		{		
			timeval tv;
			
			LOG(NOTICE) << "Start Uncompressing " << in_devname << " to " << out_devname; 					
			signal(SIGINT,sighandler);
			while (!stop) 
			{
				tv.tv_sec=1;
				tv.tv_usec=0;
				int ret = videoCapture->isReadable(&tv);
				if (ret == 1)
				{
					char buffer[videoCapture->getBufferSize()];
					int rsize = videoCapture->read(buffer, sizeof(buffer));
					if (rsize == -1)
					{
						LOG(NOTICE) << "stop " << strerror(errno); 
						stop=1;					
					}
					else
					{												
						// compress
						unsigned char * outBuffer = NULL;
						unsigned int outSize = 0;
						jpeg2yuyv((unsigned char *)buffer, rsize, outBuffer, outSize);							

						if (outBuffer) {
							int wsize = videoOutput->write((char*)outBuffer, outSize);
							LOG(DEBUG) << "Copied " << rsize << " " << wsize; 
							
							free(outBuffer);
						}
					}
				}
				else if (ret == -1)
				{
					LOG(NOTICE) << "stop " << strerror(errno); 
					stop=1;
				}
			}
			delete videoOutput;
		}
		
		delete videoCapture;
	}
	
	return 0;
}
