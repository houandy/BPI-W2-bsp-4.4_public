/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2016 Realtek Semiconductor Corp.
 * Author: Phinex Hung <phinexhung@realtek.com>
 */
#ifndef __ASM_MACH_REALTEK_IRQ_H__
#define  __ASM_MACH_REALTEK_IRQ_H__

extern void mips_vec_irq_init(void);
extern int __init mips_vec_irq_of_init(struct device_node *of_node,
                                struct device_node *parent);


#ifndef MIPS_CPU_IRQ_BASE
#define MIPS_CPU_IRQ_BASE 0
#endif


#ifdef CONFIG_IRQ_VEC
#define MIPS_VEC_IRQ_BASE	(MIPS_CPU_IRQ_BASE + 8)
#endif

#ifndef NR_IRQS
#define NR_IRQS 16
#endif

#endif /* __ASM_MACH_REALTEK_IRQ_H__ */
