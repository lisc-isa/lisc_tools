#! /bin/bash
#
# Script to build RISC-V ISA simulator, proxy kernel, and GNU toolchain.

. build.common

echo "Starting LISC Toolchain build process"

tar -mvzcf lisc-patch.tar.gz riscv spike_main

#cd lisc-isa-sim
git clone https://github.com/riscv/riscv-fesvr.git
git clone https://github.com/riscv/riscv-isa-sim.git
cd riscv-isa-sim
git checkout -f 2dbcb01

cp ../lisc-patch.tar.gz .

tar -mxvf lisc-patch.tar.gz
cp -f riscv/encoding.h ../riscv-fesvr/fesvr/encoding.h 
rm -rf lisc-patch.tar.gz
rm -rf ../lisc-patch.tar.gz
cd ..


build_project riscv-fesvr --prefix=$RISCV
build_project riscv-isa-sim --prefix=$RISCV --with-fesvr=$RISCV

echo -e "\\nlisc spike installation completed!"
