[![Codacy Badge](https://api.codacy.com/project/badge/Grade/0c2b3215b77a4a6f82d977986bca7842)](https://www.codacy.com/app/michelpromonet_2643/v4l2tools?utm_source=github.com&utm_medium=referral&utm_content=mpromonet/v4l2tools&utm_campaign=badger)
[![Build status](https://travis-ci.org/mpromonet/v4l2tools.png)](https://travis-ci.org/mpromonet/v4l2tools)
[![CircleCI](https://circleci.com/gh/mpromonet/v4l2tools.svg?style=shield)](https://circleci.com/gh/mpromonet/v4l2tools)
[![CirusCI](https://api.cirrus-ci.com/github/mpromonet/v4l2tools.svg?branch=master)](https://cirrus-ci.com/github/mpromonet/v4l2tools)
[![Snap Status](https://snapcraft.io//v4l2tools/badge.svg)](https://snapcraft.io/v4l2tools)
[![GithubCI](https://github.com/mpromonet/v4l2tools/workflows/C/C++%20CI/badge.svg)](https://github.com/mpromonet/v4l2tools/actions)

[![Docker Pulls](https://img.shields.io/docker/pulls/mpromonet/v4l2tools.svg)](https://hub.docker.com/r/mpromonet/v4l2tools/)

v4l2tools
====================

This is simple V4L2 tools based on libv4l2cpp

Dependencies
------------
 - liblog4cpp5-dev (optional)
 - libvpx-dev      (for v4l2compress)
 - libx264-dev     (for v4l2compress)
 - libx265-dev     (for v4l2compress)
 - libjpeg-dev     (for v4l2compress)
 
Tools
-------

 - v4l2copy          : 

>	read from a V4L2 capture device and write to a V4L2 output device

 - v4l2compress  : 

>	read YUV from a V4L2 capture device, compress in VP8/VP9/H264/HEVC/JPEG format and write to a V4L2 output device
>	read JPEG format from a V4L2 capture device, uncompress in JPEG format and write to a V4L2 output device

 - v4l2dump          : 

>	read from a V4L2 capture device and print to output frame information (work with H264 & HEVC)

 - v4l2source_yuv :
 
>	generate YUYV frames and write to a V4L2 output device

Tools for Raspberry
-------------------

 - v4l2grab_h264     : 

>	grab raspberry pi screen, compress in H264 format using OMX and write to a V4L2 output device

 - v4l2display_h264     : 

>	read H264 from V4L2 capture device, uncompress and display using OMX

 - v4l2compress_omx : 

>	read YUV from a V4L2 capture device, compress in H264 format using OMX and write to a V4L2 output device

Build
-----

     make

Install
-------

     make install

Using Docker image
===============

You can expose V4L2 devices from your host using :

        docker run --device=/dev/video0 --device=/dev/video1 -it mpromonet/v4l2tools

The container entry point is the v4l2tools application, then you can :

* get the help using :

        docker run -it mpromonet/v4l2tools -h

* run the container specifying some paramaters :

        docker run --device=/dev/video0 --device=/dev/video1 -it mpromonet/v4l2tools -fH264
