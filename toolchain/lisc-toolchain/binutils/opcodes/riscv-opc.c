/* RISC-V opcode list
   Copyright 2011-2015 Free Software Foundation, Inc.

   Contributed by Andrew Waterman (waterman@cs.berkeley.edu) at UC Berkeley.
   Based on MIPS target.

   PULP family support contributed by Eric Flamand (eflamand@iis.ee.ethz.ch) at ETH-Zurich

   This file is part of the GNU opcodes library.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */

#include "sysdep.h"
#include "opcode/riscv.h"
#include <stdio.h>

/* Register names used by gas and objdump.  */

const char * const riscv_gpr_names_numeric[NGPR] =
{
  "x0",   "x1",   "x2",   "x3",   "x4",   "x5",   "x6",   "x7",
  "x8",   "x9",   "x10",  "x11",  "x12",  "x13",  "x14",  "x15",
  "x16",  "x17",  "x18",  "x19",  "x20",  "x21",  "x22",  "x23",
  "x24",  "x25",  "x26",  "x27",  "x28",  "x29",  "x30",  "x31"
};

const char * const riscv_gpr_names_abi[NGPR] = {
  "zero", "ra", "sp",  "gp",  "tp", "t0",  "t1",  "t2",
  "s0",   "s1", "a0",  "a1",  "a2", "a3",  "a4",  "a5",
  "a6",   "a7", "s2",  "s3",  "s4", "s5",  "s6",  "s7",
  "s8",   "s9", "s10", "s11", "t3", "t4",  "t5",  "t6"
};

const char * const riscv_fpr_names_numeric[NFPR] =
{
  "f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7",
  "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15",
  "f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",
  "f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31"
};

const char * const riscv_fpr_names_abi[NFPR] = {
  "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
  "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
  "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
  "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
};

/* The order of overloaded instructions matters.  Label arguments and
   register arguments look the same. Instructions that can have either
   for arguments must apear in the correct order in this table for the
   assembler to pick the right one. In other words, entries with
   immediate operands must apear after the same instruction with
   registers.

   Because of the lookup algorithm used, entries with the same opcode
   name must be contiguous.  */

#define WR_xd INSN_WRITE_GPR_D
#define WR_fd INSN_WRITE_FPR_D
#define RD_xs1 INSN_READ_GPR_S
#define RD_xs2 INSN_READ_GPR_T
#define RD_xs3 INSN_READ_GPR_R
#define RD_fs1 INSN_READ_FPR_S
#define RD_fs2 INSN_READ_FPR_T
#define RD_fs3 INSN_READ_FPR_R

#define MASK_RS1 (OP_MASK_RS1 << OP_SH_RS1)
#define MASK_RS2 (OP_MASK_RS2 << OP_SH_RS2)
#define MASK_RD (OP_MASK_RD << OP_SH_RD)
#define MASK_CRS2 (OP_MASK_CRS2 << OP_SH_CRS2)
#define MASK_IMM ENCODE_ITYPE_IMM(-1U)
#define MASK_RVC_IMM ENCODE_RVC_IMM(-1U)
#define MASK_UIMM ENCODE_UTYPE_IMM(-1U)
#define MASK_RM (OP_MASK_RM << OP_SH_RM)
#define MASK_PRED (OP_MASK_PRED << OP_SH_PRED)
#define MASK_SUCC (OP_MASK_SUCC << OP_SH_SUCC)
#define MASK_AQ (OP_MASK_AQ << OP_SH_AQ)
#define MASK_RL (OP_MASK_RL << OP_SH_RL)
#define MASK_AQRL (MASK_AQ | MASK_RL)
#define MASK_CLIP3 ((unsigned long)OP_MASK_CLIP3 << OP_SH_CLIP3)
#define MASK_CLIP4 ((unsigned long)OP_MASK_CLIP4 << OP_SH_CLIP4)
#define MASK_SHAMT3 (OP_MASK_SHAMT3 << OP_SH_SHAMT3)
#define MASK_C_11 (OP_MASK_C_11 << OP_SH_C_11) /* lisc mask for 16 reg */
#define MASK_C_6 (OP_MASK_C_6 << OP_SH_C_6) /* lisc mask for 16 reg */
#define MASK_CRD (OP_MASK_CRD << OP_SH_CRD)

