#! /bin/bash
#
# Script to build RISC-V ISA simulator, proxy kernel, and GNU toolchain.

. build.common

echo "Starting LISC Toolchain build process"

tar -vzxf lisc-patch.tar.gz

#cd lisc-isa-sim
git clone https://github.com/riscv/riscv-fesvr.git
git clone https://github.com/riscv/riscv-isa-sim.git
git checkout -f 2dbcb01

cp -f lisc-delta/riscv/encoding.h  riscv-fesvr/fesvr/encoding.h
cp -f lisc-delta/riscv/insns/auipc.h riscv-isa-sim/riscv/insns/auipc.h
cp -f lisc-delta/riscv/debug_module.cc riscv-isa-sim/riscv/debug_module.cc
cp -f lisc-delta/riscv/decode.h riscv-isa-sim/riscv/decode.h
cp -f lisc-delta/riscv/encoding.h riscv-isa-sim/riscv/encoding.h
cp -f lisc-delta/riscv/insns/csrrci.h riscv-isa-sim/riscv/insns/csrrci.h
cp -f lisc-delta/riscv/insns/csrrsi.h riscv-isa-sim/riscv/insns/csrrsi.h
cp -f lisc-delta/riscv/insns/csrrwi.h riscv-isa-sim/riscv/insns/csrrwi.h
cp -f lisc-delta/riscv/insns/jal.h riscv-isa-sim/riscv/insns/jal.h
cp -f lisc-delta/riscv/insns/jalr.h riscv-isa-sim/riscv/insns/jalr.h
cp -f lisc-delta/riscv/insns/j.h riscv-isa-sim/riscv/insns/j.h
cp -f lisc-delta/riscv/insns/jr.h riscv-isa-sim/riscv/insns/jr.h
cp -f lisc-delta/riscv/opcodes.h riscv-isa-sim/riscv/opcodes.h
cp -f lisc-delta/riscv/riscv.mk.in riscv-isa-sim/riscv/riscv.mk.in
cp -f lisc-delta/riscv/sim.cc riscv-isa-sim/riscv/sim.cc
cp -f lisc-delta/spike_main/disasm.cc riscv-isa-sim/spike_main/disasm.cc


build_project riscv-fesvr --prefix=$RISCV
build_project riscv-isa-sim --prefix=$RISCV --with-fesvr=$RISCV

echo -e "\\nlisc spike installation completed!"
