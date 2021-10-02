ALL_PROGS = v4l2copy v4l2source_yuv v4l2dump v4l2compress
CFLAGS = -std=c++11 -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I include
RM = rm -rf
CC = $(CROSS)gcc
CXX = $(CROSS)g++
PREFIX?=/usr
$(info PREFIX=$(PREFIX))
DESTDIR?=$(PREFIX)
ARCH?=$(shell uname -m)
$(info ARCH=$(ARCH))

ifeq ("$(ARCH)","aarch64")
CMAKE_CXX_FLAGS += -DLIBYUV_DISABLE_NEON
CFLAGS += -DLIBYUV_DISABLE_NEON
endif

# log4cpp
ifeq ($(shell pkg-config --exists log4cpp && echo yes || echo no),yes)
$(info with log4cpp)
CFLAGS += -DHAVE_LOG4CPP 
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

ifeq ("$(ARCH)","x86_64")
# cuda
CUDADIR=/usr/local/cuda
ifneq ($(wildcard $(CUDADIR)),)
CFLAGS  +=-DHAVE_CUDA -I $(CUDADIR)/include/ -I $(PWD)/Video_Codec_SDK_11.1.5/Interface
LDFLAGS += $(PWD)/Video_Codec_SDK_11.1.5/Lib/linux/stubs/x86_64/libnvidia-encode.so $(PWD)/Video_Codec_SDK_11.1.5/Lib/linux/stubs/x86_64/libnvcuvid.so /usr/local/cuda/targets/x86_64-linux/lib/stubs/libcuda.so
endif
endif


# opencv
ifeq ($(shell pkg-config --exists opencv && echo yes || echo no),yes)
ALL_PROGS+=v4l2detect_yuv
endif

# libx264
ifeq ($(shell pkg-config --exists x264 && echo yes || echo no),yes)
$(info with x264)
CFLAGS += -DHAVE_X264
LDFLAGS += -lx264
endif

# libx265
ifeq ($(shell pkg-config --exists x265 && echo yes || echo no),yes)
$(info with x265)
CFLAGS += -DHAVE_X265
LDFLAGS += -lx265
endif

# libvpx
ifeq ($(shell pkg-config --exists vpx && echo yes || echo no),yes)
$(info with vpx)
CFLAGS += -DHAVE_VPX
LDFLAGS += -lvpx
endif

# libjpeg
ifeq ($(shell pkg-config --exists libjpeg && echo yes || echo no),yes)
$(info with jpeg)
ALL_PROGS+=v4l2uncompress_jpeg
CFLAGS += -DHAVE_JPEG
LDFLAGS += -ljpeg
endif

# libfuse
ifeq ($(shell pkg-config --exists fuse && echo yes || echo no),yes)
ALL_PROGS+=v4l2fuse
endif

all: $(ALL_PROGS)

libyuv/CMakeLists.txt:
	git submodule update --init libyuv

libyuv.a: libyuv/CMakeLists.txt
	cd libyuv && cmake -DCMAKE_CXX_FLAGS=$(CMAKE_CXX_FLAGS) . && make 
	mv libyuv/libyuv.a .
	make -C libyuv clean

v4l2wrapper/Makefile:
	git submodule update --init v4l2wrapper

libv4l2wrapper.a: v4l2wrapper/Makefile
	make -C v4l2wrapper
	mv v4l2wrapper/libv4l2wrapper.a .
	make -C v4l2wrapper clean

# read V4L2 capture -> write V4L2 output
v4l2copy: src/v4l2copy.cpp  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS)

# -> write V4L2 output
v4l2source_yuv: src/v4l2source_yuv.cpp  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS)

# read V4L2 capture -> compress using libvpx/libx264/libx265 -> write V4L2 output
v4l2compress: src/v4l2compress.cpp libyuv.a  libv4l2wrapper.a
	echo $(LDFLAGS)
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS) -I libyuv/include

# read V4L2 capture -> uncompress using libjpeg -> write V4L2 output
v4l2uncompress_jpeg: src/v4l2uncompress_jpeg.cpp libyuv.a  libv4l2wrapper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS)  -I libyuv/include
	
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
