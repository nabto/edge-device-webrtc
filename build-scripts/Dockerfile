FROM debian:bookworm

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config libssl-dev wget vim host gdb ninja-build valgrind sudo clang gcc

ARG CMAKE_VERSION=3.19.6

WORKDIR /tmp
RUN wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
RUN tar xf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz  -C /usr --strip-components=1


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
