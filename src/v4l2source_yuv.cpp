/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2source_yuv.cpp
** 
** Generate YUV frames and write to  V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

int stop=0;


int getFrame(char buffer[], int bufSize, int width, int height, int i)
{
    for(int y=0;y<height;y++) {
        for(int x=0;x<width;x++) {
            /* Y */
            buffer[ (y * width + x)*2 ] = x + y + i * 3;
	    if ( (y * width + x) % 2 == 0) {	
		/* U */
		buffer[ (y * width + x)*2+1] = 128 + y + i * 2;
	    } else {
		/* V */
		buffer[ (y * width + x)*2+1] = 64 + x + i * 5;
	    }
        }
    }
    return bufSize;
}
/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *out_devname = "/dev/video1";	
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
    	int width = 640;
    	int height = 480;
	int fps = 25;
	
	int c = 0;
	while ((c = getopt (argc, argv, "hP:F:v::w" "WHF")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose   = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'w':	ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
			
			case 'W':	width = atoi(optarg); break;
			case 'H':	height = atoi(optarg); break;
			case 'F':	fps = atoi(optarg); break;			
			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				std::cout << "\t -W width      : V4L2 capture width (default "<< width << ")" << std::endl;
				std::cout << "\t -H height     : V4L2 capture height (default "<< height << ")" << std::endl;
				std::cout << "\t -F fps        : V4L2 capture framerate (default "<< fps << ")" << std::endl;				
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t dest_device   : V4L2 capture device (default "<< out_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		out_devname = argv[optind];
		optind++;
	}	

	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 output interface
	V4L2DeviceParameters outparam(out_devname, V4L2_PIX_FMT_YUYV, width, height, fps,verbose);
	V4l2Output* videoOutput = V4l2Output::create(outparam, ioTypeOut);
	if (videoOutput == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 output interface for device:" << out_devname; 
	}
	else
	{		
		LOG(NOTICE) << "Start generating frames to " << out_devname; 
		signal(SIGINT,sighandler);				
		int i=0;
		int picture_size = videoOutput->getBufferSize();
		char buffer[picture_size]; 
		
		while (!stop) 
		{
			int rsize = getFrame(buffer, sizeof(buffer), width, height, i++);
			if (rsize == -1)
			{
				LOG(NOTICE) << "stop " << strerror(errno); 
				stop=1;					
			}
			else
			{
				int wsize = videoOutput->write(buffer, rsize);
				LOG(DEBUG) << "Copied " << rsize << " " << wsize; 
				usleep(1000000/fps);
			}
		}
		delete videoOutput;
	}
	
	return 0;
}
