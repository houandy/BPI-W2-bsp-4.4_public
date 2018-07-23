#ifndef _PCIE_HOST_H
#define _PCIE_HOST_H

enum bits {
    BIT_0 = (1 << 0),
    BIT_1 = (1 << 1),
    BIT_2 = (1 << 2),
    BIT_3 = (1 << 3),
    BIT_4 = (1 << 4),
    BIT_5 = (1 << 5),
    BIT_6 = (1 << 6),
    BIT_7 = (1 << 7),
    BIT_8 = (1 << 8),
    BIT_9 = (1 << 9),
    BIT_10 = (1 << 10),
    BIT_11 = (1 << 11),
    BIT_12 = (1 << 12),
    BIT_13 = (1 << 13),
    BIT_14 = (1 << 14),
    BIT_15 = (1 << 15),
    BIT_16 = (1 << 16),
    BIT_17 = (1 << 17),
    BIT_18 = (1 << 18),
    BIT_19 = (1 << 19),
    BIT_20 = (1 << 20),
    BIT_21 = (1 << 21),
    BIT_22 = (1 << 22),
    BIT_23 = (1 << 23),
    BIT_24 = (1 << 24),
    BIT_25 = (1 << 25),
    BIT_26 = (1 << 26),
    BIT_27 = (1 << 27),
    BIT_28 = (1 << 28),
    BIT_29 = (1 << 29),
    BIT_30 = (1 << 30),
    BIT_31 = (1 << 31)
};

#define INT8U u8
#define INT16U u16
#define USHORT u16
#define INT32U u32
#define ULONG u32
#define PULONG u32*
#define INT64U u64

#define OS_ENTER_CRITICAL raw_spin_lock_irqsave(&rtl8117_pci_lock, flags);
#define OS_EXIT_CRITICAL raw_spin_unlock_irqrestore(&rtl8117_pci_lock, flags);

#define PCI_DEV_ON		(BIT_0)
#define RC_LINK_DOWN		(BIT_1)

#define PH_SUCCESS		BIT_0
#define PH_ERROR_PAGEFULL	BIT_1
#define PH_ERROR_WRONGVALUE	BIT_2
#define PH_ERROR_PCIELINK_FAIL	BIT_3
#define PH_ERROR_NO_DEV		BIT_4
#define PH_ERROR_TIMEOUT	BIT_5
#define PH_ERROR_UNKNOWN	BIT_6
#define PH_ERROR_NODRV		BIT_7

#define MAX_BUS_NUM		255
#define MAX_DEV_NUM		32
#define MAX_FUN_NUM		8

#define SYS_PWR_S0		0x00
#define SYS_PWR_S3		0x03
#define SYS_PWR_S4		0x04
#define SYS_PWR_S5		0x05

#define MODE_4281		BIT_0
#define MODE_BYPASS		BIT_1

#define MSG_MODE_CHANGE		0

//Capability id
#define PCI_PM_CAPID		0x1
#define PCIE_CAPID		0x10
#define PMCSR			0x4
#define LINK_CTRL_REG		0x10

#define CFG_SPCACE_SIZE		0x300

#define BSP_Device_IRQ 15

#define BYPASS_MODE_RDY		0x01
#define TO_4281_MODE_RDY		0x02
#define TO_BYPASS_MODE 		0x11
#define TO_4281_MODE			0x12
#define TO_BYPASS_MODE_SYS	0x13
#define PCI_PWR_STATE			0x63

