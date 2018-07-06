# lisc_gnu_toolchain
LISC指令系统是在RISC-V指令系统基础上开发的全新指令系统，具有以下特点：
  1. 16个系统寄存器；
  2. 全新的指令编码；
  3. 大多数指令包含3-4比特的CLIPS位（Cross-Layer Information Parcels），在指令系统中不定义，用于未来定制指令系统之用；
  4. 去除‘JAL’指令的Rd位域，由INSN[28]=1定义PC+4回写，回写地址固定为x1 （去除RISC-V中回写x5的abi）；INSN[28]=0时，不回写，汇编助记符为'J'；
  5. ‘JALR’指令，由INSN[28]=1定义PC+4回写到Rd寄存器；INSN[28]=0时，不回写，汇编助记符为'Jr'；
  6. LISC指令系统的16位指令完全采用RV32E指令编码。
  TODO:
  7. 加入LPTR和SPTR指令，支持指针类型数据的load/store.

LISC指令系统用于北京大学信息科学技术学院《数字逻辑和微处理器设计》课程的教学之用，也向外界全面开放。参见LICENSE文件。

lisc_gnu_toolchian采用https://github.com/pulp-platform/ri5cy_gnu_toolchain 工具链。更改了其中binutils的若干文件而成。

GCC-6支持：
  如果用GCC-6以上的版本编译，需要打上ri5cy_gnu_toolchain/toolchain-patches/中的支持GCC-6的补丁。（注意，ri5cy_gnu_toolchain/toolchain-patches中的patch文件，最后一行为非法字符，应该删除'+'后面的所有字符）

# ri5cy_gnu_toolchain
Introduction
This is a port of the RISCV GCC toolchain, which has been extended to support the extensions of the Pulpino core.

Build

Run the following command to build the toolchain (by default for riscy, see the next section to select another core):

make

This will download a specific version of the RISCV toolchain based on gcc 5.2, patch it with extensions for Pulpino and compile it.

The resulting toolchains should be in the install directory.

Supported cores

This toolchain supports the following Pulpino cores :

Riscy. Compile the toolchain with: make

Riscy_fpu (riscy with hardare floating point unit). Compile the toolchain with: make RISCY_FPU=1

Zeroriscy. Compile the toolchain with: make ZERORISCY=1

Microriscy. Compile the toolchain with: make MICRORISCY=1

Usage

The toolchain can be used as the standard RISCV toolchain except that one of the following option must be used:

Riscy: -march=IMXpulpv2

Riscy_fpu: -march=IMFDXpulpv2 -mhard-float

Zeroriscy. Compile the toolchain with: -march=IM

Microriscy. Compile the toolchain with: -march=I -m16r

This option will select the appropriate set of instructions to be used, thus no other RISCV option is needed.
