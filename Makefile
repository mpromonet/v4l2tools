ALL_PROGS = v4l2copy v4l2convert_yuv v4l2source_yuv v4l2dump v4l2compress
CFLAGS = -std=c++11 -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I include
RM = rm -rf
CC = $(CROSS)gcc
CXX = $(CROSS)g++
PREFIX?=/usr
DESTDIR?=$(PREFIX)
ARCH?=$(shell uname -m)

ifeq ($(ARCH),"arm64")
CMAKE_CXX_FLAGS += -mfpu=neon
endif

# log4cpp
ifneq ($(wildcard $(SYSROOT)$(PREFIX)/include/log4cpp/Category.hh),)
$(info with log4cpp)
CFLAGS += -DHAVE_LOG4CPP -I $(SYSROOT)$(PREFIX)/include
LDFLAGS += -llog4cpp 
endif

# v4l2wrapper
CFLAGS += -I v4l2wrapper/inc

.DEFAULT_GOAL := all

# raspberry tools using ilclient
ILCLIENTDIR=/opt/vc/src/hello_pi/libs/ilclient
ifneq ($(wildcard $(ILCLIENTDIR)),)
CFLAGS  +=-I /opt/vc/include/ -I /opt/vc/include/interface/vcos/ -I /opt/vc/include/interface/vcos/pthreads/ -I /opt/vc/include/interface/vmcs_host/linux/ -I $(ILCLIENTDIR) 
CFLAGS  += -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi
LDFLAGS +=-L /opt/vc/lib -L $(ILCLIENTDIR) -lpthread -lopenmaxil -lbcm_host -lvcos -lvchiq_arm

v4l2compress_omx: src/encode_omx.cpp src/v4l2compress_omx.cpp $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a libyuv.a
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS) -I libyuv/include

v4l2grab_h264: src/encode_omx.cpp src/v4l2grab_h264.cpp $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS) 

v4l2display_h264: src/v4l2display_h264.cpp $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS) 


$(ILCLIENTDIR)/libilclient.a:
	make -C $(ILCLIENTDIR)
	
ALL_PROGS+=v4l2grab_h264
ALL_PROGS+=v4l2display_h264
ALL_PROGS+=v4l2compress_omx
endif

# opencv
ifneq ($(wildcard /usr/include/opencv),)
ALL_PROGS+=v4l2detect_yuv
endif

# libx264
ifneq ($(wildcard /usr/include/x264.h),)
CFLAGS += -DHAVE_X264
LDFLAGS += -lx264
endif

# libx265
ifneq ($(wildcard /usr/include/x265.h),)
CFLAGS += -DHAVE_X265
LDFLAGS += -lx265
endif

# libvpx
ifneq ($(wildcard /usr/include/vpx),)
CFLAGS += -DHAVE_VPX
LDFLAGS += -lvpx
endif

# libjpeg
ifneq ($(wildcard /usr/include/jpeglib.h),)
ALL_PROGS+=v4l2compress_jpeg v4l2uncompress_jpeg
CFLAGS += -DHAVE_JPEG
LDFLAGS += -ljpeg
endif

# libfuse
ifneq ($(wildcard /usr/include/fuse.h),)
ALL_PROGS+=v4l2fuse
endif

all: $(ALL_PROGS)

libyuv.a:
	git submodule init libyuv
	git submodule update libyuv
	cd libyuv && cmake -DCMAKE_CXX_FLAGS=$(CMAKE_CXX_FLAGS) . && make VERBOSE=1
	mv libyuv/libyuv.a .
	make -C libyuv clean

libv4l2wrapper.a: 
	git submodule init v4l2wrapper
	git submodule update v4l2wrapper
	make -C v4l2wrapper
	mv v4l2wrapper/libv4l2wrapper.a .
	make -C v4l2wrapper clean

# read V4L2 capture -> write V4L2 output
v4l2copy: src/v4l2copy.cpp  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS)

# read V4L2 capture -> convert YUV format -> write V4L2 output
v4l2convert_yuv: src/v4l2convert_yuv.cpp  libyuv.a libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -I libyuv/include

# -> write V4L2 output
v4l2source_yuv: src/v4l2source_yuv.cpp  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS)

# read V4L2 capture -> compress using libvpx/libx264/libx265 -> write V4L2 output
v4l2compress: src/v4l2compress.cpp libyuv.a  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -I libyuv/include

# read V4L2 capture -> compress using libjpeg -> write V4L2 output
v4l2compress_jpeg: src/v4l2compress_jpeg.cpp libyuv.a  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -ljpeg -I libyuv/include

# read V4L2 capture -> uncompress using libjpeg -> write V4L2 output
v4l2uncompress_jpeg: src/v4l2uncompress_jpeg.cpp libyuv.a  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -ljpeg -I libyuv/include
	
# try with opencv
v4l2detect_yuv: src/v4l2detect_yuv.cpp libyuv.a  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lopencv_core -lopencv_objdetect -lopencv_imgproc -I libyuv/include

# dump
h264bitstream/Makefile:
	git submodule update --init h264bitstream	

h264bitstream/.libs/libh264bitstream.so: h264bitstream/Makefile
	cd h264bitstream && autoreconf -i -f && ./configure --host $(shell $(CC) -dumpmachine)
	make -C h264bitstream 

hevcbitstream/Makefile:
	git submodule update --init hevcbitstream	

hevcbitstream/.libs/libhevcbitstream.so: hevcbitstream/Makefile
	cd hevcbitstream && autoreconf -i -f && LDFLAGS=-lm ./configure --host $(shell $(CC) -dumpmachine)
	make -C hevcbitstream 

v4l2dump: src/v4l2dump.cpp libv4l2wrapper.a h264bitstream/.libs/libh264bitstream.so  hevcbitstream/.libs/libhevcbitstream.so libyuv.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -Ih264bitstream  -Ihevcbitstream -Wl,-rpath=./h264bitstream/.libs,-rpath=./hevcbitstream/.libs -I libyuv/include 

v4l2fuse: src/v4l2fuse.c 
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -D_FILE_OFFSET_BITS=64 -lfuse

	
upgrade:
	git submodule foreach git pull origin master
	
install: all
	mkdir -p $(DESTDIR)/bin
	install -D -m 0755 $(ALL_PROGS) $(DESTDIR)/bin

clean:
	-@$(RM) $(ALL_PROGS) .*o *.a