typedef struct _rtk_pci_dev {
	void __iomem *pci_base;
	void __iomem *oob_mac_base;
	void __iomem *fun0_base_addr;

	volatile INT32U FunNo;
	/*0x0-0x1FC/4 is for cfg 0x0~0x1FC,0x200/4~0x300/4 is for 0x0x700~0x7FC*/
	volatile INT32U Default_cfg[1][CFG_SPCACE_SIZE/4];
	volatile INT32U Vendor_cfg[1][CFG_SPCACE_SIZE/4];
	volatile INT32U Rxcount;
	volatile INT32U Txcount;
	volatile INT32U io_addr;
	volatile INT32U mmio_addr;
	volatile INT32U rxdesctmp;
	volatile INT32U txdesctmp;
	volatile INT32U WrongCount;
	volatile INT32U IMRvalue;
	volatile INT32U Vendor_cfg_bar[6][2];//6 means bar0~bar5,2 means type and size
	volatile INT32U PM_status_offset;
	volatile INT32U ASPM_status_offset;
	volatile INT16U slave_dev_imr;
	volatile INT8U S3_Flag;
	volatile INT8U S4_Flag;
	volatile INT8U S5_Flag;
	volatile INT8U Bypass_mode_wocfg_flag;
	volatile INT8U goto4281mode;
	volatile INT8U EP_prst_status;
	volatile INT8U PcieSwISR:1, DevToOn:1, DevToOff:1;
	volatile INT8U BME_disable_inBypass;
	INT32U nEP_link_down;
	INT32U nRC_link_down;
	INT16U bus;
	INT16U dev;
	INT16U fun;
	INT8U driver_ready;

	struct delayed_work switch_mode_schedule;
}rtk_pci_dev;

typedef struct _Ep_DBI {
	volatile INT32U :2;
	volatile INT32U addr:10;
	volatile INT32U :4;
	volatile INT32U func_num:3;
	volatile INT32U :1;
	volatile INT32U cs2:1;
	volatile INT32U :11;
	volatile INT32U wdata:32;
	volatile INT32U rdata:32;
	volatile INT32U Ctrl_execute:1;
	volatile INT32U Ctrl_cmd:2;	//2'b01: Read,2'b10: Write
	volatile INT32U Ctrl_byteen:4;
	volatile INT32U :1;
	volatile INT32U Ctrl_err:1;
	volatile INT32U :23;
} Ep_DBI;

typedef struct Rc_DBI {
	volatile INT32U :2;
	volatile INT32U addr:10;
	volatile INT32U cs2:1;
	volatile INT32U :19;
	volatile INT32U wdata:32;
	volatile INT32U rdata:32;
	volatile INT32U Ctrl_execute:1;
	volatile INT32U Ctrl_cmd:2;		//2'b01: Read,2'b10: Write
	volatile INT32U Ctrl_byteen:4;
	volatile INT32U :1;
	volatile INT32U Ctrl_err:1;
	volatile INT32U :23;
} Rc_DBI;

typedef struct FUN0_INT {
	volatile INT16U rx_pkt_avail_sts:1;		//bit0
	volatile INT16U rx_ok_sts:1;			//bit1
	volatile INT16U rdu_sts:1;				//bit2
	volatile INT16U tx_ok_sts:1;			//bit3
	volatile INT16U vpd_sts:1;				//bit4
	volatile INT16U perstb_r_sts:1;			//bit5
	volatile INT16U perstb_f_sts:1;			//bit6
	volatile INT16U lanwake_rc_f_sts:1;		//bit7
	volatile INT16U sii_rc_ltr_msg:8;		//bit8
} FUN0_INT;

typedef struct CDM_MBOX_INT {
	volatile INT16U rg_cdm_rw:1;
	volatile INT16U rg_cdm_axi_err:1;
	volatile INT16U rg_cdm_timeout:1;
	volatile INT32U :13;
} CDM_MBOX_INT;

typedef struct CDM_MBOX_CFG {
	volatile INT32U rg_cdm_data;		//io data;
	volatile INT32U rg_cdm_addr;		//io addr;
	volatile INT32U :16;
	volatile INT32U rg_cdm_rd:4;		//io wirte byte enalbe;
	volatile INT32U rg_cdm_wr:4;		//io read byte enalbe;
	volatile INT32U :5;
	volatile INT32U rg_func_num:2;	//notify which pcie function's io operation

	//1'b1 : set by ELBI_TRANS,  notify HOST an io operation to be proccessed ,
	//1'b0 : clear by HOST (4281) , notify ELBI_TRANS io operation completed
	volatile INT32U rg_cdm_rw_flag:1;
} CDM_MBOX_CFG;

typedef struct _ELBI_TRAN_INT {
	volatile INT16U rg_io_rw:1;
	volatile INT16U rg_io_axi_err:1;
	volatile INT16U rg_io_timeout:1;
	volatile INT32U :13;
} ELBI_TRAN_INT;

