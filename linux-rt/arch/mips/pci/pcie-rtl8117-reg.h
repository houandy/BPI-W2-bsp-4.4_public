#ifndef _PCIE_HOST_REG_H
#define _PCIE_HOST_REG_H

//WIFI DASH memory mapping
//define WIFI_DASH_BASE_ADDR			0xBAFA0000
#define WIFI_DASH_BASE_ADDR				this_pdev->pci_base
#define EP_INDIRECT_CH_OFFSET			0x0
#define RC_INDIRECT_CH_OFFSET			0x100
#define PAGE0_DATA_B3_B0				0x150
#define PAGE_STATUS_REG					(WIFI_DASH_BASE_ADDR + RC_INDIRECT_CH_OFFSET)
#define PAGE_CTRL_OFFSET				0x10
#define PAGE_DATA_OFFSET				0x80

#define EP_DBI_CH_OFFSET				0x400
#define EP_DBI_ADDR						(WIFI_DASH_BASE_ADDR + EP_DBI_CH_OFFSET)
#define EP_DBI_WDATA					(WIFI_DASH_BASE_ADDR + EP_DBI_CH_OFFSET + 0x4)
#define EP_DBI_RDATA					(WIFI_DASH_BASE_ADDR + EP_DBI_CH_OFFSET + 0x8)
#define EP_DBI_CTRL_REG 				(WIFI_DASH_BASE_ADDR + EP_DBI_CH_OFFSET + 0xC)

#define RC_DBI_CH_OFFSET				0x500
#define RC_DBI_ADDR 					(WIFI_DASH_BASE_ADDR + RC_DBI_CH_OFFSET)
#define RC_DBI_WDATA					(WIFI_DASH_BASE_ADDR + RC_DBI_CH_OFFSET + 0x4)
#define RC_DBI_RDATA					(WIFI_DASH_BASE_ADDR + RC_DBI_CH_OFFSET + 0x8)
#define RC_DBI_CTRL_REG 				(WIFI_DASH_BASE_ADDR + RC_DBI_CH_OFFSET + 0xC)

#define ELBI_CH_OFFSET					0x600
#define ELBI_TRAN_CFG0					(WIFI_DASH_BASE_ADDR + ELBI_CH_OFFSET + 0x10)

#define CDM_MBOX_OFFSET					0x700
#define CDM_MBOX_DATA					(WIFI_DASH_BASE_ADDR + CDM_MBOX_OFFSET)
#define CDM_MBOX_ADDR					(WIFI_DASH_BASE_ADDR + CDM_MBOX_OFFSET + 0x4)
#define CDM_MBOX_CTRL					(WIFI_DASH_BASE_ADDR + CDM_MBOX_OFFSET + 0x8)
#define CDM_MBOX_CFG0					(WIFI_DASH_BASE_ADDR + CDM_MBOX_OFFSET + 0x10)

#define DASH_MSIX_OFFSET				0x800
#define SII_OFFSET						0x900
#define SII_INT_CR						(WIFI_DASH_BASE_ADDR + SII_OFFSET)
#define SII_MSG_BYP_ENABLE_REG			(WIFI_DASH_BASE_ADDR + SII_OFFSET + 0x4)
#define SII_MSG_SRC_SEL					(WIFI_DASH_BASE_ADDR + SII_OFFSET + 0x8)

#define CFG_OFFSET						0xA00
#define WIFI_CFG0						(WIFI_DASH_BASE_ADDR + CFG_OFFSET)
#define BYP_MODE_CFG_REG				(WIFI_DASH_BASE_ADDR + CFG_OFFSET + 0x8)
#define WIFIDASH_SOFT_RESET				(WIFI_DASH_BASE_ADDR + CFG_OFFSET + 0xC)
#define WIFI_DIRECTION_MODE_CR			(WIFI_DASH_BASE_ADDR + CFG_OFFSET + 0x10)
#define DUMMY_REG0						(WIFI_DASH_BASE_ADDR + CFG_OFFSET + 0x14)

#define WIFI_DASH_ISR_OFFSET			0xB00
#define WIFI_ISR						(WIFI_DASH_BASE_ADDR + WIFI_DASH_ISR_OFFSET)
#define WIFI_IMR						(WIFI_DASH_BASE_ADDR + WIFI_DASH_ISR_OFFSET + 0x4)

//Fun0
//#define PCIE_FUN0_BASE_ADDR		0xBAF10000
#define PCIE_FUN0_BASE_ADDR				this_pdev->fun0_base_addr

// ****************************************
//#define PCIE_FUN0_BASE_ADDR this_pdev->base
//*********************************************

#define PCIE_FUNO_SW_RESET		(PCIE_FUN0_BASE_ADDR + 0x34)
#define PCIE_FUN0_ISR			(PCIE_FUN0_BASE_ADDR + 0x38)
#define PCIE_FUN0_IMR			(PCIE_FUN0_BASE_ADDR + 0x3A)
#define HOST_PAD_OUT_REG		(PCIE_FUN0_BASE_ADDR + 0x3C)
#define HOST_PAD_SRC_SEL_REG	(PCIE_FUN0_BASE_ADDR + 0X40)
#define RC_MAC_STATUS_REG		(PCIE_FUN0_BASE_ADDR + 0X4c)


#define IO_TYPE		0b00010
#define MEM_TYPE	0b00000
#define CFG_TYPE	0b00100
#define R_CMD		0b01
#define W_CMD		0b10

//for OOB mac 2 IB access channel
//#define OOBMAC_BASE_ADDR		0xBAF70000
#define OOBMAC_BASE_ADDR this_pdev->oob_mac_base
#define OOBMAC_IMR				0x002C
#define IB_ACC_DATA				0x00A0
#define IB_ACC_SET				0x00A4
#define MAC_DBG_SEL				0x00F0
#define MAC_EXTR_INT			0x0100
#define MAC_STATUS				0x0104
#define SWISR					0x180
#define SYS_STATE				0x184
#define PCI_MSG					0x185
#define PCI_CMD					0x186
#define PCI_INFO 				0x187
#define MAC_GPIOCTL				0x0500
#define MAC_GPIOCTL2			0x0504

#define OOBMAC_IB_ACC_DATA    (OOBMAC_BASE_ADDR + IB_ACC_DATA)
#define OOBMAC_IB_ACC_SET     (OOBMAC_BASE_ADDR + IB_ACC_SET)

//UMAC OCP register
#define RISC_SYNC_REG                                0xD3EC
#define PIN_C                                         0xDC30
#define EPHY_MDCMDIO_DATA                    0xDE20
#define EPHY_MDCMDIO_RC_DATA              0xDE28
#define EN_CLK_REG1                                   0xE002
#define WDT9_CTRL                                      0xE434

//PCIE cfg0 space offset
#define VID_DID                                 0x0
#define CMD_STATUS_REG  0x4
#define BADDR0                          0x10
#define BADDR1                          0x14
#define BADDR2                          0x18
#define BADDR3                          0x1c
#define BADDR4                          0x20
#define BADDR5                          0x24
#define CARDBUS_CIS                     0x28
#define SVID_SDID                       0x2c
#define EXP_ROM_ADDR            0x30
#define CAP_PTR                         0x34
#define INT_REG                                 0x3

#define PH_ENABLE (BIT_2)

#endif
