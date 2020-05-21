/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress_main.cpp
** 
** Read YUYV from a V4L2 capture -> compress in VP8 -> write to a V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <signal.h>

#include <iostream>
#include <map>

#include "V4l2Access.h"

extern int compress(const std::string& in_devname, V4l2Access::IoType ioTypeIn, const std::string& out_devname, V4l2Access::IoType ioTypeOut, int outformat, const std::map<std::string,std::string>& opt, int & stop, int verbose);

/* ---------------------------------------------------------------------------
**  end condition
** -------------------------------------------------------------------------*/
int stop=0;

/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

// -----------------------------------------
//    convert string format to fourcc 
// -----------------------------------------
int decodeFormat(const char* fmt)
{
	char fourcc[4];
	memset(&fourcc, 0, sizeof(fourcc));
	if (fmt != NULL)
	{
		strncpy(fourcc, fmt, 4);	
	}
	return v4l2_fourcc(fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *in_devname = "/dev/video0";	
	const char *out_devname = "/dev/video1";	
	int c = 0;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
	V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
	std::map<std::string,std::string> opt;
	opt["VBR"] = "1000";
	std::string strformat = "VP80";
	opt["GOP"] = "25";
	
	while ((c = getopt (argc, argv, "hv::rw" "f:" "C:V:Q:F:G:q:d:")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			
			case 'f':	strformat      = optarg; break;

			// parameters for VPx/H26x
			case 'G':	opt["GOP"] = optarg; break;
			case 'C':	opt["CBR"] = optarg; break;	
			case 'V':	opt["VBR"] = optarg; break;	
			case 'Q':	opt["RC_CQP"] = optarg; break;	
			case 'F':	opt["RC_CRF"] = optarg; break;				

			// parameters for JPEG
			case 'q':	opt["QUALITY"] = optarg; break;
			case 'd':	opt["DRI"] = optarg; break;	
			
			case 'r':	ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			case 'w':	ioTypeOut = V4l2Access::IOTYPE_READWRITE; break;	
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v                   : verbose " << std::endl;
				std::cout << "\t -vv                  : very verbose " << std::endl;

				std::cout << "\t -C bitrate           : target CBR bitrate" << std::endl;
				std::cout << "\t -V bitrate           : target VBR bitrate" << std::endl;
				std::cout << "\t -f format            : format (default is VP80) " << std::endl;

				std::cout << "\t -r                   : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w                   : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t source_device        : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				std::cout << "\t dest_device          : V4L2 capture device (default "<< out_devname << ")" << std::endl;
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

	int outformat = decodeFormat(strformat.c_str());
		
	signal(SIGINT,sighandler);	

	return compress(in_devname, ioTypeIn, out_devname, ioTypeOut, outformat, opt, stop, verbose);
}
