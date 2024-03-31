FROM debian:bullseye AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    build-essential \
    libomp-dev \
    libgdal-dev \
    cmake

COPY . /orthorectify

RUN cd orthorectify && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

FROM debian:bullseye AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgomp1 libgdal28 && apt-get clean

WORKDIR /root/
COPY --from=builder /orthorectify/build ./

ENTRYPOINT  ["./Orthorectify"]
