#ifndef __ASM_MACH_RTL8117_PLATFORM_H
#define __ASM_MACH_RTL8117_PLATFORM_H

#define BSP_UART_VADDR		0xba000000UL

#define UART_BASE_ADDR		(u8 __iomem *)KSEG1ADDR(0xba000000)
#define CPU1_BASE_ADDR		(u8 __iomem *)KSEG1ADDR(0xbb000000)
#define CPU2_BASE_ADDR		(u8 __iomem *)KSEG1ADDR(0xbaf00000)
#define OOB_MAC_BASE_ADDR	(u8 __iomem *)KSEG1ADDR(0xbaf70000)
#define TIMER_BASE_ADDR		(u8 __iomem *)KSEG1ADDR(0xba800000)
#define SPI_BASE_ADDR		(u8 __iomem *)KSEG1ADDR(0xbc000000)

#define CPU1_CTRL_REG 		0x04
#define CPU1_FREQ_CAL_REG0	0x08
#define CPU1_FREQ_CAL_REG1	0x0C

#define CPU2_RISC_DATA		0x00
#define CPU2_RISC_CMD		0x04
#define CPU2_DCO_CAL		0x0C

#define OOB_MAC_OCP_ADDR	0xA4
#define OOB_MAC_OCP_DATA	0xA0

#define UMAC_PAD_CONT_REG	0xDC00
#define UMAC_PIN_OE_REG 	0xDC06
#define PIN_REG 		0xDC0C

#define CLKSW_SET_REG		0xE018

#define UMAC_MISC_1		0xE85A
#define UMAC_CONFIG6_PMCH	0xE90A

#define OOBMAC_EXTR_INT 	0x100
#define GPIO_CTRL1_SET		0x500
#define GPIO_CTRL2_SET		0x504
#define GPIO_CTRL3_SET		0x508
#define GPIO_CTRL4_SET		0x50C
#define GPIO_CTRL5_SET		0x510
#define GPIO_CTRL6_SET		0x514
#define GPIO_CTRL7_SET		0x518
#define GPIO_CTRL8_SET		0x51C

#define UART_RBR		0x00
#define UART_THR		0x00
#define UART_DLL		0x00
#define UART_DLH		0x04
#define UART_IER		0x04
#define UART_IIR		0x08
#define UART_FCR		0x08
#define UART_LCR		0x0C
#define UART_MCR		0x10
#define UART_LSR		0x14
#define UART_MSR		0x18
#define UART_SCR		0x1C
#define UART_USR		0x7C

#define TIMER_LC		0x00
#define TIMER_CR		0x08

#define FLASH_SSIENR		0x08
#define FLASH_BAUDR		0x14
#define FLASH_AUTO_LENGTH	0x11C

#define RISC_READ_OP		0x40000000
#define RISC_WRITE_OP   	0x80000000

#define OOB_READ_OP		0x80000000
#define OOB_WRITE_OP		0x80800000

#define DCO_FREQ		400000
#define RISC_FREQ		250000

#define EN_DCO_500M		8
#define REF_DCO_500M		9
#define REF_DCO_500M_VALID	15
#define FRE_REF_COUNT		16
#define FREQ_CAL_EN		1
#define EN_DCO_CLK		10

void OOB_access_IB_reg(u16 offset, volatile u32 *data, u8 en_byte, u32 mode);
void access_RISC_reg(u8 offset, volatile u32 *data, u8 en_byte, u32 mode);

int register_swisr(u8 num, int (*callback)(void *), void (*context));
int unregister_swisr(u8 num, int (*callback)(void *), void (*context));
//EHCI
/** RTK EHCI Engine Register Definition **/
#define EHCI_BASE_ADDR          (0xBAF60000)
#define EHCICONFIG              (0x000 + EHCI_BASE_ADDR)
/* EHCI Configuration register */
#define CMDSTS                  (0x004 + EHCI_BASE_ADDR)
/* Command and  Status register */
#define EHCI_IMR                (0x008 + EHCI_BASE_ADDR)
/* Interrupt mask register */
#define EHCI_ISR                (0x00C + EHCI_BASE_ADDR)
/* Interrupt status register */
#define OUTDesc_Addr            (0x010 + EHCI_BASE_ADDR)
/* Bulk OUT descriptor address register*/
#define INDesc_Addr             (0x014 + EHCI_BASE_ADDR)
/* Bulk IN descriptor address register*/
#define CTLOUTDesc_Addr         (0x018 + EHCI_BASE_ADDR)
/* Control OUT descriptor address register */
#define CTLINDesc_Addr          (0x01C + EHCI_BASE_ADDR)
/* Control IN descriptor address register*/
#define OOBACTDATA              (0x020 + EHCI_BASE_ADDR)
/* OOB access IB IO channel : data register*/
#define OOBACTADDR              (0x024 + EHCI_BASE_ADDR)
/* OOB access IB IO channel : address register*/
#define DBIACTDATA              (0x028 + EHCI_BASE_ADDR)
/* OOB access PCIE Configuration Space channel : data registe*/
#define DBIACTADDR              (0x02C + EHCI_BASE_ADDR)
/* OOB access PCIE Configuration Space channel : address registe*/
/** Hardware state machine response register for debug use **/
#define HWSTATE                 (0x050 + EHCI_BASE_ADDR)
/* Hardware state machine Register */

#define TIMEOUTCFG              (0x028 + EHCI_BASE_ADDR)
/* Timeout config registe*/
#define INTINDESCADDR1		(0x030 + EHCI_BASE_ADDR)
/*Interrupt IN Descriptor Start Address Register 1 for port 2 device(keyboard) */
#define INTINDESCADDR2		(0x034 + EHCI_BASE_ADDR)
/*Interrupt IN Descriptor Start Address Register 2 for port 3 device(mouse)*/
#define DEVICE_ADDRESS		(0x040 + EHCI_BASE_ADDR)
/*Bit[22:16]:device address for mouse device, Bit[14:8]: device address for keyboard device, Bit[6:0]: device address for msd device*/
#define ENDPOINT_REG  		(0x044 + EHCI_BASE_ADDR)
/*BIT[7:4]: keyboard EP;BIT[3:0]: Mouse EP*/
#define BYPASS_INTERRUPT	(0x04C + EHCI_BASE_ADDR)
//#endif

//EHCI connection bit
#define PORTSC                   0x064
#define HCSPARAMS                0x004
//tomadd 2011.09.22
//EHCI IO register
#define CONFIGFLAG               0x060
//tomadd 2011.09.30
//debug for WIN7 remove virtual device issue
#define USBCMD                   0x020

#endif /* __ASM_MACH_RTL8117_PLATFORM_H */
