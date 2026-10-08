#!/bin/bash

#
# Script for downloading and compiling the riscv toolchain for linux base systems
#
# Instructions gotten from the riscv github
#  

set -e

./utils.sh || { echo "Could not load utils file" && exit 255 }

ROOT_DIR=$(pwd)

if [ -z $(command -v git) ]; then
   error "please install git first"
   exit 255
fi

install_arch() {
   info "It can be downloaded too via AUR instead of compiling"
   sudo pacman -Syu curl python3 libmpc mpfr gmp base-devel texinfo gperf patchutils bc zlib expat libslirp ncurses meson ninja cmake
} #install_arch

install_fedora() {
   sudo yum install autoconf automake python3 libmpc-devel mpfr-devel gmp-devel gawk  bison flex texinfo patchutils gcc gcc-c++ zlib-devel expat-devel libslirp-devel ncurses-devel meson ninja-build cmake
} #install_arch

install_debian() {
   sudo apt-get install autoconf automake autotools-dev curl python3 python3-pip python3-tomli libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev meson ninja-build git cmake libglib2.0-dev expect device-tree-compiler libslirp-dev libzstd-dev libncurses-dev
} #install_arch
clone() {
   git clone https://github.com/riscv/riscv-gnu-toolchain    toolchain
   cd toolchain
   git submodule update --init --recursive 
   cd ..
} #clone
install_deps() {
  DISTRO=$(cat /etc/os-release | grep "^ID=" | cut -d '=' -f 2)
  case DISTRO in
      cachyos|arch)
       install_arch()
       ;;
      fedora|relos|centos)
       install_fedora()
       ;;
      ubuntu|debian|mint)
       install_debian()
       ;;
      *)
       error "$DISTRO not supported"
       exit 255
       ;;
  esac
  success "dependencies installed"
} #install_deps

build() {
 cd $ROOT_DIR/toolchain
 make linux
} #build

# Execution

clone()
install_deps()
build()
