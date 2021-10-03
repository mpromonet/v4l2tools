ARG IMAGE=ubuntu:20.04
FROM $IMAGE as builder
LABEL maintainer michel.promonet@free.fr
WORKDIR /build
COPY . /build

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates g++ autoconf automake libtool xz-utils cmake make pkg-config git wget libx264-dev libx265-dev libjpeg-dev libvpx-dev \
    && make install PREFIX=/usr/local && apt-get clean && rm -rf /var/lib/apt/lists/

FROM $IMAGE
WORKDIR /usr/local/share/v4l2tools
COPY --from=builder /usr/local/bin/ /usr/local/bin/

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates x264 x265 libjpeg-dev libvpx6 \
    && apt-get clean && rm -rf /var/lib/apt/lists/

ENTRYPOINT [ "/usr/local/bin/v4l2compress" ]
CMD [ "" ]
