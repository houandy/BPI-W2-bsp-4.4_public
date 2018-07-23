/*
 * Realtek RTL8117 platform setup
 *
 * Copyright (C) 2016-2017 Realtek Semiconductor Corp.
 * Author: Phinex Hung <phinexhung@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mutex.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/prom.h>
#include <asm/traps.h>
#include <asm/fw/fw.h>
#include <asm/reboot.h>

#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/libfdt.h>
#include <linux/delay.h>
#include <rtl8117_platform.h>

struct mutex rtl8117_mutex;

extern const char __appended_dtb;
extern const char __dtb_rtl8117_begin;

//en_byte: enable byte for read/write
//mode: RISC_WRITE_OP or RISC_READ_OP
//data: for read or write
//offset: register offset
void access_RISC_reg(u8 offset, volatile u32 *data, u8 en_byte, u32 mode)
{
	u8 dcount = 0;

	if(mode == RISC_WRITE_OP)
		writel(*data, CPU2_BASE_ADDR + CPU2_RISC_DATA);

	writel((mode | offset | ( (en_byte & 0xf) << 16 ) ), CPU2_BASE_ADDR + CPU2_RISC_CMD);

	while((readl(CPU2_BASE_ADDR + CPU2_RISC_CMD) & mode))
	{
		mdelay(100);
		if(++dcount == 20)
			pr_emerg("Access RISC reg fail\n");
	}

	if(mode == RISC_READ_OP)
		*data = readl(CPU2_BASE_ADDR + CPU2_RISC_DATA);

}
EXPORT_SYMBOL_GPL(access_RISC_reg);

void OOB_access_IB_reg(u16 offset, volatile u32 *data, u8 en_byte, u32 mode)
{
	u32 dcount = 0 ;

	mutex_lock(&rtl8117_mutex);

	if(mode == OOB_WRITE_OP)
    		writel(*data, OOB_MAC_BASE_ADDR + OOB_MAC_OCP_DATA);
	
	writel( (mode | offset | ((en_byte & 0xf) << 16)), OOB_MAC_BASE_ADDR + OOB_MAC_OCP_ADDR);
	
	while((readl(OOB_MAC_BASE_ADDR + OOB_MAC_OCP_ADDR) & 0x80000000))
	{
		udelay(10);
		if(++dcount == 200000)
			pr_emerg("OOB Access IB fail\n");
	}

	//delay maximal 100ms*20 = 2 seconds

	if(mode == OOB_READ_OP)
		*data = readl(OOB_MAC_BASE_ADDR + OOB_MAC_OCP_DATA);

	mutex_unlock(&rtl8117_mutex);
}
EXPORT_SYMBOL_GPL(OOB_access_IB_reg);


void rtl8117_cpu_reset(void)
{
	volatile u32 temp;

	//disable DCO clock and
	OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_READ_OP);
	temp &= ~(1 << 10);
	OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_WRITE_OP);

	udelay(10);

	local_irq_disable();
	temp = readl(CPU1_BASE_ADDR + CPU1_CTRL_REG);
	temp &= ~((0x07)|(1<<11));
	temp |= (1<<6);
	writel(temp, CPU1_BASE_ADDR + CPU1_CTRL_REG);

	writel(0x0, OOB_MAC_BASE_ADDR + GPIO_CTRL2_SET);

	//cpu sw reset
	access_RISC_reg(CPU1_CTRL_REG, &temp, 0x1, RISC_READ_OP);
	temp &= ~(1 << 3);
	access_RISC_reg(CPU1_CTRL_REG, &temp, 0x1, RISC_WRITE_OP);

	access_RISC_reg(CPU1_CTRL_REG, &temp, 0x1, RISC_READ_OP);
	temp |= (1 << 3);
	access_RISC_reg(CPU1_CTRL_REG, &temp, 0x1, RISC_WRITE_OP);
}

static void rtl8117_restart(char *command)
{
	while(1)
		rtl8117_cpu_reset();
}

static void rtk_imem_setup(void)
{
	unsigned int cctl0;

	//turn on IMEM Load/Store for traps
	cctl0 = read_c0_cctl0();
	cctl0 &= ~(CCTL_IMEM0LDSTLON );
	write_c0_cctl0(cctl0);

	cctl0 = read_c0_cctl0();
	cctl0 |= (CCTL_IMEM0LDSTLON);
	write_c0_cctl0(cctl0);
}


const char *get_system_type(void)
{
	return "RTL8117";
}

void __init plat_mem_setup(void)
{
	if (fw_arg0 != -2)
		panic("Device-tree not present");

	__dt_setup_arch((void *)fw_arg1);


	board_ebase_setup = &rtk_imem_setup;
	_machine_restart = rtl8117_restart;

}

void __init prom_init(void)
{
	mutex_init(&rtl8117_mutex);

        if (strstr(arcs_cmdline, "console=") == NULL)
		strcat(arcs_cmdline, " console=ttyS0,115200");

        mips_set_machine_name("RTL8117 Embedded Linux Platform");

	setup_8250_early_printk_port(CKSEG1ADDR(BSP_UART_VADDR), 2, 0);

}

void __init prom_free_prom_memory(void)
{
}

void __init device_tree_init(void)
{
	if (!initial_boot_params)
		return;

	unflatten_and_copy_device_tree();
}


static int __init plat_of_setup(void)
{
	if (!of_have_populated_dt())
		panic("Device tree not present");

	if (of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL))
		panic("Failed to populate DT");

	return 0;
}
arch_initcall(plat_of_setup);
