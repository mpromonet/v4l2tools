FROM ubuntu:18.04 as builder
LABEL maintainer michel.promonet@free.fr
WORKDIR /build
COPY . /build

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates g++ autoconf automake libtool xz-utils cmake make pkg-config git wget libx264-dev libx265-dev libjpeg8-dev libvpx-dev \
    && make install PREFIX=/usr/local && apt-get clean && rm -rf /var/lib/apt/lists/

FROM ubuntu:18.04
WORKDIR /usr/local/share/v4l2tools
COPY --from=builder /usr/local/bin/ /usr/local/bin/

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates x264 x265 libjpeg8 libvpx5 \
    && apt-get clean && rm -rf /var/lib/apt/lists/

ENTRYPOINT [ "/usr/local/bin/v4l2compress" ]
CMD [ "" ]
