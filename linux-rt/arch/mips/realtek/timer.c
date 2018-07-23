/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Copyright (C) 2015 Nikolay Martynov <mar.kolya@gmail.com>
 * Copyright (C) 2015 John Crispin <blogic@openwrt.org>
 * Copyright (C) 2016 Realtek Semiconductor Corp.
 * Author: Phinex Hung <phinexhung@realtek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/of.h>

#include <asm/time.h>


void __init plat_time_init(void)
{

	of_clk_init(NULL);
	clocksource_probe();
}
