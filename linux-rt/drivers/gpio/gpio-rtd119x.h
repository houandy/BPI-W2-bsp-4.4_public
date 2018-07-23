/*
* Realtek GPIO Driver
*
* Copyright(c) 2015 Realtek Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

//#ifndef	__RTK119X_RTK119X_GPIO_H
//#define	__RTK119X_RTK119X_GPIO_H

#include <linux/io.h>
#include <linux/spinlock.h>
#include <asm-generic/gpio.h>


//#define RTK_DEBUG
#ifdef RTK_DEBUG
#define RTK_debug(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define RTK_debug(fmt, ...)
#endif

#define GP_HIGH		1
#define GP_LOW		0
#define GP_DIROUT	1
#define GP_DIRIN	0

typedef enum {
    RTK119X_GPIO_DEBOUNCE_37ns = 0,
    RTK119X_GPIO_DEBOUNCE_1us,
    RTK119X_GPIO_DEBOUNCE_10us,
    RTK119X_GPIO_DEBOUNCE_100us,
    RTK119X_GPIO_DEBOUNCE_1ms,
    RTK119X_GPIO_DEBOUNCE_10ms,
    RTK119X_GPIO_DEBOUNCE_20ms,
    RTK119X_GPIO_DEBOUNCE_30ms,
}RTK119X_GPIO_DEBOUNCE;



struct rtk119x_gpio_groups {
	const char		*group_name;
	u32		linux_irq_base;
	void __iomem		*irq_membase;
	u32 		gpio_isr_deassert_offset;
	u32 		gpio_isr_assert_offset;
	u32 		reg_isr_off;
	u32 		reg_umsk_isr_gpa_off;
	u32 		reg_umsk_isr_gpda_off;
	void __iomem		*gpio_membase;
	u32		mem_size;
	u32 		reg_dir_off;
	u32 		reg_dato_off;
	u32 		reg_dati_off;
	u32 		reg_ie_off;
	u32 		reg_dp_off;
	u32 		reg_deb_off;
};

struct rtk119x_gpio_controller {
	struct gpio_chip	chip;
	struct irq_chip	gp_irq_chip;
	unsigned int				bank_deassert_irq;
	unsigned int				bank_assert_irq;
	struct irq_domain *irq_mux_domain;
	spinlock_t		lock;
	u32		gpio_isr_deassert_offset;
	u32		gpio_isr_assert_offset;
	long unsigned int		gpio_isr_deassert_enable_flag[10];
	long unsigned int		gpio_isr_assert_enable_flag[10];
	u32		linux_irq_base;
	void __iomem		*reg_isr;
	void __iomem		*regs_umsk_isr_gpa;
	void __iomem		*regs_umsk_isr_gpda;
	void __iomem 	*reg_dir;
	void __iomem 	*reg_dato;
	void __iomem		*reg_dati;
	void __iomem 	*reg_ie;
	void __iomem 	*reg_dp;
	void __iomem 	*reg_deb;

};

//#endif	/* __RTK119X_RTK119X_GPIO_H */