typedef struct _ELBI_TRAN_IO {
	volatile INT32U rg_io_data;		//io data;
	volatile INT32U rg_io_addr;		//io addr;
	volatile INT32U :16;
	volatile INT32U rg_io_rd:4;		//io wirte byte enalbe;
	volatile INT32U rg_io_wr:4;		//io read byte enalbe;
	volatile INT32U :5;
	volatile INT32U rg_func_num:2;	//notify which pcie function's io operation

	//1'b1 : set by ELBI_TRANS,  notify HOST an io operation to be proccessed ,
	//1'b0 : clear by HOST (4281) , notify ELBI_TRANS io operation completed
	volatile INT32U rg_io_rw_flag:1;
} ELBI_TRAN_IO;

typedef struct _PH_INT {
	volatile INT32U sii_rc_inta:1;		//bit0
	volatile INT32U sii_rc_intb:1;		//bit1
	volatile INT32U sii_rc_intc:1;		//bit2
	volatile INT32U sii_rc_intd:1;		//bit3
	volatile INT32U sii_rc_pme_msg:1;	//bit4
	volatile INT32U sii_rc_err_msg:1;	//bit5
	volatile INT32U sii_rc_ven_msg:1;	//bit6
	volatile INT32U sii_ep_ven_msg:1;	//bit7
	volatile INT32U sii_rc_ltr_msg:1;	//bit8
	volatile INT32U sii_ep_obff_msg:1;	//bit9
	volatile INT32U sii_ep_unlock_msg:1;//bit10
	volatile INT32U elbi_rw_sts:1;		//bit11
	volatile INT32U elbi_axi_err:1;		//bit12
	volatile INT32U rc_link_down:1;	//bit13
	volatile INT32U cdm_rw:1;			//bit14
	volatile INT32U cdm_axi_err:1;		//bit15
	volatile INT32U ep_link_down:1;	//bit16
	volatile INT32U rc_indr_int:1;		//bit17
	volatile INT32U lc_wr_timeout:1;	//bit18
	volatile INT32U lc_rd_timeout:1;		//bit19
	volatile INT32U rg_up_dma_wdu:1;	//bit20
	volatile INT32U rg_up_dma_ok:1;	//bit21
	volatile INT32U hst_wr_timeout:1;	//bit22
	volatile INT32U hst_rd_timeout:1;	//bit23
	volatile INT32U rg_dn_dma_rdu:1;	//bit24
	volatile INT32U rg_dn_dma_ok:1;	//bit25
	volatile INT32U :6;
}PH_INT;


struct RcPage {
	volatile INT32U TLP_TYPE:5;	//Tlp's type,IO,cfg,mem [4:0]
	volatile INT32U :1;			//reserved  [5];
	volatile INT32U EP:1;		//TLP's EP bit [6]
	volatile INT32U :1;			//reserved [7]
	volatile INT32U NS:1;		//TLP's NS bit [8]
	volatile INT32U RO:1;		//TLP's RO bit [9]
	volatile INT32U TC:3;		//TLP's TC bit [12:10]
	volatile INT32U MSG:8;		//TLP's MSG code [20:13]
	volatile INT32U DBI:1;		//TLP's TC bit [21]
	volatile INT32U :10;		//resrverd [31:22]
	volatile INT32U LEN:5;		//pcie len
	volatile INT32U CMD:2;		//r/w cmd
	volatile INT32U :1;			//reserved
	volatile INT32U FIRST_BE:4;	//first_byte
	volatile INT32U LAST_BE:4;	//last_byte
	volatile INT32U :12;		//reserved
	volatile INT32U DONE:1;
	volatile INT32U ERR:1;		//ERR BIT read only
	volatile INT32U INTEN:1;	//'1' mean need  send interrupt after page compeltion
	volatile INT32U OWN:1;	//'1' valid for HW, 0
	volatile INT32U ADDRL:32;
	volatile INT32U ADDRH:32;
};

