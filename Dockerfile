FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    build-essential \
    libomp-dev \
    libgdal-dev \
    libeigen3-dev \
    nlohmann-json3-dev \
    cmake \
    ninja-build \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

COPY . /orthorectify
WORKDIR /orthorectify

# Bootstrap vcpkg and install packages not available via apt (header-only, fast)
RUN ./vcpkg/bootstrap-vcpkg.sh -disableMetrics && \
    VCPKG_MANIFEST_INSTALL=OFF ./vcpkg/vcpkg install cxxopts plog --triplet=x64-linux

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
        -G Ninja \
        -DCMAKE_PREFIX_PATH=/orthorectify/vcpkg/installed/x64-linux \
    && cmake --build build -j$(nproc)

FROM debian:bookworm-slim AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgomp1 \
    libgdal32 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /root/
COPY --from=builder /orthorectify/build/Orthorectify ./

ENTRYPOINT ["./Orthorectify"]
