FROM ubuntu:20.04
MAINTAINER Darrell Wright
ENV LLVM_VERSION=13
ENV CONTAINER_USER="developer"
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get -y upgrade && apt-get -y install wget sudo gnupg
RUN wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add - && \
	echo "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-${LLVM_VERSION} main" >> /etc/apt/sources.list.d/llvm.list && \
	wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc|sudo apt-key add - && \
	echo "deb https://apt.kitware.com/ubuntu/ focal main" >> /etc/apt/sources.list.d/kitware.list  && \
	apt-get update

RUN apt-get -y install \
  bash-completion \
  build-essential \
  clang-${LLVM_VERSION} \
  cmake \
  coreutils \
  gcc \
  g++ \
  gdb \
  git-core \
  htop \
  ninja-build \
  unzip \
  vim


# Add non-root user for container but give it sudo access.
# Password is the same as the username
RUN useradd -m ${CONTAINER_USER} && \
    echo ${CONTAINER_USER}:${CONTAINER_USER} | chpasswd && \
    cp /etc/sudoers /etc/sudoers.bak && \
    echo "${CONTAINER_USER}  ALL=(root) ALL" >> /etc/sudoers
# Make bash the default shell (useful for when using tmux in the container)
RUN chsh --shell /bin/bash ${CONTAINER_USER}
USER ${CONTAINER_USER}
