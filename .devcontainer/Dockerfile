# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.234.0/containers/ubuntu/.devcontainer/base.Dockerfile

# [Choice] Ubuntu version (use ubuntu-22.04 or ubuntu-18.04 on local arm64/Apple Silicon): ubuntu-22.04, ubuntu-20.04, ubuntu-18.04
ARG VARIANT="jammy"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

# [Optional] Uncomment this section to install additional OS packages.
RUN dpkg --add-architecture i386 \
    && apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        make \
        cmake \
        ninja-build \
        gcc \
        g++ \
        gcc-multilib \
        libc6:i386 \
        libncurses5:i386 \
        libstdc++6:i386 \
        libx11-dev:i386 \
        libnsl-dev:i386 \
        libxext-dev:i386 \
        libgmp-dev

RUN mkdir -p /opt \
    && cd /opt \
    && wget https://ftp.gnu.org/gnu/gdb/gdb-11.2.tar.gz \
    && tar xf gdb-11.2.tar.gz \
    && cd gdb-11.2 \
    && mkdir -p build \
    && cd build \
    && ../configure \
    && make -j8 \
    && make install \
    && cd / \
    && rm -rf /opt/gdb*

