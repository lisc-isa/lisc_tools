reg_t tmp = npc;
set_pc( RS1 + insn.i_imm() );
WRITE_RD(tmp >> 1 << 1);
