FROM ubuntu:20.04

RUN apt update && \
  apt upgrade -y && \
  apt install -y cmake ninja-build libssl-dev inotify-tools

WORKDIR /kyber

COPY monitor-and-compile.sh .

# RUN mkdir build-ninja && \
#   cd build-ninja && \
#   cmake -DBUILD_SHARED_LIBS=ON -GNinja .. && \
#   ninja && ninja test

CMD ["bash", "monitor-and-compile.sh"]
# CMD ["/kyber/build-ninja/avx2/test_speed512-90s_avx2"]
