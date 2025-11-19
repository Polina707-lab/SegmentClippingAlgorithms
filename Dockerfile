FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# --- обновления ---
RUN apt-get update && apt-get upgrade -y

# --- зависимости CMake/Qt6 ---
RUN apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-tools-dev \
    qttools5-dev-tools

# --- X11 для запуска GUI через XLaunch ---
RUN apt-get install -y \
    libxkbcommon-x11-0 \
    libx11-xcb1 \
    libxcb1 \
    libxext6 \
    libxfixes3 \
    xauth \
    x11-apps

# --- проект ---
WORKDIR /app
COPY . /app

# --- сборка ---
RUN cmake -B build -S . -G Ninja \
 && cmake --build build -j$(nproc)

# запуск
CMD ["./build/SegmentClippingAlgorithms"]
