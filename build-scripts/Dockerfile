FROM debian:bookworm

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config libssl-dev wget vim host gdb ninja-build valgrind sudo clang gcc curl libcurl4-openssl-dev libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly zip

ARG CMAKE_VERSION=3.30.2

WORKDIR /tmp
RUN wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
RUN tar xf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz  -C /usr --strip-components=1

ARG VALGRIND_VERSION=VALGRIND_3_23_0

WORKDIR /tmp
RUN git clone --depth 1 --branch ${VALGRIND_VERSION} https://sourceware.org/git/valgrind.git
WORKDIR /tmp/valgrind
RUN ./autogen.sh
RUN ./configure
RUN make
RUN make install

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME -s /bin/bash \
    #
    # [Optional] Add sudo support. Omit if you don't need to install software after connecting.
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME

WORKDIR /workspace