struct CFG_ADDR {
	volatile INT32U :2;
	volatile INT32U reg_num:10;
	volatile INT32U :4;
	volatile INT32U fun:3;
	volatile INT32U dev:5;
	volatile INT32U bus:8;
};

void OOBMACWriteIBReg(INT16U reg, INT8U highBit, INT8U lowBit, INT32U value);
INT32U OOBMACReadIBReg(INT16U reg);
void Rc_ephy_W(INT32U reg, INT32U data);
void OOB2IB_W(INT16U reg, INT8U byte_en, INT32U value);

INT32U pcieh_rc32_read(volatile USHORT addr, volatile INT32U *value);
INT32U pcieh_rc32_write(volatile USHORT addr, volatile ULONG value);
ULONG pcieh_cfg32_read(volatile USHORT bus, volatile USHORT dev, volatile USHORT fun, volatile USHORT addr, volatile INT32U *value);
ULONG pcieh_cfg32_write(volatile USHORT bus, volatile USHORT dev, volatile USHORT fun, volatile USHORT addr, volatile INT32U value);
ULONG pcieh_cfg16_read(volatile USHORT bus, volatile USHORT dev, volatile USHORT fun, volatile USHORT addr, volatile INT32U *value);
ULONG pcieh_cfg16_write(volatile USHORT bus, volatile USHORT dev, volatile USHORT fun, volatile USHORT addr, volatile INT32U value);
ULONG pcieh_cfg8_read(volatile USHORT bus, volatile USHORT dev, volatile USHORT fun,volatile USHORT addr, volatile INT32U *value);
ULONG pcieh_cfg8_write(volatile USHORT bus, volatile USHORT dev, volatile USHORT fun, volatile USHORT addr, volatile INT32U value);
ULONG pcieh_io32_read(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U *value);
ULONG pcieh_io32_write(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U value);
ULONG pcieh_io16_read(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U *value);
ULONG pcieh_io16_write(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U value);
ULONG pcieh_io8_read(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U *value);
ULONG pcieh_io8_write( volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U value);
u32 pcieh_mem32_read(volatile u32 Haddr, volatile u32 Laddr,volatile u32 *value);
u32 pcieh_mem32_write(volatile u32 Haddr, u32 Laddr, u32 value);
u32 pcieh_mem16_read(volatile u32 Haddr, volatile u32 Laddr, volatile u32 *value);
u32 pcieh_mem16_write(volatile u32 Haddr, volatile u32 Laddr,volatile u32 value);
u32 pcieh_mem8_read(volatile u32 Haddr, volatile u32 Laddr, volatile u32 *value);
u32 pcieh_mem8_write(volatile u32 Haddr, volatile u32 Laddr, volatile u32 value);
u32 Read_Vt_PCIDword(volatile USHORT addr, volatile USHORT fun_num, volatile INT32U *value);
u32 Write_Vt_PCIDword(volatile USHORT addr, volatile USHORT fun_num, volatile ULONG value);

u8 oobm_read_pci_cmd(void);

ULONG Send_TLP_Polling(volatile INT32U TLP_TPYE, volatile INT32U RW,
	volatile INT32U Base_addr_H, volatile INT32U Base_addr_L, volatile INT32U offset,
	volatile INT32U first_byte_en, volatile INT32U last_byte_en, volatile INT32U LEN,
	volatile INT32U Timeout, volatile INT32U *value);

void pcieh_enable_power_seq(void);
void pcieh_disable_pwr_seq(void);
void bsp_Fun0_handler(void);
void config_rw_handler(rtk_pci_dev *pdev);
void rc_link_down_handler(rtk_pci_dev *pdev);

//void pcie_dash_chk_mode(struct work_struct *work);

void pcie_dash_switch_to_bypass(rtk_pci_dev *pdev);
void pcie_dash_switch_to_4281_in_sleep(rtk_pci_dev *pdev);

INT32U adapter_cfg_init(rtk_pci_dev *pdev); //use for initial scan, sucnk link bios scan

void init_pci_setup(rtk_pci_dev *pdev);

INT32U pcieh_init(rtk_pci_dev *pdev);
INT32U pcieh_exit(rtk_pci_dev *pdev);

#endif
