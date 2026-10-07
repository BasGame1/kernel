#!/bin/bash

set -e

ROOT_DIR=$(pwd)

if [ -z $(command -v git) ]; then
   echo "ERROR: please install git first"
   exit 255
fi
install_deps() {
  DISTRO = $(cat /etc/os-release | grep "^ID=" | cut -d '=' -f 2)
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
       echo "$DISTRO not supported"
       exit 255
       ;;
  esac

} #install_deps
# Instructions gotten from the riscv github
git clone https://github.com/riscv/riscv-gnu-toolchain toolchain
cd toolchain
git submodule update --init --recursive 
install_deps()
make linux

