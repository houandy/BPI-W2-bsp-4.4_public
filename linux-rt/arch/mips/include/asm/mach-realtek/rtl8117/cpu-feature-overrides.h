/*
 * Realtek RTL8168FP specific CPU feature overrides
 *
 * Copyright (C) 2016 Phinex Hung <phinexhung@realtek.com>
 *
 * This file was derived from: include/asm-mips/cpu-features.h
 *	Copyright (C) 2003, 2004 Ralf Baechle
 *	Copyright (C) 2004 Maciej W. Rozycki
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */
#ifndef __ASM_MACH_RTL8117_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_RTL8117_CPU_FEATURE_OVERRIDES_H


#define cpu_has_tlb			1
#define cpu_has_mips16			1
#define cpu_has_mmips			0
#define cpu_has_tlbinv			0
#define cpu_has_segment			0
#define cpu_has_eva			0
#define cpu_has_ftlb			0
#define cpu_has_htw			0
#define cpu_has_rixiex			0
#define cpu_has_maar			0
#define cpu_has_xpa			0
#define cpu_has_fre			0
#define cpu_has_cdmm			0
#define cpu_has_bp_ghist		0
#define cpu_has_sleep			1
#define cpu_has_3kex			1
#define cpu_has_4kex			0
#define cpu_has_3k_cache		1
#define cpu_has_4k_cache		0
#define cpu_has_6k_cache		0
#define cpu_has_8k_cache		0
#define cpu_has_tx39_cache		0
#define cpu_has_octeon_cache		0
#define cpu_has_divec			0
#define cpu_has_vce			0
#define cpu_has_prefetch		0
#define cpu_has_tr			0
#define cpu_has_mcheck			0
#define cpu_has_mips_2			0
#define cpu_has_mips_3			0
#define cpu_has_mips_4			0
#define cpu_has_mips_5			0
#define cpu_has_mips32r1		0
#define cpu_has_mips32r2		0
#define cpu_has_mips64r1		0
#define cpu_has_mips64r2		0
#define cpu_has_dc_aliases		0
#define cpu_has_pindexed_dcache		1
#define cpu_has_local_ebase		0
#define cpu_has_llsc			1
#define cpu_has_ic_fills_f_dc		0
#define cpu_has_vtag_icache		0
#define cpu_has_cache_cdex_p 		0
#define cpu_has_cache_cdex_s 		0

#define cpu_has_fpu			0
#define cpu_has_32fpr			0
#define cpu_has_nofpuex			1

#define cpu_has_dsp			0

#define cpu_has_ejtag			0
#define cpu_has_rixi			0
#define cpu_has_smartmips		0
#define cpu_has_mips3d			0
#define cpu_has_mdmx			0

#define cpu_has_mipsmt			0

#define cpu_has_userlocal		1

#define cpu_has_counter			1

#ifdef CONFIG_HARDWARE_WATCHPOINTS
#define cpu_has_watch			1
#else
#define cpu_has_watch			0
#endif


#define cpu_has_64bits			0
#define cpu_has_64bits_zero_reg		0
#define cpu_has_64bits_gp_regs		0
#define cpu_has_64bit_addresses		0
#define cpu_has_vz			0

#define cpu_has_sync			1

#define cpu_icache_size			(64 << 10)
#define cpu_dcache_size			(64 << 10)
#define cpu_scache_size			0
#define cpu_icache_line			32
#define cpu_dcache_line			32
#define cpu_scache_line			0
#define cpu_tlb_entry			128
#define cpu_mem_size			(16 << 20)
#define cpu_imem_size			256
#define cpu_dmem_size			256
#define cpu_smem_size			0


#ifdef cpu_dcache_line
#define cpu_dcache_line_size()		cpu_dcache_line
#endif

#ifdef cpu_icache_line
#define cpu_icache_line_size()		cpu_icache_line
#endif

#ifdef cpu_scache_line
#define cpu_scache_line_size()		cpu_scache_line
#endif

#endif /* __ASM_MACH_RTL8117_CPU_FEATURE_OVERRIDES_H */
