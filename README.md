[![Build status](https://travis-ci.org/mpromonet/v4l2tools.png)](https://travis-ci.org/mpromonet/v4l2tools)

v4l2tools
====================

This is V4L2 tools based on libv4l2cpp

License
------------
Domain public 

Dependencies
------------
 - liblog4cpp5-dev
 - libvpx-dev      (for v4l2compress_vp8)
 - libx264-dev     (for v4l2compress_h264)

 
Tools
-------

 - v4l2copy          : 

>	read from a V4L2 capture device and write to a V4L2 output device

 - v4l2convert_YUV          : 

>	read an YUV format from a V4L2 capture device, convert to an other YUV format and write to a V4L2 output device

 - v4l2compress_vpx  : 

>	read YUYV from a V4L2 capture device, compress in VP8 format using libvpx and write to a V4L2 output device

 - v4l2compress_h264 : 

>	read YUYV from a V4L2 capture device, compress in H264 format using libx264 and write to a V4L2 output device

Tools for Raspberry
-------------------

 - v4l2grab_h264     : 

>	grab raspberry pi screen, compress in H264 format using OMX and write to a V4L2 output device

 - v4l2display_h264     : 

>	read H264 from V4L2 capture device, uncompress and display using OMX

 - v4l2compress_omx : 

>	read YUV420 from a V4L2 capture device, compress in H264 format using OMX and write to a V4L2 output device

Build
-----

     make

Install
-------

     make install

