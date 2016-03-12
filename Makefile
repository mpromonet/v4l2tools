ALL_PROGS = v4l2copy v4l2compress_vp8 v4l2compress_h264 v4l2convert_yuv
CFLAGS = -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I include
RM = rm -rf
CC = g++

# log4cpp
LDFLAGS += -llog4cpp 
# v4l2wrapper
CFLAGS += -I v4l2wrapper/inc

.DEFAULT_GOAL := all

# raspberry grab -> compress H264 -> write V4L2 output
ILCLIENTDIR=/opt/vc/src/hello_pi/libs/ilclient
ifneq ($(wildcard $(ILCLIENTDIR)),)
CFLAGS  +=-I /opt/vc/include/ -I /opt/vc/include/interface/vcos/ -I /opt/vc/include/interface/vcos/pthreads/ -I /opt/vc/include/interface/vmcs_host/linux/ -I $(ILCLIENTDIR)
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

# read V4L2 capture -> compress using libvpx -> write V4L2 output
v4l2compress_vp8: src/v4l2compress_vp8.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lvpx -I libyuv/include

# read V4L2 capture -> compress using x264 -> write V4L2 output
v4l2compress_h264: src/v4l2compress_h264.cpp libyuv.a  libv4l2wrapper.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lx264 -I libyuv/include
	
upgrade:
	git submodule foreach git pull origin master
	
install:
	install -m 0755 $(ALL_PROGS) /usr/local/bin

clean:
	-@$(RM) $(ALL_PROGS) .*o *.a
