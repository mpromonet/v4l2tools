ALL_PROGS = v4l2copy v4l2convert_yuv v4l2source_yuv
CFLAGS = -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I include
RM = rm -rf
CC = g++

# log4cpp
LDFLAGS += -llog4cpp 
# v4l2wrapper
CFLAGS += -I v4l2wrapper/inc

.DEFAULT_GOAL := all

# raspberry tools using ilclient
ILCLIENTDIR=/opt/vc/src/hello_pi/libs/ilclient
ifneq ($(wildcard $(ILCLIENTDIR)),)
CFLAGS  +=-I /opt/vc/include/ -I /opt/vc/include/interface/vcos/ -I /opt/vc/include/interface/vcos/pthreads/ -I /opt/vc/include/interface/vmcs_host/linux/ -I $(ILCLIENTDIR) 
CFLAGS  += -DOMX_SKIP64BIT
LDFLAGS +=-L /opt/vc/lib -L $(ILCLIENTDIR) -lpthread -lopenmaxil -lbcm_host -lvcos -lvchiq_arm

v4l2compress_omx: src/encode_omx.cpp src/v4l2compress_omx.cpp $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a 
	$(CC) -o $@ $^ -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi $(CFLAGS) $(LDFLAGS) 

v4l2grab_h264: src/encode_omx.cpp src/v4l2grab_h264.cpp $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a
	$(CC) -o $@ $^ -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi $(CFLAGS) $(LDFLAGS) 

v4l2display_h264: src/v4l2display_h264.cpp $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a
	$(CC) -o $@ $^ -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi $(CFLAGS) $(LDFLAGS) 


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
ALL_PROGS+=v4l2compress_h264
endif

# libvpx
ifneq ($(wildcard /usr/include/vpx),)
ALL_PROGS+=v4l2compress_vpx
endif

# libjpeg
ifneq ($(wildcard /usr/include/jpeglib.h),)
ALL_PROGS+=v4l2compress_jpeg v4l2uncompress_jpeg
endif

all: $(ALL_PROGS)

libyuv.a:
	git submodule init libyuv
	git submodule update libyuv
	make -C libyuv -f linux.mk
	mv libyuv/libyuv.a .
	make -C libyuv -f linux.mk clean

libv4l2wrapper.a: 
	git submodule init v4l2wrapper
	git submodule update v4l2wrapper
	make -C v4l2wrapper
	mv v4l2wrapper/libv4l2wrapper.a .
	make -C v4l2wrapper clean

# read V4L2 capture -> write V4L2 output
v4l2copy: src/v4l2copy.cpp  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

# read V4L2 capture -> convert YUV format -> write V4L2 output
v4l2convert_yuv: src/v4l2convert_yuv.cpp  libyuv.a libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -I libyuv/include

# -> write V4L2 output
v4l2source_yuv: src/v4l2source_yuv.cpp  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

# read V4L2 capture -> compress using libvpx -> write V4L2 output
v4l2compress_vpx: src/v4l2compress_vpx.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lvpx -I libyuv/include

# read V4L2 capture -> compress using x264 -> write V4L2 output
v4l2compress_h264: src/v4l2compress_h264.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lx264 -I libyuv/include

# read V4L2 capture -> compress using libjpeg -> write V4L2 output
v4l2compress_jpeg: src/v4l2compress_jpeg.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -ljpeg -I libyuv/include

# read V4L2 capture -> uncompress using libjpeg -> write V4L2 output
v4l2uncompress_jpeg: src/v4l2uncompress_jpeg.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -ljpeg -I libyuv/include
	
# try with opencv
v4l2detect_yuv: src/v4l2detect_yuv.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lopencv_core -lopencv_objdetect -lopencv_imgproc -I libyuv/include

	
	
upgrade:
	git submodule foreach git pull origin master
	
install:
	install -m 0755 $(ALL_PROGS) /usr/local/bin

clean:
	-@$(RM) $(ALL_PROGS) .*o *.a
