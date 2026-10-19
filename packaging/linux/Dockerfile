# Build environment for the Linux AppImage (scripts/build-appimage.sh).
#
# Ubuntu 22.04 keeps glibc at 2.35, so the binary runs on SteamOS and other current distributions;
# a build made on a newer distribution would need its newer glibc. Ubuntu's SDL2 (2.0.20) is too old
# for the frontend, so SDL2 is built from source and bundled into the AppImage.

FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG LLVM_VERSION=18
ARG SDL2_VERSION=2.30.3

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget gnupg lsb-release software-properties-common \
        build-essential ninja-build pkg-config python3 python3-pip git file desktop-file-utils \
        libfreetype-dev libgtk-3-dev libvulkan-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxinerama-dev \
        libwayland-dev libxkbcommon-dev wayland-protocols libegl-dev libdecor-0-dev \
        libpulse-dev libasound2-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev \
    && rm -rf /var/lib/apt/lists/*

# GCC 13's C++ library headers: clang uses the newest installed GCC's, and Ubuntu's GCC 11 lacks
# constexpr std::string. libstdc++ is linked statically (CMakeLists.txt), so nothing newer is
# needed at runtime.
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt-get update \
    && apt-get install -y --no-install-recommends g++-13 \
    && rm -rf /var/lib/apt/lists/*

# Clang (the project builds with clang) and a newer CMake than Ubuntu's 3.22.
RUN wget -q https://apt.llvm.org/llvm.sh && bash llvm.sh ${LLVM_VERSION} && rm llvm.sh \
    && ln -sf /usr/bin/clang-${LLVM_VERSION} /usr/local/bin/clang \
    && ln -sf /usr/bin/clang++-${LLVM_VERSION} /usr/local/bin/clang++ \
    && pip3 install --no-cache-dir cmake==3.30.5

RUN wget -q https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz \
    && tar xf SDL2-${SDL2_VERSION}.tar.gz \
    && cmake -S SDL2-${SDL2_VERSION} -B sdl-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DSDL_STATIC=OFF -DSDL_SHARED=ON -DSDL_WAYLAND=ON -DSDL_X11=ON -DSDL_PIPEWIRE=ON -DSDL_PULSEAUDIO=ON \
    && cmake --build sdl-build && cmake --install sdl-build \
    && rm -rf SDL2-${SDL2_VERSION} SDL2-${SDL2_VERSION}.tar.gz sdl-build && ldconfig

# AppImage tools. linuxdeploy collects the libraries the binary needs; the GTK plugin carries what
# the file dialog (nativefiledialog, GTK 3) needs at runtime.
RUN wget -q -O /usr/local/bin/linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && wget -q -O /usr/local/bin/linuxdeploy-plugin-gtk https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh \
    && chmod +x /usr/local/bin/linuxdeploy /usr/local/bin/linuxdeploy-plugin-gtk

# The AppImage tools can't use FUSE inside a container.
ENV APPIMAGE_EXTRACT_AND_RUN=1
