reg_t tmp = npc;
set_pc(JUMP_TARGET);
WRITE_REG( 1 , tmp >> 1 << 1);
