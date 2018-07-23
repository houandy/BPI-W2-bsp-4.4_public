/*
 *  arch/arm/mach-rtk119x/include/mach/memory.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define RTK_FLAG_NONCACHED      (1U << 0)
#define RTK_FLAG_SCPUACC        (1U << 1)
#define RTK_FLAG_ACPUACC        (1U << 2)
#define RTK_FLAG_HWIPACC        (1U << 3)
#define RTK_FLAG_DEAULT         (RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC | RTK_FLAG_HWIPACC)


#define SYS_BOOTCODE_MEMBASE		(PLAT_PHYS_OFFSET)
#define SYS_BOOTCODE_MEMSIZE		(0x0000C000)

#define PLAT_AUDIO_BASE_PHYS		(0x01b00000)
#define PLAT_AUDIO_SIZE			(0x00400000)

#define RPC_RINGBUF_PHYS		(0x01ffe000)
#define RPC_RINGBUF_VIRT		(0xFC7F8000+0x00004000)
#define RPC_RINGBUF_SIZE		(0x00004000)

#define RBUS_BASE_PHYS			(0x18000000)
#define RBUS_BASE_VIRT			(0xFE000000)
#define RBUS_BASE_SIZE			(0x00070000)

#define PLAT_NOR_BASE_PHYS		(0x18100000)
#define PLAT_NOR_SIZE			(0x01000000)

#define PLAT_SECURE_BASE_PHYS		(0x10000000)
#define PLAT_SECURE_SIZE		(0x00100000)

#ifdef CONFIG_ARM_NORMAL_WORLD_OS
#define PLAT_SECUREOS_BASE_PHYS		(PLAT_SECURE_BASE_PHYS + PLAT_SECURE_SIZE)
#define PLAT_SECUREOS_SIZE		(0x00a00000)
#endif

#define ION_AUDIO_HEAP_SIZE		(1024*1024*1)
#define ION_AUDIO_HEAP_PHYS     	(PLAT_SECURE_BASE_PHYS  - ION_AUDIO_HEAP_SIZE)

#define RPC_COMM_PHYS			(0x0000B000)
#define RPC_COMM_VIRT			(RBUS_BASE_VIRT+RBUS_BASE_SIZE)
#define RPC_COMM_SIZE			(0x00001000)

#define SPI_RBUS_PHYS			(0x18100000)
#define SPI_RBUS_VIRT			(0xfb000000)
#define SPI_RBUS_SIZE			(0x01000000)

#define SYSTEM_GIC_BASE_PHYS    	(0xff010000)
#define SYSTEM_GIC_BASE_VIRT    	IOMEM(0xff010000)
#define SYSTEM_GIC_CPU_BASE             IOMEM(0xff012000)
#define SYSTEM_GIC_DIST_BASE    	IOMEM(0xff011000)

#endif
