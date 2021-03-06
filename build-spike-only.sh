#! /bin/bash
#
# Script to build LISC ISA simulator and GNU toolchain.

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
build_project isa-sim --prefix=$LISC --with-fesvr=$LISC

echo -e "\\nlisc spike installation completed!"