static int match_opcode(const struct riscv_opcode *op, insn_t insn)
{
  return ((insn ^ op->match) & op->mask) == 0;
}

static int match_never(const struct riscv_opcode *op ATTRIBUTE_UNUSED,
		       insn_t insn ATTRIBUTE_UNUSED)
{
  return 0;
}

static int match_rs1_eq_rs2(const struct riscv_opcode *op, insn_t insn)
{
  int rs1 = (insn & MASK_RS1) >> OP_SH_RS1;
  int rs2 = (insn & MASK_RS2) >> OP_SH_RS2;
  return match_opcode (op, insn) && rs1 == rs2;
}

static int match_crd_nonzero(const struct riscv_opcode *op, insn_t insn)
{
  return match_opcode (op, insn) && ((insn & MASK_CRD) != 0);
}

static int match_c_add(const struct riscv_opcode *op, insn_t insn)
{
  return match_crd_nonzero (op, insn) && ((insn & MASK_CRS2) != 0);
}

static int match_c_lui(const struct riscv_opcode *op, insn_t insn)
{
  return match_crd_nonzero (op, insn) && (((insn & MASK_CRD) >> OP_SH_CRD) != 2);
}

const struct riscv_opcode riscv_builtin_opcodes[] =
{
/* name,      isa,   operands, match, mask, match_func, pinfo */
{"unimp",     "C",   "",  0, 0xffffU,  match_opcode, 0 },
{"unimp",     "I",   "",  	MATCH_CSRRW | (CSR_CYCLE << OP_SH_CSR), 0xffffffffU,  match_opcode, 0 }, /* csrw cycle, x0 */
{"ebreak",    "C",   "",  MATCH_C_EBREAK, MASK_C_EBREAK, match_opcode, INSN_ALIAS },
{"ebreak",    "I",   "",    	MATCH_EBREAK, MASK_EBREAK, match_opcode,   0 },
{"sbreak",    "C",   "",  MATCH_C_EBREAK, MASK_C_EBREAK, match_opcode, INSN_ALIAS },
{"sbreak",    "I",   "",    	MATCH_EBREAK, MASK_EBREAK, match_opcode,   INSN_ALIAS },
{"ret",       "C",   "",  MATCH_C_JR | (X_RA << OP_SH_CRD), MASK_C_JR | MASK_CRD | MASK_C_11, match_opcode, INSN_ALIAS },
{"ret",       "I",   "",  	MATCH_JR | (X_RA << OP_SH_RS1), MASK_JALR | MASK_RD | MASK_RS1 | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1 },
{"jr",        "C",   "Cd",  MATCH_C_JR, MASK_C_JR | MASK_C_11 , match_crd_nonzero, INSN_ALIAS },
{"jr",        "I",   "s",  	MATCH_JR, MASK_JR | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1 },
{"jr",        "I",   "s,j",  	MATCH_JR, MASK_JR | MASK_CLIP3, match_opcode,   RD_xs1 },
{"jalr",      "C",   "Cd",  MATCH_C_JALR, MASK_C_JALR | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"jalr",      "I",   "s",  	MATCH_JALR | (X_RA << OP_SH_RD), MASK_JALR | MASK_RD | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"jalr",      "I",   "s,j",  	MATCH_JALR | (X_RA << OP_SH_RD), MASK_JALR | MASK_RD | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"jalr",      "I",   "d,s",  	MATCH_JALR, MASK_JALR | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"jalr",      "I",   "d,s,j",   MATCH_JALR, MASK_JALR | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"j",         "C",   "Ca",  MATCH_C_J, MASK_C_J, match_opcode, INSN_ALIAS },
{"j",         "I",   "a",  	MATCH_J, MASK_J | MASK_CLIP3, match_opcode,   0 },
{"jal",       "32C", "Ca",  MATCH_C_JAL, MASK_C_JAL, match_opcode, INSN_ALIAS },
{"jal",       "I",   "a",  	MATCH_JAL, MASK_JAL | MASK_CLIP3, match_opcode,   WR_xd },
{"call",      "I",   "c", 	(X_T1 << OP_SH_RS1) | (X_RA << OP_SH_RD), (int) M_CALL,  match_never, INSN_MACRO },
{"call",      "I",   "d,c", 	(X_T1 << OP_SH_RS1), (int) M_CALL,  match_never, INSN_MACRO },
{"tail",      "I",   "c", 	(X_T1 << OP_SH_RS1), (int) M_CALL,  match_never, INSN_MACRO },
{"jump",      "I",   "c,s", 	0, (int) M_CALL,  match_never, INSN_MACRO },
{"nop",       "C",   "",  MATCH_C_ADDI, 0xffff, match_opcode, INSN_ALIAS },
{"nop",       "I",   "",        MATCH_ADDI, MASK_ADDI | MASK_RD | MASK_RS1 | MASK_IMM | MASK_CLIP3, match_opcode,  INSN_ALIAS },
{"lui",       "C",   "Cd,Cu",  MATCH_C_LUI, MASK_C_LUI | MASK_C_11, match_c_lui, INSN_ALIAS },
{"lui",       "I",   "d,u",  	MATCH_LUI, MASK_LUI | MASK_CLIP3, match_opcode,   WR_xd },
{"li",        "C",   "Cd,Cv",  MATCH_C_LUI, MASK_C_LUI | MASK_C_11, match_c_lui, INSN_ALIAS },
{"li",        "C",   "Cd,Cj",  MATCH_C_LI, MASK_C_LI | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"li",        "C",   "Cd,0",   MATCH_C_LI, MASK_C_LI | MASK_RVC_IMM | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"li",        "I",   "d,j",     MATCH_ADDI, MASK_ADDI | MASK_RS1 | MASK_CLIP3, match_opcode,  INSN_ALIAS|WR_xd }, /* addi */
{"li",        "I",   "d,I",  	0, (int) M_LI,  match_never, INSN_MACRO },
{"mv",        "C",   "Cd,CV",  MATCH_C_MV, MASK_C_MV | MASK_C_11 | MASK_C_6, match_c_add, INSN_ALIAS },
{"mv",        "I",   "d,s",  	MATCH_ADDI, MASK_ADDI | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"move",      "C",   "Cd,CV",  MATCH_C_MV, MASK_C_MV | MASK_C_11 | MASK_C_6, match_c_add, INSN_ALIAS },
{"move",      "I",   "d,s",  	MATCH_ADDI, MASK_ADDI | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"andi",      "C",   "Cs,Cw,Cj",  MATCH_C_ANDI, MASK_C_ANDI, match_opcode, INSN_ALIAS },
{"andi",      "I",   "d,s,j",  MATCH_ANDI, MASK_ANDI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"and",       "C",   "Cs,Cw,Ct",  MATCH_C_AND, MASK_C_AND, match_opcode, INSN_ALIAS },
{"and",       "C",   "Cs,Ct,Cw",  MATCH_C_AND, MASK_C_AND, match_opcode, INSN_ALIAS },
{"and",       "C",   "Cs,Cw,Cj",  MATCH_C_ANDI, MASK_C_ANDI, match_opcode, INSN_ALIAS },
{"and",       "I",   "d,s,t",  MATCH_AND, MASK_AND | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"and",       "I",   "d,s,j",  MATCH_ANDI, MASK_ANDI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"beqz",      "C",   "Cs,Cp",  MATCH_C_BEQZ, MASK_C_BEQZ, match_opcode, INSN_ALIAS },
{"beqz",      "I",   "s,p",  	MATCH_BEQ, MASK_BEQ | MASK_RS2 | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1 },
{"beq",       "I",   "s,t,p",  MATCH_BEQ, MASK_BEQ | MASK_CLIP3, match_opcode,   RD_xs1|RD_xs2 },
{"blez",      "I",   "t,p",  	MATCH_BGE, MASK_BGE | MASK_RS1 | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs2 },
{"bgez",      "I",   "s,p",  	MATCH_BGE, MASK_BGE | MASK_RS2 | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1 },
{"ble",       "I",   "t,s,p",  MATCH_BGE, MASK_BGE | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1|RD_xs2 },
{"bleu",      "I",   "t,s,p",  MATCH_BGEU, MASK_BGEU | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1|RD_xs2 },
{"bge",       "I",   "s,t,p",  MATCH_BGE, MASK_BGE | MASK_CLIP3, match_opcode,   RD_xs1|RD_xs2 },
{"bgeu",      "I",   "s,t,p",  MATCH_BGEU, MASK_BGEU | MASK_CLIP3, match_opcode,   RD_xs1|RD_xs2 },
{"bltz",      "I",   "s,p",  	MATCH_BLT, MASK_BLT | MASK_RS2 | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1 },
{"bgtz",      "I",   "t,p",  	MATCH_BLT, MASK_BLT | MASK_RS1 | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs2 },
{"blt",       "I",   "s,t,p",  MATCH_BLT, MASK_BLT | MASK_CLIP3, match_opcode,   RD_xs1|RD_xs2 },
{"bltu",      "I",   "s,t,p",  MATCH_BLTU, MASK_BLTU | MASK_CLIP3, match_opcode,   RD_xs1|RD_xs2 },
{"bgt",       "I",   "t,s,p",  MATCH_BLT, MASK_BLT | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1|RD_xs2 },
{"bgtu",      "I",   "t,s,p",  MATCH_BLTU, MASK_BLTU | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1|RD_xs2 },
{"bnez",      "C",   "Cs,Cp",  MATCH_C_BNEZ, MASK_C_BNEZ, match_opcode, INSN_ALIAS },
{"bnez",      "I",   "s,p",  	MATCH_BNE, MASK_BNE | MASK_RS2 | MASK_CLIP3, match_opcode,   INSN_ALIAS|RD_xs1 },
{"bne",       "I",   "s,t,p",  MATCH_BNE, MASK_BNE | MASK_CLIP3, match_opcode,   RD_xs1|RD_xs2 },
{"addi",      "C",   "Ct,Cc,CK", MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN, match_opcode, INSN_ALIAS },
{"addi",      "C",   "Cd,CU,Cj",  MATCH_C_ADDI, MASK_C_ADDI | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"addi",      "C",   "Cc,Cc,CL", MATCH_C_ADDI16SP, MASK_C_ADDI16SP, match_opcode, INSN_ALIAS },
{"addi",      "I",   "d,s,j",  MATCH_ADDI, MASK_ADDI | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1 },
{"add",       "C",   "Cd,CU,CV",  MATCH_C_ADD, MASK_C_ADD | MASK_C_11 | MASK_C_6, match_c_add, INSN_ALIAS },
{"add",       "C",   "Cd,CV,CU",  MATCH_C_ADD, MASK_C_ADD | MASK_C_11 | MASK_C_6, match_c_add, INSN_ALIAS },
{"add",       "C",   "Cd,CU,Cj",  MATCH_C_ADDI, MASK_C_ADDI | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"add",       "C",   "Ct,Cc,CK", MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN, match_opcode, INSN_ALIAS },
{"add",       "C",   "Cc,Cc,CL", MATCH_C_ADDI16SP, MASK_C_ADDI16SP, match_opcode, INSN_ALIAS },
{"add",       "I",   "d,s,t",  MATCH_ADD, MASK_ADD | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"add",       "I",   "d,s,t,0",MATCH_ADD, MASK_ADD | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"add",       "I",   "d,s,j",  MATCH_ADDI, MASK_ADDI | MASK_CLIP3, match_opcode,  INSN_ALIAS|WR_xd|RD_xs1 },
{"la",        "I",   "d,A",  	0, (int) M_LA,  match_never, INSN_MACRO },
{"lla",       "I",   "d,A",  	0, (int) M_LLA,  match_never, INSN_MACRO },
{"la.tls.gd", "I",   "d,A",  	0, (int) M_LA_TLS_GD,  match_never, INSN_MACRO },
{"la.tls.ie", "I",   "d,A",  	0, (int) M_LA_TLS_IE,  match_never, INSN_MACRO },
{"neg",       "I",   "d,t",  	MATCH_SUB, MASK_SUB | MASK_RS1 | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs2 }, /* sub 0 */
{"slli",      "C",   "Cd,CU,C>",  MATCH_C_SLLI, MASK_C_SLLI | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"slli",      "I",   "d,s,>",   MATCH_SLLI, MASK_SLLI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"sll",       "C",   "Cd,CU,C>",  MATCH_C_SLLI, MASK_C_SLLI | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"sll",       "I",   "d,s,t",   MATCH_SLL, MASK_SLL | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"sll",       "I",   "d,s,>",   MATCH_SLLI, MASK_SLLI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"srli",      "C",   "Cs,Cw,C>",  MATCH_C_SRLI, MASK_C_SRLI, match_crd_nonzero, INSN_ALIAS },
{"srli",      "I",   "d,s,>",   MATCH_SRLI, MASK_SRLI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"srl",       "C",   "Cs,Cw,C>",  MATCH_C_SRLI, MASK_C_SRLI, match_crd_nonzero, INSN_ALIAS },
{"srl",       "I",   "d,s,t",   MATCH_SRL, MASK_SRL | MASK_CLIP3 , match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"srl",       "I",   "d,s,>",   MATCH_SRLI, MASK_SRLI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"srai",      "C",   "Cs,Cw,C>",  MATCH_C_SRAI, MASK_C_SRAI, match_crd_nonzero, INSN_ALIAS },
{"srai",      "I",   "d,s,>",   MATCH_SRAI, MASK_SRAI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"sra",       "C",   "Cs,Cw,C>",  MATCH_C_SRAI, MASK_C_SRAI, match_crd_nonzero, INSN_ALIAS },
{"sra",       "I",   "d,s,t",   MATCH_SRA, MASK_SRA | MASK_CLIP3 , match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"sra",       "I",   "d,s,>",   MATCH_SRAI, MASK_SRAI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"sub",       "C",   "Cs,Cw,Ct",  MATCH_C_SUB, MASK_C_SUB, match_opcode, INSN_ALIAS },
{"sub",       "I",   "d,s,t",  MATCH_SUB, MASK_SUB | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"lb",        "I",   "d,o(s)",  MATCH_LB, MASK_LB | MASK_CLIP4, match_opcode,   WR_xd|RD_xs1 },
{"lb",        "I",   "d,A",  	0, (int) M_LB, match_never, INSN_MACRO },
{"lbu",       "I",   "d,o(s)",  MATCH_LBU, MASK_LBU | MASK_CLIP4, match_opcode,   WR_xd|RD_xs1 },
{"lbu",       "I",   "d,A",  	0, (int) M_LBU, match_never, INSN_MACRO },
{"lh",        "I",   "d,o(s)",  MATCH_LH, MASK_LH | MASK_CLIP4, match_opcode,   WR_xd|RD_xs1 },
{"lh",        "I",   "d,A",  	0, (int) M_LH, match_never, INSN_MACRO },
{"lhu",       "I",   "d,o(s)",  MATCH_LHU, MASK_LHU | MASK_CLIP4, match_opcode,   WR_xd|RD_xs1 },
{"lhu",       "I",   "d,A",  	0, (int) M_LHU, match_never, INSN_MACRO },
{"lw",        "C",   "Cd,Cm(Cc)",  MATCH_C_LWSP, MASK_C_LWSP | MASK_C_11, match_crd_nonzero, INSN_ALIAS },
{"lw",        "C",   "Ct,Ck(Cs)",  MATCH_C_LW, MASK_C_LW, match_opcode, INSN_ALIAS },
{"lw",        "I",   "d,o(s)",  MATCH_LW, MASK_LW | MASK_CLIP4, match_opcode,   WR_xd|RD_xs1 },
{"lw",        "I",   "d,A",  	0, (int) M_LW, match_never, INSN_MACRO },
{"not",       "I",   "d,s",  	MATCH_XORI | MASK_IMM, MASK_XORI | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"ori",       "I",   "d,s,j",  MATCH_ORI, MASK_ORI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"or",        "C",   "Cs,Cw,Ct",  MATCH_C_OR, MASK_C_OR, match_opcode, INSN_ALIAS },
{"or",        "C",   "Cs,Ct,Cw",  MATCH_C_OR, MASK_C_OR, match_opcode, INSN_ALIAS },
{"or",        "I",   "d,s,t",  MATCH_OR, MASK_OR | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"or",        "I",   "d,s,j",  MATCH_ORI, MASK_ORI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"auipc",     "I",   "d,u",  	MATCH_AUIPC, MASK_AUIPC | MASK_CLIP3, match_opcode,  WR_xd },
{"seqz",      "I",   "d,s",  	MATCH_SLTIU | ENCODE_ITYPE_IMM(1), MASK_SLTIU | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"snez",      "I",   "d,t",  	MATCH_SLTU, MASK_SLTU | MASK_RS1 | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs2 },
{"sltz",      "I",   "d,s",  	MATCH_SLT, MASK_SLT | MASK_RS2 | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"sgtz",      "I",   "d,t",  	MATCH_SLT, MASK_SLT | MASK_RS1 | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs2 },
{"slti",      "I",   "d,s,j",  MATCH_SLTI, MASK_SLTI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"slt",       "I",   "d,s,t",  MATCH_SLT, MASK_SLT | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"slt",       "I",   "d,s,j",  MATCH_SLTI, MASK_SLTI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"sltiu",     "I",   "d,s,j",  MATCH_SLTIU, MASK_SLTIU | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"sltu",      "I",   "d,s,t",  MATCH_SLTU, MASK_SLTU | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"sltu",      "I",   "d,s,j",  MATCH_SLTIU, MASK_SLTIU | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },
{"sgt",       "I",   "d,t,s",  MATCH_SLT, MASK_SLT | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1|RD_xs2 },
{"sgtu",      "I",   "d,t,s",  MATCH_SLTU, MASK_SLTU | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1|RD_xs2 },
{"sb",        "I",   "t,q(s)",  MATCH_SB, MASK_SB | MASK_CLIP4, match_opcode,   RD_xs1|RD_xs2 },
{"sb",        "I",   "t,A,s",  0, (int) M_SB, match_never,  INSN_MACRO },
{"sh",        "I",   "t,q(s)",  MATCH_SH, MASK_SH | MASK_CLIP4, match_opcode,   RD_xs1|RD_xs2 },
{"sh",        "I",   "t,A,s",  0, (int) M_SH, match_never,  INSN_MACRO },
{"sw",        "C",   "CV,CM(Cc)",  MATCH_C_SWSP, MASK_C_SWSP | MASK_C_6, match_opcode, INSN_ALIAS },
{"sw",        "C",   "Ct,Ck(Cs)",  MATCH_C_SW, MASK_C_SW, match_opcode, INSN_ALIAS },
{"sw",        "I",   "t,q(s)",  MATCH_SW, MASK_SW | MASK_CLIP4, match_opcode,   RD_xs1|RD_xs2 },
{"sw",        "I",   "t,A,s",  0, (int) M_SW, match_never,  INSN_MACRO },
{"fence",     "I",   "",  	MATCH_FENCE | MASK_PRED | MASK_SUCC, MASK_FENCE | MASK_RD | MASK_RS1 | MASK_IMM | MASK_CLIP3, match_opcode,   INSN_ALIAS },
{"fence",     "I",   "P,Q",  	MATCH_FENCE, MASK_FENCE | MASK_RD | MASK_RS1 | (MASK_IMM & ~MASK_PRED & ~MASK_SUCC) | MASK_CLIP3, match_opcode,   0 },
{"fence.i",   "I",   "",  	MATCH_FENCE_I, MASK_FENCE | MASK_RD | MASK_RS1 | MASK_IMM | MASK_CLIP3, match_opcode,   0 },
{"rdcycle",   "I",   "d",  	MATCH_RDCYCLE, MASK_RDCYCLE, match_opcode,  WR_xd },
{"rdinstret", "I",   "d",  	MATCH_RDINSTRET, MASK_RDINSTRET, match_opcode,  WR_xd },
{"rdtime",    "I",   "d",  	MATCH_RDTIME, MASK_RDTIME, match_opcode,  WR_xd },
{"rdcycleh",  "32I", "d",  	MATCH_RDCYCLEH, MASK_RDCYCLEH, match_opcode,  WR_xd },
{"rdinstreth","32I", "d",  	MATCH_RDINSTRETH, MASK_RDINSTRETH, match_opcode,  WR_xd },
{"rdtimeh",   "32I", "d",  	MATCH_RDTIMEH, MASK_RDTIMEH, match_opcode,  WR_xd },
{"ecall",     "I",   "",    	MATCH_SCALL, MASK_SCALL, match_opcode,   0 },
{"scall",     "I",   "",   	MATCH_SCALL, MASK_SCALL, match_opcode,   0 },
{"scallimm",  "I",   "b3",   	MATCH_SCALL, MASK_SCALL_IMM, match_opcode,   0 },
{"xori",      "I",   "d,s,j",  MATCH_XORI, MASK_XORI | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1 },
{"xor",       "C",   "Cs,Cw,Ct",  MATCH_C_XOR, MASK_C_XOR, match_opcode, INSN_ALIAS },
{"xor",       "C",   "Cs,Ct,Cw",  MATCH_C_XOR, MASK_C_XOR, match_opcode, INSN_ALIAS },
{"xor",       "I",   "d,s,t",  MATCH_XOR, MASK_XOR | MASK_CLIP3, match_opcode,   WR_xd|RD_xs1|RD_xs2 },
{"xor",       "I",   "d,s,j",  MATCH_XORI, MASK_XORI | MASK_CLIP3, match_opcode,   INSN_ALIAS|WR_xd|RD_xs1 },

/* Supervisor instructions */
{"csrr",      "I",   "d,E",  MATCH_CSRRS, MASK_CSRRS | MASK_RS1, match_opcode,  WR_xd },
{"csrwi",     "I",   "E,b3",  MATCH_CSRRWI, MASK_CSRRWI | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrw",      "I",   "E,s",  MATCH_CSRRW, MASK_CSRRW | MASK_RD, match_opcode,  RD_xs1 },
{"csrw",      "I",   "E,b3",  MATCH_CSRRWI, MASK_CSRRWI | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrsi",     "I",   "E,b3",  MATCH_CSRRSI, MASK_CSRRSI | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrs",      "I",   "E,s",  MATCH_CSRRS, MASK_CSRRS | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrs",      "I",   "E,b3",  MATCH_CSRRSI, MASK_CSRRSI | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrci",     "I",   "E,b3",  MATCH_CSRRCI, MASK_CSRRCI | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrc",      "I",   "E,s",  MATCH_CSRRC, MASK_CSRRC | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrc",      "I",   "E,b3",  MATCH_CSRRCI, MASK_CSRRCI | MASK_RD, match_opcode,  WR_xd|RD_xs1 },
{"csrrw",     "I",   "d,E,s",  MATCH_CSRRW, MASK_CSRRW, match_opcode,  WR_xd|RD_xs1 },
{"csrrw",     "I",   "d,E,b3",  MATCH_CSRRWI, MASK_CSRRWI, match_opcode,  WR_xd|RD_xs1 },
{"csrrs",     "I",   "d,E,s",  MATCH_CSRRS, MASK_CSRRS, match_opcode,  WR_xd|RD_xs1 },
{"csrrs",     "I",   "d,E,b3",  MATCH_CSRRSI, MASK_CSRRSI, match_opcode,  WR_xd|RD_xs1 },
{"csrrc",     "I",   "d,E,s",  MATCH_CSRRC, MASK_CSRRC, match_opcode,  WR_xd|RD_xs1 },
{"csrrc",     "I",   "d,E,b3",  MATCH_CSRRCI, MASK_CSRRCI, match_opcode,  WR_xd|RD_xs1 },
{"csrrwi",    "I",   "d,E,b3",  MATCH_CSRRWI, MASK_CSRRWI, match_opcode,  WR_xd|RD_xs1 },
{"csrrsi",    "I",   "d,E,b3",  MATCH_CSRRSI, MASK_CSRRSI, match_opcode,  WR_xd|RD_xs1 },
{"csrrci",    "I",   "d,E,b3",  MATCH_CSRRCI, MASK_CSRRCI, match_opcode,  WR_xd|RD_xs1 },

/* V1.9 Supervisor Instructions */
{"mret",      "I",   "",     MATCH_MRET, MASK_MRET, match_opcode,  0 },
{"wfi",       "I",   "",     MATCH_WFI, MASK_WFI, match_opcode,  0 },

/* Multiply/Divide instruction subset */
{"mul",       "M",   "d,s,t",  MATCH_MUL, MASK_MUL | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"mulh",      "M",   "d,s,t",  MATCH_MULH, MASK_MULH | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"mulhu",     "M",   "d,s,t",  MATCH_MULHU, MASK_MULHU | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"mulhsu",    "M",   "d,s,t",  MATCH_MULHSU, MASK_MULHSU | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"div",       "M",   "d,s,t",  MATCH_DIV, MASK_DIV | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"divu",      "M",   "d,s,t",  MATCH_DIVU, MASK_DIVU | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"rem",       "M",   "d,s,t",  MATCH_REM, MASK_REM | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },
{"remu",      "M",   "d,s,t",  MATCH_REMU, MASK_REMU | MASK_CLIP3, match_opcode,  WR_xd|RD_xs1|RD_xs2 },

/* Compressed instructions */

{"c.ebreak",  "C",   "",  MATCH_C_EBREAK, MASK_C_EBREAK, match_opcode, 0 },
{"c.jr",      "C",   "Cd",  MATCH_C_JR, MASK_C_JR | MASK_C_11 , match_crd_nonzero, 0 },
{"c.jalr",    "C",   "Cd",  MATCH_C_JALR, MASK_C_JALR | MASK_C_11, match_crd_nonzero, 0 },
{"c.j",       "C",   "Ca",  MATCH_C_J, MASK_C_J, match_opcode, 0 },
{"c.jal",     "32C", "Ca",  MATCH_C_JAL, MASK_C_JAL, match_opcode, 0 },
{"c.nop",     "C",   "",  MATCH_C_ADDI, 0xffff, match_opcode, 0 },
{"c.lui",     "C",   "Cd,Cu",  MATCH_C_LUI, MASK_C_LUI | MASK_C_11, match_c_lui, 0 },
{"c.li",      "C",   "Cd,Cj",  MATCH_C_LI, MASK_C_LI | MASK_C_11, match_crd_nonzero, 0 },
{"c.mv",      "C",   "Cd,CV",  MATCH_C_MV, MASK_C_MV | MASK_C_11 | MASK_C_6, match_c_add, 0 },
{"c.andi",    "C",   "Cs,Cj",     MATCH_C_ANDI, MASK_C_ANDI, match_opcode, 0 },
{"c.and",     "C",   "Cs,Ct",     MATCH_C_AND, MASK_C_AND, match_opcode, 0 },
{"c.beqz",    "C",   "Cs,Cp",  MATCH_C_BEQZ, MASK_C_BEQZ, match_opcode, 0 },
{"c.bnez",    "C",   "Cs,Cp",  MATCH_C_BNEZ, MASK_C_BNEZ, match_opcode, 0 },
{"c.addi4spn","C",   "Ct,Cc,CK", MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN, match_opcode, 0 },
{"c.addi",    "C",   "Cd,Cj",     MATCH_C_ADDI, MASK_C_ADDI | MASK_C_11, match_crd_nonzero, 0 },
{"c.addi16sp","C",   "Cc,CL",    MATCH_C_ADDI16SP, MASK_C_ADDI16SP, match_opcode, 0 },
{"c.add",     "C",   "Cd,CV",     MATCH_C_ADD, MASK_C_ADD | MASK_C_11 | MASK_C_6, match_c_add, 0 },
{"c.slli",    "C",   "Cd,C>",     MATCH_C_SLLI, MASK_C_SLLI | MASK_C_11, match_crd_nonzero, 0 },
{"c.srli",    "C",   "Cs,C>",     MATCH_C_SRLI, MASK_C_SRLI, match_opcode, 0 },
{"c.srai",    "C",   "Cs,C>",     MATCH_C_SRAI, MASK_C_SRAI, match_opcode, 0 },
{"c.sub",     "C",   "Cs,Ct",     MATCH_C_SUB, MASK_C_SUB, match_opcode, 0 },
{"c.lwsp",    "C",   "Cd,Cm(Cc)",  MATCH_C_LWSP, MASK_C_LWSP | MASK_C_11, match_crd_nonzero, 0 },
{"c.lw",      "C",   "Ct,Ck(Cs)",  MATCH_C_LW, MASK_C_LW, match_opcode, 0 },
{"c.or",      "C",   "Cs,Ct",     MATCH_C_OR, MASK_C_OR, match_opcode, 0 },
{"c.swsp",    "C",   "CV,CM(Cc)",  MATCH_C_SWSP, MASK_C_SWSP | MASK_C_6, match_opcode, 0 },
{"c.sw",      "C",   "Ct,Ck(Cs)",  MATCH_C_SW, MASK_C_SW, match_opcode, 0 },
{"c.xor",     "C",   "Cs,Ct",     MATCH_C_XOR, MASK_C_XOR, match_opcode, 0 },

};

#define RISCV_NUM_OPCODES \
  ((sizeof riscv_builtin_opcodes) / (sizeof (riscv_builtin_opcodes[0])))
const int bfd_riscv_num_builtin_opcodes = RISCV_NUM_OPCODES;

/* Removed const from the following to allow for dynamic extensions to the
   built-in instruction set.  */
struct riscv_opcode *riscv_opcodes =
  (struct riscv_opcode *) riscv_builtin_opcodes;
int bfd_riscv_num_opcodes = RISCV_NUM_OPCODES;
#undef RISCV_NUM_OPCODES
