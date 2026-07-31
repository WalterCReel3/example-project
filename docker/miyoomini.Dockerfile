# Miyoo Mini build environment.
#
# The upstream union-miyoomini-toolchain image is Debian 10 buster, whose CMake
# is 3.13 — too old for this project's CMakePresets.json (needs 3.21). This
# layers a current CMake and Ninja on top so `cmake --preset miyoomini` works
# inside the container without further setup.
#
# Build the base image first, since it is not published to a registry:
#
#     git clone https://github.com/shauninman/union-miyoomini-toolchain.git
#     cd union-miyoomini-toolchain && make .build     # tags 'miyoomini-toolchain'
#
# Then from this repository:
#
#     docker build -f docker/miyoomini.Dockerfile -t wreel-miyoomini .
#     docker run --rm -it -v "$PWD":/src -w /src wreel-miyoomini bash
#     cmake --preset miyoomini && cmake --build --preset miyoomini

ARG BASE_IMAGE=miyoomini-toolchain
FROM ${BASE_IMAGE}

# Pin so image rebuilds are reproducible.
ARG CMAKE_VERSION=3.31.6
ARG NINJA_VERSION=1.12.1
ARG PATCHELF_VERSION=0.19.1

ENV DEBIAN_FRONTEND=noninteractive

# Deliberately NO apt-get here. The base is Debian 10 buster, which is EOL and
# whose repositories have moved to archive.debian.org; running apt against it is
# the single most likely step to fail and it buys nothing, because the upstream
# image already installs everything needed:
#
#   wget, unzip, tar  — from union-miyoomini-toolchain's own Dockerfile
#   working TLS       — its setup-toolchain.sh already wgets over https
#   readelf           — binutils, this being a toolchain image
#
# Official upstream binaries, no compiling, and buster's ancient system CMake
# (3.13) is left in place untouched rather than replaced.
#
# patchelf is here for the bundle rather than for the build: the vendored SDL2
# runtime declares a 21.8 MB libGLESv2.so it references no symbol from, and
# `cmake --build --target bundle-onion` drops that dependency. Without patchelf
# the step skips and the bundle ships at 30 MB instead of 8.4 MB. Its release
# binaries are static-pie with no libc floor at all, so buster is a non-issue.
RUN set -eux; \
    arch="$(uname -m)"; \
    case "$arch" in \
        x86_64)  cm_arch=x86_64;  nj=ninja-linux.zip;         pe_arch=x86_64 ;; \
        aarch64) cm_arch=aarch64; nj=ninja-linux-aarch64.zip; pe_arch=aarch64 ;; \
        *) echo "unsupported host arch: $arch" >&2; exit 1 ;; \
    esac; \
    wget -q -O /tmp/cmake.tar.gz \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${cm_arch}.tar.gz"; \
    mkdir -p /opt/cmake; \
    tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1; \
    rm /tmp/cmake.tar.gz; \
    wget -q -O /tmp/ninja.zip \
        "https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/${nj}"; \
    unzip -q /tmp/ninja.zip -d /usr/local/bin; \
    chmod +x /usr/local/bin/ninja; \
    rm /tmp/ninja.zip; \
    wget -q -O /tmp/patchelf.tar.gz \
        "https://github.com/NixOS/patchelf/releases/download/${PATCHELF_VERSION}/patchelf-${PATCHELF_VERSION}-${pe_arch}.tar.gz"; \
    tar -xzf /tmp/patchelf.tar.gz -C /usr/local ./bin/patchelf; \
    rm /tmp/patchelf.tar.gz

# Ahead of /usr/bin so the modern CMake wins over buster's 3.13.
ENV PATH="/opt/cmake/bin:${PATH}"

# Mirrors union-miyoomini-toolchain/support/setup-env.sh.
ENV MIYOOMINI_TOOLCHAIN_ROOT=/opt/miyoomini-toolchain
ENV UNION_PLATFORM=miyoomini

RUN cmake --version && ninja --version && patchelf --version && readelf --version \
    && "${MIYOOMINI_TOOLCHAIN_ROOT}/usr/bin/arm-linux-gnueabihf-g++" --version

WORKDIR /src
CMD ["/bin/bash"]
