#! /bin/bash
#
# Script to build LISC ISA simulator, proxy kernel, and GNU toolchain.

. build.common

echo "Starting LISC Toolchain build process"

cd ./patch && tar -mvzcf lisc-patch.tar.gz riscv spike_main && cd ..

if [ ! -e fesvr ]
then
  (
  git clone https://github.com/riscv/riscv-fesvr.git fesvr
  )
fi
if [ ! -e isa-sim ]
then
  (
  git clone https://github.com/riscv/riscv-isa-sim.git isa-sim
  )
fi


cd isa-sim
git checkout -f 2dbcb01

tar -mxvf ../patch/lisc-patch.tar.gz
cp -f riscv/encoding.h ../fesvr/fesvr/encoding.h 
rm -rf ../patch/lisc-patch.tar.gz
cd ..

build_project fesvr --prefix=$LISC
build_project isa-sim --prefix=$LISC --with-fesvr=$LISC --with-isa=rv32im

echo -e "\\nlisc spike installation completed!"

cd ./patch && tar -mvzcf lisc-patch.tar.gz pk machine configure Makefile.in && cd ..
if [ ! -e pk ]
then
  (
  git clone https://github.com/riscv/riscv-pk.git pk
  )
fi

cd pk
git checkout -f 537ae11

tar -mxvf ../patch/lisc-patch.tar.gz
rm -rf ../patch/lisc-patch.tar.gz
cd ..

if [ ! `which lisc32-unknown-elf-gcc` ]
then
  echo "lisc32-unknown-elf-gcc doesn't appear to be installed; use the full-on build.sh"
  exit 1
fi

CC= CXX= build_project pk --prefix=$RISCV --host=lisc32-unknown-elf --with-arch=RV32IM --disable-fp-emulation --disable-vm

echo -e "\\nlisc spike pk installation completed!"
