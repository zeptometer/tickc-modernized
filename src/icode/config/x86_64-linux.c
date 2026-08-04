bvdt i_ireg_tmp[] = { 0 };
bvdt i_ireg_var[] = { R(__RBX) | R(__R12) | R(__R13) | R(__R14) | R(__R15) };
bvdt i_freg_tmp[] = { R(__XMM0) | R(__XMM1) };
bvdt i_freg_var[] = { R(__XMM2) | R(__XMM3) | R(__XMM4) | R(__XMM5) |
	R(__XMM6) | R(__XMM7) | R(__XMM8) | R(__XMM9) | R(__XMM10) |
	R(__XMM11) | R(__XMM12) | R(__XMM13) };

bvdt i_ireg_alltmp[] = { R(__RAX) | R(__R10) | R(__R11) };
bvdt i_freg_alltmp[] = { R(__XMM0) | R(__XMM1) };
