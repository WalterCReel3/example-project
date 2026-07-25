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

ENV DEBIAN_FRONTEND=noninteractive

# buster is EOL; its apt sources have moved to archive.debian.org. The base image
# already handles this, but curl/unzip may not be present.
RUN apt-get -y update \
    && apt-get -y install --no-install-recommends ca-certificates curl unzip \
    && rm -rf /var/lib/apt/lists/*

# Official upstream binaries: no compiling, and independent of buster's ancient
# system CMake, which is left in place untouched.
RUN set -eux; \
    arch="$(uname -m)"; \
    case "$arch" in \
        x86_64)  cm_arch=x86_64;  nj=ninja-linux.zip ;; \
        aarch64) cm_arch=aarch64; nj=ninja-linux-aarch64.zip ;; \
        *) echo "unsupported host arch: $arch" >&2; exit 1 ;; \
    esac; \
    curl -fsSL -o /tmp/cmake.tar.gz \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${cm_arch}.tar.gz"; \
    mkdir -p /opt/cmake; \
    tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1; \
    rm /tmp/cmake.tar.gz; \
    curl -fsSL -o /tmp/ninja.zip \
        "https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/${nj}"; \
    unzip -q /tmp/ninja.zip -d /usr/local/bin; \
    chmod +x /usr/local/bin/ninja; \
    rm /tmp/ninja.zip

# Ahead of /usr/bin so the modern CMake wins over buster's 3.13.
ENV PATH="/opt/cmake/bin:${PATH}"

# Mirrors union-miyoomini-toolchain/support/setup-env.sh.
ENV MIYOOMINI_TOOLCHAIN_ROOT=/opt/miyoomini-toolchain
ENV UNION_PLATFORM=miyoomini

RUN cmake --version && ninja --version \
    && "${MIYOOMINI_TOOLCHAIN_ROOT}/usr/bin/arm-linux-gnueabihf-g++" --version

WORKDIR /src
CMD ["/bin/bash"]
