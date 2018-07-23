#include <linux/types.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>
#include <linux/platform_device.h>

#include <rtl8117_platform.h>
#include "pcie-rtl8117-reg.h"
#include "pcie-rtl8117-core.h"

#if 1
#define default_IMR (BIT_0 | BIT_1 | BIT_2 | BIT_3 |BIT_13 | BIT_14 | BIT_16)
#else
#define default_IMR (BIT_0 | BIT_1 | BIT_2 | BIT_3)
#endif

#define disable_dev_IMR (BIT_13 | BIT_14 | BIT_16)

#define IO_BASE_ADDR                    0x0000D000
#define MEM_BASE_ADDR                   0xC0000000

#define FAKE_DID                0x816510EC
#define FUN5                            0x5
#define PHOST_WIFIDASH  FUN5

#define BUS_NUM_T	1 //total bus num,defalt define 1
#define DEV_NUM_T	1 //total device num,defalt define 1
#define FUN_NUM_T	2 //total function num,defalt defin 2

#define PCIE_TIME_OUT		1000000
#define PCIE_SCAN_TIME_OUT	5
#define PCIE_RESCAN_MAX		2

static rtk_pci_dev *this_pdev;

static DEFINE_RAW_SPINLOCK(rtl8117_pci_lock);
static unsigned long flags;

static INT8U oobm_read_pci_info(void)
{
	return readb(OOBMAC_BASE_ADDR + PCI_INFO);
}

static void oobm_write_pci_info(INT8U value)
{
	writeb(value, OOBMAC_BASE_ADDR + PCI_INFO);
}

static inline u8 oobm_read_pci_msg(void)
{
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + PCI_MSG));
}

static inline void oobm_write_pci_msg(u8 value)
{
	writeb(value, (volatile void __iomem *)(OOBMAC_BASE_ADDR + PCI_MSG));
}

inline u8 oobm_read_pci_cmd(void)
{
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + PCI_CMD));
}

static inline u8 oobm_write_pci_cmd(u8 value)
{
	writeb(value, (volatile void __iomem *)(OOBMAC_BASE_ADDR + PCI_CMD));
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + PCI_CMD));
}

static inline u8 oobm_read_pci_swisr(void)
{
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + SWISR));
}

static inline u8 oobm_write_pci_swisr(u8 value)
{
	writeb(value, (volatile void __iomem *)(OOBMAC_BASE_ADDR + SWISR));
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + SWISR));
}

static inline u8 oobm_read_pci_sys_state(void)
{
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + SYS_STATE));
}

static inline u8 oobm_write_pci_sys_state(u8 value)
{
	writeb(value, (volatile void __iomem *)(OOBMAC_BASE_ADDR + SYS_STATE));
	return readb((volatile void __iomem *)(OOBMAC_BASE_ADDR + SYS_STATE));
}

inline void pcieh_set_imr_with_devint(void)
{
	writel(default_IMR, WIFI_IMR);
}

inline void pcieh_set_imr_wo_devint(void)
{
	writel(disable_dev_IMR, WIFI_IMR);
}

static INT32U Byte_Enable_2_FF(volatile INT8U byte_enable)
{
	INT32U FFtype = 0;

	if (byte_enable & BIT_0)
		FFtype |= 0xFF;
	if (byte_enable & BIT_1)
		FFtype |= 0xFF00;
	if (byte_enable & BIT_2)
		FFtype |= 0xFF0000;
	if (byte_enable & BIT_3)
		FFtype |= 0xFF000000;

	return FFtype;
}

//Virtual CFG Read for mutifunction
static INT32U _pcieh_vt32_read(volatile USHORT addr, volatile USHORT fun_num, volatile INT32U *value, volatile USHORT cs2)
{
	volatile INT32U tmp = 0, rv = PH_ERROR_TIMEOUT, EP_dbi_addr = 0;

	OS_ENTER_CRITICAL;

	if (readb(BYP_MODE_CFG_REG) == 3 && (fun_num < 5 || fun_num > 6)) {
		rv = 0xFFFFFFFF;
		goto exit;
	}

	EP_dbi_addr |= addr;
	EP_dbi_addr |= (fun_num << 16);
	EP_dbi_addr |= (cs2 << 19);

	writel(EP_dbi_addr, EP_DBI_ADDR);	//EP_DBI addr,fun mum and cs2
	writel(0x3, EP_DBI_CTRL_REG);	//write and execute

	for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
		if (!(readl(EP_DBI_CTRL_REG) & BIT_0)) {
			*value = readl(EP_DBI_RDATA);
			rv  = PH_SUCCESS;
			break;
		}
	}
	OS_EXIT_CRITICAL;

exit:
	if (rv != PH_SUCCESS)
		printk("[PCIE] _pcieh_vt32_read fauled, addr = 0x%x, fun_num = 0x%x\n", addr, fun_num);
	return rv;
}

//Virtual CFG Write for mutifunction
static INT32U _pcieh_vt32_write(volatile USHORT addr, volatile USHORT fun_num, volatile ULONG value,
								volatile INT32U first_byte_en, volatile USHORT cs2)
{
	volatile INT32U tmp = 0, rv = PH_ERROR_TIMEOUT, EP_dbi_addr = 0, cmd = 0;

	OS_ENTER_CRITICAL;

	if (readb(BYP_MODE_CFG_REG) == 3 && (fun_num < 5 || fun_num > 6)) {
		rv = 0xFFFFFFFF;
		goto exit;
	}

	EP_dbi_addr |= addr;
	EP_dbi_addr |= (fun_num << 16);
	EP_dbi_addr |= cs2 << 20;

	writel(EP_dbi_addr, EP_DBI_ADDR);
	writel(value, EP_DBI_WDATA);	//EPDBI Write data

	cmd |= (first_byte_en << 3);
	cmd |= 5;

	writel(cmd, EP_DBI_CTRL_REG);	//EPDBI byte_enalbe+write_cmd+execute
	for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
		if (!(readl(EP_DBI_CTRL_REG) & BIT_0) ) {
			rv = PH_SUCCESS;
			break;
		}
	}

	OS_EXIT_CRITICAL;

exit:
	if (rv != PH_SUCCESS)
		printk("[PCIE] _pcieh_vt32_write fauled, addr = 0x%x, fun_num = 0x%x\n", addr, fun_num);
	return rv;
}

INT32U pcieh_vt32_read(volatile USHORT addr, volatile USHORT fun_num, volatile INT32U *value)
{
	return _pcieh_vt32_read(addr, fun_num, value, 0);
}

INT32U pcieh_vt32_write(volatile USHORT addr, volatile USHORT fun_num, volatile ULONG value)
{
	return _pcieh_vt32_write(addr, fun_num, value, 0xF, 0);
}

//RC CFG RW
INT32U pcieh_rc32_read(volatile USHORT addr, volatile INT32U *value)
{
	volatile INT32U tmp = 0, rv = PH_ERROR_TIMEOUT;

	OS_ENTER_CRITICAL;

	writel(addr, RC_DBI_ADDR);
	writel(0x3, RC_DBI_CTRL_REG);

	for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
		if (!(readl(RC_DBI_CTRL_REG) & BIT_0)) {
			*value = readl(RC_DBI_RDATA);
			rv  = PH_SUCCESS;
			break;
		}
	}

	OS_EXIT_CRITICAL;

	return rv;
}
EXPORT_SYMBOL(pcieh_rc32_read);

INT32U pcieh_rc32_write(volatile USHORT addr, volatile ULONG value)
{
	volatile INT32U tmp = 0, rv = 0;

	OS_ENTER_CRITICAL;

	writel(addr, RC_DBI_ADDR);
	writel(value, RC_DBI_WDATA);
	writel(0x7D, RC_DBI_CTRL_REG);

	for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
		if (!(readl(RC_DBI_CTRL_REG) & BIT_0)) {
			rv ++;
			break;
		}
	}

	OS_EXIT_CRITICAL;

	return 0;
}

static INT32U backup_vendor_cfg(volatile USHORT bus, volatile USHORT dev,
								volatile USHORT fun, volatile INT32U * memory)
{
	volatile INT32U i, ph_result = 0;

	for (i = 0; i < CFG_SPCACE_SIZE/4; i++) {
		/*back up 0x0~0x1FC*/
		if (i < 0x200/4)
			ph_result |= pcieh_cfg32_read(bus, dev, fun, i*4, memory+i);	//Back up Vendor cfg to memory

		/*back up 0x700~0x7FC*/
		else if (i >= 0x200/4)
			ph_result |= pcieh_cfg32_read(bus, dev, fun, (i+0x140)*4, memory+i);	//Back up Vendor cfg to memory
	}
	return ph_result;
}

static INT32U default_table_2_wificfg(rtk_pci_dev *pdev)
{
	INT32U i = 0, ph_result = PH_SUCCESS;

	for (i = 1; i < CFG_SPCACE_SIZE/4; i++) {
		/*resume 0x0~0x1FC*/
		if (i < 0x200/4)
			ph_result |= pcieh_cfg32_write(0, 0, 0, i*4, pdev->Default_cfg[0][i]);
		/*resume 0x700~0x7FC*/
		else if (i >= 0x200/4)
			ph_result |= pcieh_cfg32_write(0, 0, 0, (i+0x140)*4, pdev->Default_cfg[0][i]);
	}

	return ph_result;
}

static INT32U cfg_bar_vd2vt(rtk_pci_dev *pdev, volatile USHORT vdfun)
{
	volatile INT32U i = 0, ph_result = 0, addr = 0;

	for (i = 0; i < 6; i++) {
		addr = BADDR0 + i*4;
		if (pdev->Vendor_cfg_bar[i][1] != 0) {
			/*cfg type*/
			ph_result |= pcieh_vt32_write(addr, vdfun, pdev->Vendor_cfg_bar[i][0]);
			/*cfg size*/
			ph_result |= _pcieh_vt32_write(addr, vdfun, pdev->Vendor_cfg_bar[i][1]-1, 0xF, 1);
		} else
			ph_result |= _pcieh_vt32_write(addr, vdfun, 0, 0xF, 1);
	}

	return ph_result;
}

static INT32U vdtable_2_wificfg(rtk_pci_dev *pdev)
{
	volatile INT32U i = 0, ph_result = 0;

	for (i = 1; i < CFG_SPCACE_SIZE/4; i++)
	{
		/*resume 0x0~0x1FC*/
		if ((i < 0x200/4)) {
			ph_result |= pcieh_cfg32_write(0, 0, 0 ,i*4, pdev->Vendor_cfg[0][i]);
			ph_result |= pcieh_cfg32_read(0, 0, 0, i*4, &pdev->Vendor_cfg[0][i]);
		}
		/*resume 0x700~0x7FC*/
		else if(i >= 0x200/4) {
			ph_result |= pcieh_cfg32_write(0, 0, 0, (i+0x140)*4, pdev->Vendor_cfg[0][i]);
			ph_result |= pcieh_cfg32_read(0, 0, 0, (i+0x140)*4, &pdev->Vendor_cfg[0][i]);
		}
	}
	return ph_result;
}

static void  _2_4281_cfg(rtk_pci_dev* pdev)
{
/*
	OS_ENTER_CRITICAL;

#if 0
	writeb(0x0, WIFI_CFG0);
	writeb(0x3, BYP_MODE_CFG_REG);
	writeb(0x2, DUMMY_REG0);
#endif

	writeb(readb(ELBI_TRAN_CFG0) & 0xFE, ELBI_TRAN_CFG0); //set  LEBI_RTANS working at by 4281mode(0)

#if 0
	writeb(readb(CDM_MBOX_CFG0) & 0xFE, CDM_MBOX_CFG0); //set CDM_MBOX working at 4281 mode(0)
#endif

	writeb(0xFF, SII_INT_CR);
	writeb(0x00, SII_MSG_BYP_ENABLE_REG);
	writeb(0x00, SII_MSG_SRC_SEL);

	pcieh_set_imr_wo_devint();	//set interrupt

	writeb(0x0, WIFI_DIRECTION_MODE_CR);

	pdev->Bypass_mode_wocfg_flag = 0;

	OS_EXIT_CRITICAL;
*/

	OS_ENTER_CRITICAL;

	writeb(0x0, WIFI_CFG0);	//rg_bypass for 4281 mode
	writeb(0x3, BYP_MODE_CFG_REG);	//by pass mode cfg reg
	writeb(0x2, DUMMY_REG0);	//wifidash_cdm_byp_en: cfg tlp bypass wifidash and cancel the ASPM binding between fp device and fp host

	writeb(readb(ELBI_TRAN_CFG0) & 0xFE, ELBI_TRAN_CFG0);	//set  LEBI_RTANS working at by 4281mode(0)
	writeb(readb(CDM_MBOX_CFG0) & 0xFE, CDM_MBOX_CFG0);	//set CDM_MBOX working at 4281 mode(0)

	writeb(0xFF, SII_INT_CR);	//device MSG interrupt to 4281 enable;
	writeb(0x00, SII_MSG_BYP_ENABLE_REG);	//MSG interrupt bypass to chipset disable;
	writeb(0x00, SII_MSG_SRC_SEL);	//other msg to 4281 mode

	pcieh_set_imr_wo_devint();	//set interrupt

	writeb(0x0, WIFI_DIRECTION_MODE_CR);	// clear mem R/W as direcion patch for dma

	pdev->Bypass_mode_wocfg_flag = 0;

	OS_EXIT_CRITICAL;
}

/*
control the PCIE host's pcie rst pin by 4281
0:pcie rst falling down;
1:pcie rst rising up;
*/
static void prst_control(INT8U value)
{
	if (!value) {
		writeb(readb(HOST_PAD_OUT_REG) & 0xFE, HOST_PAD_OUT_REG);
	}
	else if (value) {
		writeb(readb(HOST_PAD_OUT_REG) | BIT_0, HOST_PAD_OUT_REG);
	}
}

/*
control the PCIE host's pcie isolateb pin by 4281
0:isolateb falling down;
1:isolateb rising up;
*/
static void Iso_control(INT8U value)
{
	if (!value) {
		writeb(readb(HOST_PAD_OUT_REG) & 0xFD, HOST_PAD_OUT_REG);
	}
	else if (value) {
		writeb(readb(HOST_PAD_OUT_REG) | BIT_1, HOST_PAD_OUT_REG);
	}
}

static INT32U search_pid(volatile INT32U bus, volatile INT32U dev, volatile INT32U fun,
					volatile INT32U SPID, volatile INT32U offset)
{
	volatile INT32U ph_result = 0, Roffset = 0, PID = 0;

	ph_result |= pcieh_cfg8_read(bus, dev, fun, CAP_PTR, &Roffset);
	ph_result |= pcieh_cfg8_read(bus, dev, fun, Roffset, &PID);

	while (PID != SPID) {
		ph_result |= pcieh_cfg8_read(bus, dev, fun, Roffset + 1, &Roffset);
		ph_result |= pcieh_cfg8_read(bus, dev, fun, Roffset, &PID);
		if (Roffset >= 200)
			break;
	}

	return Roffset+offset;
}


//bit0:Power rst
//bit1:WIFIDASH_SOFT_RESET
static void pcieh_sw_reset(INT8U value)
{
	writel(value, PCIE_FUNO_SW_RESET);
	writel(0, PCIE_FUNO_SW_RESET);
}

static INT32U get_cfg_read_data(rtk_pci_dev *pdev,INT32U rg_cdm_addr, INT32U rg_func_num)
{
	INT32U return_data = 0;


	if ((rg_cdm_addr >= 0x700) && (rg_cdm_addr < 0x800))
		return_data = pdev->Vendor_cfg[rg_func_num][(rg_cdm_addr-0x500)/4];

	else if (rg_cdm_addr == 0x30 || rg_cdm_addr >= 0x200)
			return_data = 0x0;

	else
		return_data = pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4];

	return return_data;
}

static void put_cfg_write_data_with_Bypass(rtk_pci_dev *pdev,
	INT32U rg_cdm_addr, INT32U rg_func_num, INT32U rg_cdm_data, INT32U rg_cdm_wr)
{
	INT32U Rvalue = 0, ph_result = PH_SUCCESS;

	if (rg_cdm_addr == VID_DID ||rg_cdm_addr == EXP_ROM_ADDR){}

	else if (rg_cdm_addr == pdev->ASPM_status_offset){} //Walkaround

	else if ((rg_cdm_addr >= CMD_STATUS_REG) && (rg_cdm_addr < 0x200))
	{
		Send_TLP_Polling(CFG_TYPE, W_CMD, 0, rg_func_num, rg_cdm_addr,
						rg_cdm_wr, 0, 1, PCIE_TIME_OUT, &rg_cdm_data);
		pcieh_cfg32_read(0, 0, rg_func_num, rg_cdm_addr, &Rvalue);
		pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4] = Rvalue;

		/*cfg base addr reg*/
		if (rg_cdm_addr >= BADDR0 && rg_cdm_addr <= BADDR5) {
			ph_result |= _pcieh_vt32_write(rg_cdm_addr, rg_func_num + 5,
						rg_cdm_data | pdev->Vendor_cfg_bar[(rg_cdm_addr-0x10)/4][0],
						rg_cdm_wr, 0);
		}
		else if (rg_cdm_addr < 0x40) {
			ph_result |= _pcieh_vt32_write(rg_cdm_addr, rg_func_num + 5,
						rg_cdm_data, rg_cdm_wr, 0);

			if (rg_cdm_addr == CMD_STATUS_REG) {
				/* disable driver process */
#if 0
				if (!Getbit(pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4], BIT_2))
#else
				if (!(pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4] & BIT_2))
#endif
					pdev->BME_disable_inBypass = 1;
				else
					pdev->BME_disable_inBypass = 0;
			}
		}

		else if (rg_cdm_addr == pdev->PM_status_offset) {
			ph_result |= _pcieh_vt32_write(pdev->PM_status_offset,
						rg_func_num + 5, rg_cdm_data, rg_cdm_wr, 0);
		}
		/*aspm cfg write*/
		else if (rg_cdm_addr == pdev->ASPM_status_offset) {
			ph_result |= _pcieh_vt32_write(pdev->PM_status_offset, rg_func_num+5,
				rg_cdm_data, rg_cdm_wr, 0);
		}
	}
	else if (rg_cdm_addr >= 0x700 && rg_cdm_addr < 0x800) {
		Send_TLP_Polling(CFG_TYPE, W_CMD, 0, rg_func_num, rg_cdm_addr,
						rg_cdm_wr, 0, 1, PCIE_TIME_OUT, &rg_cdm_data);
		pcieh_cfg32_read(0 ,0 ,rg_func_num, rg_cdm_addr, &Rvalue);
		pdev->Vendor_cfg[rg_func_num][(rg_cdm_addr-0x500)/4] = Rvalue;
	}
	else
		printk("[PCIE] Attention!\n cfg write over 0x200:0x%8X\n", rg_cdm_addr);
}

static void put_cfg_write_data_with_4281(rtk_pci_dev *pdev, INT32U rg_cdm_addr,
	INT32U rg_func_num, INT32U rg_cdm_data, INT32U rg_cdm_wr)
{
	INT32U ph_result = PH_SUCCESS, FFType =0;

	if (rg_cdm_addr == VID_DID || rg_cdm_addr == EXP_ROM_ADDR){}

	else if (rg_cdm_addr >= BADDR0 && rg_cdm_addr <= BADDR5) {
		pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4]
			= (rg_cdm_data & (~(pdev->Vendor_cfg_bar[(rg_cdm_addr-0x10)/4][1] - 1)))
				|pdev->Vendor_cfg_bar[(rg_cdm_addr-0x10)/4][0];
		ph_result |= _pcieh_vt32_write(rg_cdm_addr, rg_func_num+5,
					rg_cdm_data | pdev->Vendor_cfg_bar[(rg_cdm_addr-0x10)/4][0],
					rg_cdm_wr, 0);
	}

	 /*mask the BME enable when in 4281 mode */
	else if (rg_cdm_addr == CMD_STATUS_REG) {
		pdev->Vendor_cfg[0][1] = pdev->Vendor_cfg[0][1] & 0xFFFFFFF8;
		_pcieh_vt32_write(CMD_STATUS_REG, PHOST_WIFIDASH, pdev->Vendor_cfg[0][1], 0xF, 0);
	}

	else if (rg_cdm_addr == pdev->ASPM_status_offset) {}  //Walkaround

	else if (rg_cdm_addr >= 0x8 && rg_cdm_addr < 0x200) {
		FFType = Byte_Enable_2_FF(rg_cdm_wr);

		/*update the table*/
		pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4]
			= (pdev->Vendor_cfg[rg_func_num][rg_cdm_addr/4] | FFType)
			& (rg_cdm_data |(~FFType));

		if (rg_cdm_addr < 0x40) {
			ph_result |= _pcieh_vt32_write(rg_cdm_addr, rg_func_num + 5,
						rg_cdm_data, rg_cdm_wr,	0);
		}

		else if (rg_cdm_addr == pdev->PM_status_offset) {
			ph_result |= _pcieh_vt32_write(pdev->PM_status_offset,
						rg_func_num + 5, rg_cdm_data, rg_cdm_wr, 0);
		}

		else if (rg_cdm_addr == pdev->ASPM_status_offset) {
			ph_result |= _pcieh_vt32_write(pdev->ASPM_status_offset,
						rg_func_num + 5, rg_cdm_data, rg_cdm_wr, 0);
		}
	}

	else if (rg_cdm_addr >= 0x700 && rg_cdm_addr < 0x800) {
		FFType = Byte_Enable_2_FF(rg_cdm_wr);

		pdev->Vendor_cfg[rg_func_num][(rg_cdm_addr-0x500)/4]
			= (pdev->Vendor_cfg[rg_func_num][(rg_cdm_addr-0x500)/4] | FFType)
			& (rg_cdm_data | (~FFType));
	}
	else
		printk("[PCIE] Attention!\n cfg write over 0x200:0x%8X\n", rg_cdm_addr);
}

void config_rw_handler(rtk_pci_dev *pdev)
{
	INT32U rg_cdm_data, rg_cdm_addr, rg_func_num, rg_cdm_rd, rg_cdm_wr;

#if 0
	REGX16(WIFI_ISR) |= BIT_14; // clean ISR
#else
	writew(BIT_14, WIFI_ISR);
#endif

#if 0
	rg_cdm_data = REGX32(CDM_MBOX_DATA);
	rg_cdm_addr = REGX32(CDM_MBOX_ADDR);
	rg_func_num = (REGX32(CDM_MBOX_CTRL) & 0x60000000) >> 29;
	rg_cdm_rd = (REGX32(CDM_MBOX_CTRL) & 0xF0000) >> 16;
	rg_cdm_wr = (REGX32(CDM_MBOX_CTRL) & 0xF00000) >> 20;
#else
	rg_cdm_data = readl(CDM_MBOX_DATA);
	rg_cdm_addr = readl(CDM_MBOX_ADDR);
	rg_func_num = (readl(CDM_MBOX_CTRL) & 0x60000000) >> 29;
	rg_cdm_rd = (readl(CDM_MBOX_CTRL) & 0xF0000) >> 16;
	rg_cdm_wr = (readl(CDM_MBOX_CTRL) & 0xF00000) >> 20;
#endif

	/*Deal the EP fun 5/fun0 cfg r/w */
	if (rg_func_num == 0) {
		if (pdev->Bypass_mode_wocfg_flag) /*Deal the cfg R/W in bypass mode*/
		{
			/*Not support expansion rom(0x30) ,offset 0x200~0x6FC, larger than 0x800	*/
			if (rg_cdm_rd)
				rg_cdm_data = get_cfg_read_data(pdev, rg_cdm_addr, rg_func_num);
			else if (rg_cdm_wr)
				put_cfg_write_data_with_Bypass(pdev, rg_cdm_addr, rg_func_num, rg_cdm_data, rg_cdm_wr);
		}

		else if (!pdev->Bypass_mode_wocfg_flag) /*Deal the cfg R/W in 4281 mode*/
		{
			/*Not support expansion rom(0x30) ,offset 0x200~0x6FC, larger than 0x800	*/
			if (rg_cdm_rd)
				rg_cdm_data = get_cfg_read_data(pdev, rg_cdm_addr, rg_func_num);
			else if (rg_cdm_wr)
				put_cfg_write_data_with_4281(pdev, rg_cdm_addr, rg_func_num, rg_cdm_data, rg_cdm_wr);
		}
	}
	/*Deal the EP fun 6/fun1 cfg r/w. Just return all 0xFF for temp*/
	else if (rg_func_num == 1)
		rg_cdm_data = 0xFFFFFFFF;

	writel(rg_cdm_data, CDM_MBOX_DATA);
	writel(readl(CDM_MBOX_CTRL) | BIT_31, CDM_MBOX_CTRL);
}
EXPORT_SYMBOL(config_rw_handler);

void rc_link_down_handler(rtk_pci_dev *pdev)
{
	volatile INT8U pci_info;

#if 0
	REGX16(WIFI_ISR) |= BIT_13; // clean ISR
#else
	writew(BIT_13, WIFI_ISR);
#endif

	pdev->nRC_link_down++;
	printk("[PCIE] RC link trigger%d, reset the config\r\n", pdev->nRC_link_down);
	pcieh_sw_reset(BIT_1);

	if (pdev->Bypass_mode_wocfg_flag) {
		pci_info = oobm_read_pci_info();
		oobm_write_pci_info(pci_info | RC_LINK_DOWN);
	}
}
EXPORT_SYMBOL(rc_link_down_handler);

static void fun0_init(void)
{
	writel(BIT_21|BIT_22, PCIE_FUN0_ISR);
	writeb(0x0, HOST_PAD_SRC_SEL_REG);
}

static INT32U rc_indi_cfg_init(void)
{
	volatile INT32U Rvalue, ph_result = PH_SUCCESS, Regdata;
	volatile USHORT ph_addr;

	pcieh_set_imr_wo_devint();	//set IMR of rc_indr_int_msk

	//enable device message interrupt
	Regdata = readl(SII_INT_CR);
	Regdata |= BIT_0;
	writel(Regdata, SII_INT_CR);

	//set rc IO base addr
	ph_addr = 0x1c;
	ph_result |= pcieh_rc32_read(ph_addr, &Rvalue);
	Rvalue = Rvalue >> 16;
	Rvalue = Rvalue << 16;
	Rvalue |= 0xD0D0;
	ph_result |= pcieh_rc32_write(ph_addr, Rvalue);
	ph_result |= pcieh_rc32_read(ph_addr, &Rvalue);

	//set RC IO enable memory eable bit
	ph_result |= pcieh_rc32_read(0x4, &Rvalue);
	ph_result |= pcieh_rc32_write(0x4, Rvalue | 0x7);

	return ph_result;
}

static INT32U pcieh_bar_init(rtk_pci_dev* pdev, volatile USHORT bus, volatile USHORT dev, volatile USHORT fun)
{
	volatile INT32U tmpd, reg, config_addr, smask, omask, amask, size, reloc, min_align, base, ph_result;
	static volatile INT32U io_base,mem_base;

	io_base = IO_BASE_ADDR;
	mem_base = MEM_BASE_ADDR;
	ph_result = PH_SUCCESS;
	omask = 0x00000000;

	for (reg = 0; reg < 6; reg++) {
		config_addr = BADDR0 + reg * 4;
		/* get region size */
		ph_result |= pcieh_cfg32_write(bus, dev, fun, config_addr, 0xffffffff);
		ph_result |= pcieh_cfg32_read(bus, dev, fun, config_addr, &smask);

		if (smask == 0x00000000 || smask == 0xffffffff) {
			pdev->Vendor_cfg_bar[reg][0] = 0;
			pdev->Vendor_cfg_bar[reg][1] = 0;
			ph_result |= pcieh_cfg32_write(bus, dev, fun, config_addr, 0x0);
			continue;
		}

		if (smask & 0x00000001) {
			/* I/O space */
			min_align = 1 << 7;
			amask = 0x00000001;
			base = io_base;
			pdev->Vendor_cfg_bar[reg][0] = 0x1;
		}
		else {
			/* Memory Space */
			min_align = 1 << 16;
			amask = 0x0000000F;
			pdev->Vendor_cfg_bar[reg][0] = smask &0xEF;
			base = mem_base;
		}

		omask = smask & amask;
		smask &= ~amask;
		size = (~smask) + 1;
		pdev->Vendor_cfg_bar[reg][1] = size;
		reloc = base;

		if (size < min_align)
			size = min_align;

		reloc = (reloc + size -1) & ~(size - 1);

		if (io_base == base)
			io_base = reloc + size;
		else
			mem_base = reloc + size;

		ph_result |= pcieh_cfg32_write(bus, dev, fun, config_addr, reloc|omask);

	}

	ph_result |= pcieh_cfg32_read(bus, dev, fun, BADDR0, &tmpd);
	pdev->io_addr = tmpd - 1;
	ph_result |= pcieh_cfg32_read(bus, dev, fun, BADDR2, &tmpd);
	pdev->mmio_addr = tmpd - 4;

	return ph_result;
}

static INT32U cfg_space_init(rtk_pci_dev* pdev, volatile USHORT bus, volatile USHORT dev, volatile USHORT fun)
{
	volatile INT32U tmpd, Wvalue, ph_result;

	pdev->bus = bus;
	pdev->dev = dev;
	pdev->fun = fun;

	/* back up the wifi device cfg space in vendor cfg table if there no default cfg*/
	if (pdev->Default_cfg[0][0] == 0x0) {
		ph_result |= backup_vendor_cfg(bus, dev, fun, pdev->Default_cfg[0]);
		memcpy(&pdev->Vendor_cfg, &pdev->Default_cfg, sizeof(pdev->Default_cfg));
		pdev->Vendor_cfg[0][0] = 0x816510EC;
	} else  {
		/*Resume default cfg if there has default cfg*/
		ph_result |= default_table_2_wificfg(pdev);
	}

	//cfg cmd reg
	ph_result |= pcieh_cfg32_write(bus, dev, fun, CMD_STATUS_REG, 0x00100007);

	//cfg interrupt
	ph_result |= pcieh_cfg32_read(bus, dev, fun, INT_REG, &tmpd);
	Wvalue = ((tmpd >> 8) << 8) | BSP_Device_IRQ;
	ph_result |= pcieh_cfg32_write(bus, dev, fun, INT_REG, Wvalue);
	ph_result |= pcieh_cfg32_read(bus, dev, fun, INT_REG, &tmpd);

	pdev->PM_status_offset = search_pid(bus, dev, fun, PCI_PM_CAPID, PMCSR);
	pdev->ASPM_status_offset = search_pid(bus, dev, fun, PCIE_CAPID, LINK_CTRL_REG);

	/*set D0 status*/
	ph_result |= pcieh_cfg8_read(bus, dev, fun, pdev->PM_status_offset, &tmpd);
	ph_result |= pcieh_cfg8_write(bus, dev, fun, pdev->PM_status_offset, tmpd & 0xFC);

	/*Close ASPM*/
	ph_result |= pcieh_cfg8_read(bus, dev,fun, pdev->ASPM_status_offset, &tmpd);
	tmpd &= 0xFC;
	tmpd |= BIT_6;
	ph_result |= pcieh_cfg8_write(bus, dev, fun, pdev->ASPM_status_offset, tmpd);

	/* Ted */
#if 0
	ph_result |= pcieh_cfg8_write(bus, dev, fun, 0x79, 0x00);	//cfg max request size
#endif

	//cfg base addr
	ph_result |= pcieh_bar_init(pdev, bus, dev, fun);

	return ph_result;

}

static INT32U device_scan(rtk_pci_dev* pdev)
{
	volatile INT32U i, j, k, tmpd, ph_result = PH_ERROR_NO_DEV;
	volatile INT8U pci_info;

	pdev->FunNo = 0;

	for (i = 0; i < BUS_NUM_T; i++) {
		for (j = 0; j < DEV_NUM_T; j++) {
			for (k = 0; k < FUN_NUM_T; k++) {
				if (pcieh_cfg32_read(i, j, k, 0, &tmpd) && tmpd != 0xFFFFFFFF) {
					pdev->FunNo++;
					ph_result = PH_SUCCESS;
					ph_result |= cfg_space_init(pdev, i, j, k);
					/*There has a slave device connect to PCIE host*/
					pci_info = oobm_read_pci_info();
					oobm_write_pci_info(pci_info | PCI_DEV_ON);
				}
			}
		}
	}

	return ph_result;
}

void pcieh_enable_power_seq(void)
{
	Iso_control(1);
	msleep(100); //each tick is 10ms, total delay 100ms
	prst_control(1);

}

void pcieh_disable_pwr_seq(void)
{
	prst_control(0);
	msleep(100); //each tick is 10ms, total delay 100ms
	Iso_control(0);
	msleep(100); //each tick is 10ms, total delay 100ms
}
EXPORT_SYMBOL(pcieh_disable_pwr_seq);

INT32U adapter_cfg_init(rtk_pci_dev *pdev)
{
	INT32U ph_result = PH_SUCCESS, i = 0, rescan_num = 0;

	_2_4281_cfg(pdev);

	fun0_init();

	writew(readw(OOBMAC_BASE_ADDR + OOBMAC_IMR) | BIT_6, OOBMAC_BASE_ADDR + OOBMAC_IMR);

init:
	pcieh_enable_power_seq();

	/*polling the L0 states*/
	do {
		i++;
		msleep(100);
	} while (!(readb(RC_MAC_STATUS_REG) & BIT_0) && i <= PCIE_SCAN_TIME_OUT);

	if (i > PCIE_SCAN_TIME_OUT) {
		if (rescan_num < PCIE_RESCAN_MAX) {
			pcieh_disable_pwr_seq();
			rescan_num++;
			i = 0;
			goto init;
		} else {
			printk("[PCIE] polling L0 state failed\n");
			ph_result |= PH_ERROR_TIMEOUT;
		}
	} else {
		ph_result |= rc_indi_cfg_init();
		ph_result |= device_scan(pdev);
	}

	return ph_result;
}

INT32U pcieh_init(rtk_pci_dev *pdev)
{
	INT32U ph_result = PH_SUCCESS, pci_bus_scan_status = PH_SUCCESS;
	volatile INT8U pci_info;

	printk("[PCIE] start the connection\r\n");

	pci_bus_scan_status = adapter_cfg_init(pdev);

	/* Notify the window driver that pcie host has been enable*/
	pci_info = oobm_read_pci_info();
	oobm_write_pci_info(pci_info | PH_ENABLE);

	if (pci_bus_scan_status &
		(PH_ERROR_PAGEFULL | PH_ERROR_WRONGVALUE |PH_ERROR_NO_DEV | PH_ERROR_TIMEOUT))
	{
		ph_result = PH_ERROR_PCIELINK_FAIL;
		goto error;
	}
	else if (pdev->Vendor_cfg[0][0] != 0xFFFFFFFF)
	{
		ph_result = 0;
		if (!ph_result) {
			pcieh_set_imr_with_devint();
			printk("[PCIE] init done\r\n");
		} else {
			ph_result = PH_ERROR_NODRV;
			goto error;
		}
	}
	else
	{
		ph_result = PH_ERROR_UNKNOWN;
		goto error;
	}

	return ph_result;

error:
	/*set the device_on bit 0*/
	pci_info = oobm_read_pci_info();
	oobm_write_pci_info(pci_info & ~(PCI_DEV_ON));
	return ph_result;
}
EXPORT_SYMBOL(pcieh_init);

INT32U pcieh_exit(rtk_pci_dev *pdev)
{
#if 0
	prst_control(0);
	Iso_control(0);
	memset(pdev, 0, sizeof(rtk_pci_dev));
#endif
	return 0;
}

static void rc_ephy_init(void)
{
	//RC ephy parameter
	Rc_ephy_W(0x0, 0x584E);
	Rc_ephy_W(0x6, 0xF0F0);
	Rc_ephy_W(0xC, 0x219);
	Rc_ephy_W(0xD, 0xF64);
	Rc_ephy_W(0x1E, 0x08F5);
}

/***********************************************
Function: Addr2Byte_en
Return Value: other	: successful
			0		: Failure

This function expect the rertuen value:
0x0011, 0x0110, 0x1100 			for Word
0x0001, 0x0010, 0x0100, 0x1000 		for byte
************************************************/
static USHORT Addr2Byte_en(volatile USHORT addr, volatile USHORT byte_init)
{
	volatile USHORT byte_off, byte_en;

	byte_en = byte_init;
	byte_off = addr % 4;
	byte_en = byte_en << byte_off;

	if (byte_init == 0x3) {
		if (!((byte_en & 0x3) || (byte_en & 0x6) || (byte_en & 0xc)))
			byte_en = 0;
	} else if (byte_init == 0x1) {
		if (!((byte_en & BIT_0) || (byte_en & BIT_1) || (byte_en & BIT_2) || (byte_en & BIT_3)))
			byte_en = 0;
	}

	return byte_en;
}

static INT32U Value2Byte_en_Word(INT32U byte_en, INT32U value_temp, INT8U cmd)
{
	INT32U value = 0;

	switch (byte_en) {
	case 0x3:
		value = value_temp;
		break;
	case 0x6:
		if (cmd == R_CMD)
			value = value_temp >> 8;
		else if (cmd == W_CMD)
			value = value_temp <<8;
		break;
	case 0xc:
		if (cmd == R_CMD)
			value = value_temp >> 16;
		else if (cmd == W_CMD)
			value = value_temp << 16;
		break;
	default:
		break;
	}

	return value;
}

static INT32U Value2Byte_en_Byte(INT32U byte_en, INT32U value_temp, INT8U cmd)
{
	INT32U value = 0;

	switch (byte_en) {
	case 0x1:
		value = (INT8U)value_temp;
		break;
	case 0x2:
		if (cmd == R_CMD)
			value = value_temp >> 8;
		else if (cmd == W_CMD)
			value = value_temp << 8;
		break;
	case 0x4:
		if (cmd == R_CMD)
			value = value_temp >> 16;
		else if (cmd == W_CMD)
			value = value_temp << 16;
		break;
	case 0x8:
		if (cmd == R_CMD)
			value = value_temp >> 24;
		else if (cmd == W_CMD)
			value = value_temp << 24;
		break;
	default:
		break;
	}

	return value;
}

ULONG Send_TLP_Polling(
	volatile INT32U TLP_TPYE,	//TLP Type
	volatile INT32U RW,		//Read or Write
	volatile INT32U Base_addr_H,	//high Base addr
	volatile INT32U Base_addr_L,	//low base addr
	volatile INT32U offset,		//offset addr
	volatile INT32U first_byte_en,	//first byte enable
	volatile INT32U last_byte_en,	//last byte enalbe
	volatile INT32U LEN,		//length
	volatile INT32U Timeout,	//polling time out num
	volatile INT32U *value		//Read or write data ptr
	)
{
	volatile struct RcPage *hostpage,hostpaget;
	volatile struct CFG_ADDR *cfg_addr;
	volatile INT32U tmp = 0,*pg_value, addr , rv = PH_SUCCESS, pg_wr_ptr, Rhostpage;

//	OS_CPU_SR  cpu_sr = 0;

	OS_ENTER_CRITICAL;

	hostpage = &hostpaget;

	/*check page full*/
	do{
		tmp++;
	} while((readl(PAGE_STATUS_REG) & BIT_7) && (tmp < 10000));	//check page full

	if (tmp >= 10000) {
		//reset rc ephy and pcie

		OOBMACWriteIBReg(EN_CLK_REG1, 1, 1, 0);
		OOBMACWriteIBReg(EN_CLK_REG1, 1, 1, 1);

		writel(1, WIFIDASH_SOFT_RESET);
		writel(0, WIFIDASH_SOFT_RESET);

		rv = PH_ERROR_PAGEFULL;
		goto Send_TLP_Polling_EXIT;
	}

	pg_wr_ptr = (readl(WIFI_DASH_BASE_ADDR + RC_INDIRECT_CH_OFFSET) & 0x18) >> 3;

	/*get the page should be using now*/
	Rhostpage = WIFI_DASH_BASE_ADDR + RC_INDIRECT_CH_OFFSET+ PAGE_CTRL_OFFSET*(pg_wr_ptr + 1);

	/*get the pg_value offset*/
	pg_value = (INT32U *)(WIFI_DASH_BASE_ADDR + PAGE0_DATA_B3_B0 + PAGE_DATA_OFFSET*pg_wr_ptr);

	/*rst the value of pg_value and page*/

	writel(0, Rhostpage);
	writel(0, Rhostpage + 0x4);
	writel(0, (INT32U)pg_value);

	/*cfg  page*/
	hostpage->EP = 0;
	hostpage->LEN = LEN;
	hostpage->LAST_BE = last_byte_en;
	hostpage->FIRST_BE = first_byte_en;
	hostpage->ADDRH = Base_addr_H;

	switch (TLP_TPYE) {
	case CFG_TYPE:
		cfg_addr = (struct CFG_ADDR *)((INT8U *)hostpage + 8);
		addr = offset/4;
		hostpage->TLP_TYPE = CFG_TYPE;
		cfg_addr->bus = Base_addr_L >> 8;
		cfg_addr->dev = (Base_addr_L << 8) >> 11;
		cfg_addr->fun = (Base_addr_L << 13) >> 13;
		cfg_addr->reg_num = addr;
		break;
	case IO_TYPE:
		hostpage->TLP_TYPE = IO_TYPE;
		addr= Base_addr_L + offset;
		hostpage->ADDRL = addr;
		break;
	case MEM_TYPE:
		hostpage->TLP_TYPE = MEM_TYPE;
		addr = Base_addr_L + offset;
		hostpage->ADDRL = addr;
		break;
	default:
		rv = 4;
		goto Send_TLP_Polling_EXIT;
		break;
	}

	writel(*((INT32U *)hostpage), Rhostpage);
	writel(hostpage->ADDRL, Rhostpage + 0x8);
	writel(hostpage->ADDRH, Rhostpage + 0xC);

	if (RW == R_CMD) {
		hostpage->CMD = R_CMD;
		hostpage->OWN = 1;

		writel(*((INT32U *)hostpage + 1), Rhostpage + 0x4);

		for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
			if (!(readl(Rhostpage + 0x4) & BIT_31)) {
				*value = readl((INT32U)pg_value);

				writel(readl(Rhostpage + 0x4) | BIT_28, Rhostpage + 0x4);

				rv = PH_SUCCESS;
				goto Send_TLP_Polling_EXIT;
			}
		}
	} else if (RW == W_CMD) {
		hostpage->CMD = W_CMD;
		hostpage->OWN = 1;

		writel(*value, (INT32U)pg_value);
		writel(*((INT32U *)hostpage + 1), Rhostpage + 0x4);

		for (tmp = 0 ;tmp < PCIE_TIME_OUT; tmp++) {
			if (!(readl(Rhostpage + 0x4) & BIT_31)) {
				writel(readl(Rhostpage + 0x4) | BIT_28, Rhostpage + 0x4);//clear done bit
				rv = PH_SUCCESS;
				goto Send_TLP_Polling_EXIT;
			}
		}
	}
	rv |= PH_ERROR_TIMEOUT;

Send_TLP_Polling_EXIT:

	OS_EXIT_CRITICAL;
	return rv;
}

ULONG pcieh_cfg32_read(volatile USHORT bus, volatile USHORT dev,
								volatile USHORT fun, volatile USHORT addr, volatile INT32U *value)
{
	volatile INT32U Base_addr_L = 0, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF, WIFI_IMR);

	Base_addr_L = bus;
	Base_addr_L = (Base_addr_L << 5) + dev;
	Base_addr_L = (Base_addr_L<< 3) + fun;
	ph_result = Send_TLP_Polling(CFG_TYPE, R_CMD, 0, Base_addr_L, addr, 0b1111, 0, 1, PCIE_TIME_OUT, value);

	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}
EXPORT_SYMBOL(pcieh_cfg32_read);

ULONG pcieh_cfg32_write(volatile USHORT bus, volatile USHORT dev,
								volatile USHORT fun, volatile USHORT addr, volatile INT32U value)
{
	volatile INT32U Base_addr_L = 0, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/

	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	Base_addr_L = bus;
	Base_addr_L = (Base_addr_L << 5) + dev;
	Base_addr_L = (Base_addr_L << 3) + fun;
	ph_result = Send_TLP_Polling(CFG_TYPE, W_CMD, 0, Base_addr_L, addr, 0b1111, 0, 1, PCIE_TIME_OUT, &value);

	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}
EXPORT_SYMBOL(pcieh_cfg32_write);

ULONG pcieh_cfg16_read(volatile USHORT bus, volatile USHORT dev,
								volatile USHORT fun, volatile USHORT addr, volatile INT32U *value)
{
	volatile INT32U Base_addr_L = 0, byte_en, value_temp, ph_result = 0;
	volatile INT32U IMRvalue;

	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(addr, 0x03);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	Base_addr_L = bus;
	Base_addr_L = (Base_addr_L << 5) + dev;
	Base_addr_L = (Base_addr_L << 3) + fun;

	ph_result = Send_TLP_Polling(CFG_TYPE, R_CMD, 0, Base_addr_L, addr, byte_en, 0, 1, PCIE_TIME_OUT, &value_temp);
	*value = (INT16U)Value2Byte_en_Word(byte_en, value_temp, R_CMD);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}
EXPORT_SYMBOL(pcieh_cfg16_read);

ULONG pcieh_cfg16_write(volatile USHORT bus, volatile USHORT dev,
								volatile USHORT fun, volatile USHORT addr, volatile INT32U value)
{
	volatile INT32U Base_addr_L = 0, byte_en, value_temp, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(addr, 0x03);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	Base_addr_L = bus;
	Base_addr_L = (Base_addr_L << 5) + dev;
	Base_addr_L = (Base_addr_L << 3) + fun;
	value_temp = value;

	value = Value2Byte_en_Word(byte_en, value_temp, W_CMD);
	ph_result = Send_TLP_Polling(CFG_TYPE, W_CMD, 0, Base_addr_L, addr, byte_en, 0, 1, PCIE_TIME_OUT, &value);

end:

	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}
EXPORT_SYMBOL(pcieh_cfg16_write);

ULONG pcieh_cfg8_read(volatile USHORT bus, volatile USHORT dev,
								volatile USHORT fun, volatile USHORT addr, volatile INT32U *value)
{
	volatile INT32U Base_addr_L = 0, byte_en, value_temp, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(addr, 0x01);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	Base_addr_L = bus;
	Base_addr_L = (Base_addr_L << 5) + dev;
	Base_addr_L = (Base_addr_L << 3) + fun;

	ph_result = Send_TLP_Polling(CFG_TYPE, R_CMD, 0, Base_addr_L, addr, byte_en, 0, 1, PCIE_TIME_OUT, &value_temp);
	*value = (INT8U)Value2Byte_en_Byte(byte_en, value_temp, R_CMD);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}
EXPORT_SYMBOL(pcieh_cfg8_read);

ULONG pcieh_cfg8_write(volatile USHORT bus, volatile USHORT dev,
							volatile USHORT fun, volatile USHORT addr, volatile INT32U value)
{
	volatile INT32U Base_addr_L = 0, byte_en, value_temp, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(addr, 0x01);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	Base_addr_L = bus;
	Base_addr_L = (Base_addr_L << 5) + dev;
	Base_addr_L = (Base_addr_L << 3) + fun;
	value_temp = value;

	value = Value2Byte_en_Byte(byte_en, value_temp, W_CMD);
	ph_result = Send_TLP_Polling(CFG_TYPE, W_CMD, 0, Base_addr_L, addr, byte_en, 0, 1, PCIE_TIME_OUT, &value);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}
EXPORT_SYMBOL(pcieh_cfg8_write);

ULONG pcieh_io32_read(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U *value)
{
	volatile INT32U IMRvalue, ph_result = 0;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	ph_result = Send_TLP_Polling(IO_TYPE, R_CMD, 0, io_base, io_addr, 0b1111, 0, 1, PCIE_TIME_OUT, value);

	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}

ULONG pcieh_io32_write(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U value)
{
	volatile INT32U IMRvalue,ph_result;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	ph_result = Send_TLP_Polling(IO_TYPE, W_CMD, 0, io_base, io_addr, 0b1111, 0, 1, PCIE_TIME_OUT, &value);

	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}

ULONG pcieh_io16_read(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U *value)
{
	volatile INT32U byte_en, value_temp, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(io_addr, 0x03);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	ph_result = Send_TLP_Polling(IO_TYPE, R_CMD, 0, io_base, io_addr, byte_en, 0, 1, PCIE_TIME_OUT, &value_temp);
	*value = (INT16U)Value2Byte_en_Word(byte_en, value_temp, R_CMD);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}

ULONG pcieh_io16_write(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U value)
{
	volatile INT32U byte_en, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(io_addr, 0x03);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	value = Value2Byte_en_Word(byte_en, value, W_CMD);
	ph_result = Send_TLP_Polling(IO_TYPE, W_CMD, 0, io_base, io_addr, byte_en, 0, 1, PCIE_TIME_OUT, &value);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}

ULONG pcieh_io8_read(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U *value)
{
	volatile INT32U byte_en, value_temp, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(io_addr, 0x01);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	ph_result = Send_TLP_Polling(IO_TYPE, R_CMD, 0, io_base, io_addr, byte_en, 0, 1, PCIE_TIME_OUT, &value_temp);
	*value =(INT8U)Value2Byte_en_Byte(byte_en, value_temp, R_CMD);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}

ULONG pcieh_io8_write(volatile INT32U io_base, volatile USHORT io_addr, volatile INT32U value)
{
	volatile INT32U byte_en, ph_result = 0;
	volatile INT32U IMRvalue;

	/*close imr of msg interrupt from device*/
	IMRvalue = readl(WIFI_IMR);
	writel(IMRvalue & 0xFFFFFFF0, WIFI_IMR);

	byte_en = Addr2Byte_en(io_addr, 0x01);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	value = Value2Byte_en_Byte(byte_en, value, W_CMD);
	ph_result = Send_TLP_Polling(IO_TYPE, W_CMD, 0, io_base, io_addr, byte_en, 0, 1, PCIE_TIME_OUT, &value);

end:
	writel(IMRvalue, WIFI_IMR);
	return ph_result;
}

u32 pcieh_mem32_read(volatile u32 Haddr, volatile u32 Laddr, volatile u32 *value)
{
	volatile INT32U ph_result;

	ph_result = Send_TLP_Polling(MEM_TYPE, R_CMD, Haddr, 0, Laddr, 0b1111, 0, 1, PCIE_TIME_OUT, value);

	return ph_result;
}
EXPORT_SYMBOL(pcieh_mem32_read);

u32 pcieh_mem32_write(volatile u32 Haddr, u32 Laddr, u32 value)
{
	volatile INT32U ph_result;

	ph_result = Send_TLP_Polling(MEM_TYPE, W_CMD, Haddr, 0, Laddr, 0b1111, 0, 1, PCIE_TIME_OUT, &value);

	return ph_result;
}
EXPORT_SYMBOL(pcieh_mem32_write);

u32 pcieh_mem16_read(volatile u32 Haddr, volatile u32 Laddr, volatile u32 *value)
{
	volatile INT32U byte_en, value_temp, ph_result = 0;

	byte_en = Addr2Byte_en(Laddr, 0x03);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	ph_result = Send_TLP_Polling(MEM_TYPE, R_CMD, Haddr, 0, Laddr, byte_en, 0, 1, PCIE_TIME_OUT, &value_temp);
	*value = (INT16U)Value2Byte_en_Word(byte_en, value_temp, R_CMD);

end:
	return ph_result;
}
EXPORT_SYMBOL(pcieh_mem16_read);

u32  pcieh_mem16_write(volatile u32 Haddr, volatile u32 Laddr, volatile u32 value)
{
	volatile INT32U byte_en, ph_result = 0;

	byte_en = Addr2Byte_en(Laddr, 0x03);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	value = Value2Byte_en_Word(byte_en, value, W_CMD);
	ph_result = Send_TLP_Polling(MEM_TYPE, W_CMD, Haddr, 0, Laddr, byte_en, 0, 1, PCIE_TIME_OUT, &value);

end:
	return ph_result;
}
EXPORT_SYMBOL(pcieh_mem16_write);

u32 pcieh_mem8_read(volatile u32 Haddr, volatile u32 Laddr, volatile u32 *value)
{
	volatile INT32U byte_en, value_temp, ph_result = 0;

	byte_en = Addr2Byte_en(Laddr, 0x01);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	ph_result = Send_TLP_Polling(MEM_TYPE, R_CMD, Haddr, 0, Laddr, byte_en, 0, 1, PCIE_TIME_OUT, &value_temp);
	*value =(INT8U)Value2Byte_en_Byte(byte_en, value_temp, R_CMD);

end:
	return ph_result;
}
EXPORT_SYMBOL(pcieh_mem8_read);

u32 pcieh_mem8_write(volatile u32 Haddr, volatile u32 Laddr, volatile u32 value)
{
	volatile INT32U byte_en, ph_result = 0;

	byte_en = Addr2Byte_en(Laddr, 0x01);
	if (!byte_en) {
		ph_result = PH_ERROR_WRONGVALUE;
		goto end;
	}

	value = Value2Byte_en_Byte(byte_en, value, W_CMD);
	ph_result = Send_TLP_Polling(MEM_TYPE, W_CMD, Haddr, 0, Laddr, byte_en, 0, 1, PCIE_TIME_OUT, &value);

end:
	return ph_result;
}
EXPORT_SYMBOL(pcieh_mem8_write);

/*
OOBMAC access IBMAC : DWORD alignment bit 31-0   OCP reg
value: relative to value of lowBit to highBit
*/
INT32U OOBMACReadIBReg(INT16U reg)
{
	writel(0x800f0000 | reg, OOBMAC_IB_ACC_SET);
	while((readl(OOBMAC_IB_ACC_SET) & 0x80000000));

	return readl(OOBMAC_IB_ACC_DATA);
}

void OOBMACWriteIBReg(INT16U reg, INT8U highBit, INT8U lowBit, INT32U value)
{
	INT32U reg_cmd = 0, reg_data = 0;
	INT32U oriValue = 0x00000000, inputValue = 0x00000000, maskValue = 0x00000000;
	INT8U i = 0;

	reg_cmd = OOBMAC_IB_ACC_SET;
	reg_data = OOBMAC_IB_ACC_DATA;

	oriValue = OOBMACReadIBReg(reg);

	for (i = lowBit; i <= highBit; i++)
		maskValue |= (1<<i);

	maskValue = ~maskValue;
	inputValue = (oriValue & maskValue) | (value << lowBit);

	writel(inputValue, reg_data);
	writel(0x808f0000 | reg, reg_cmd);

	while ((readl(reg_cmd) & 0x80000000));
}

void OOB2IB_W(INT16U reg, INT8U byte_en, INT32U value)
{
	writel(value, OOBMAC_IB_ACC_DATA);
	writel((0x80800000 | (byte_en << 16) | reg), OOBMAC_IB_ACC_SET);

	while ((readl(OOBMAC_IB_ACC_SET) & 0x80000000));
}

void Rc_ephy_W(INT32U reg, INT32U data)
{
	INT32U cmd = 0x80000000;

	cmd |= (reg << 16);
	cmd |= data;
	OOB2IB_W(EPHY_MDCMDIO_RC_DATA, 0xF, cmd);

	while(OOBMACReadIBReg(EPHY_MDCMDIO_RC_DATA) & BIT_31);
}

int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return of_irq_parse_and_map_pci(dev, slot, pin);
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	pcie_capability_clear_and_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_READRQ, PCI_EXP_DEVCTL_READRQ_128B);

	return 0;
}

void init_pci_setup(rtk_pci_dev *pdev)
{
	this_pdev = pdev;

	/*The pin contorl need init first or isolate pin control will cause HW abnormal*/
	OOB2IB_W(PIN_C, 0xF, OOBMACReadIBReg(PIN_C) | (BIT_16 | BIT_17 | BIT_18));
	pcieh_sw_reset(BIT_1);
	rc_ephy_init();

	msleep(100);
}
EXPORT_SYMBOL(init_pci_setup);

//Virtual CFG Read for mutifunction
u32 _Read_Vt_PCIDword(volatile USHORT addr, volatile USHORT fun_num, volatile INT32U *value, volatile USHORT cs2)
{
	volatile INT32U tmp = 0, rv = PH_ERROR_TIMEOUT, EP_dbi_addr = 0;
//	OS_CPU_SR cpu_sr = 0;

	OS_ENTER_CRITICAL;

	if (!this_pdev->EP_prst_status) {
		printk("[PCIE] _Read_Vt_PCIDword 000000\n");
		rv = 0xFFFFFFFF;
		goto exit;
	}

	if (readb(BYP_MODE_CFG_REG) == 3 && (fun_num < 5 || fun_num > 6)) {
		printk("[PCIE] _Read_Vt_PCIDword 11111\n");
		rv = 0xFFFFFFFF;
		goto exit;
	}

	EP_dbi_addr |= addr;
	EP_dbi_addr |= (fun_num << 16);
	EP_dbi_addr |= (cs2 << 19);

	writel(EP_dbi_addr, EP_DBI_ADDR);
	writel(0x3, EP_DBI_CTRL_REG);

	for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
		if (!(readl(EP_DBI_CTRL_REG) & BIT_0)) {
			*value = readl(EP_DBI_RDATA);
			rv  = PH_SUCCESS;
			break;
		}
	}
	OS_EXIT_CRITICAL;

exit:

	if (rv != PH_SUCCESS)
		printk("[PCIE] _Read_Vt_PCIDword fauled, addr = 0x%x, fun_num = 0x%x\n", addr, fun_num);

	return rv;
}

//Virtual CFG Write for mutifunction
u32 _Write_Vt_PCIDword(volatile USHORT addr, volatile USHORT fun_num, volatile ULONG value,
								volatile INT32U first_byte_en, volatile USHORT cs2)
{
	volatile INT32U tmp = 0, rv = PH_ERROR_TIMEOUT, EP_dbi_addr = 0, cmd = 0;
//	OS_CPU_SR  cpu_sr = 0;

	OS_ENTER_CRITICAL;

	if (!this_pdev->EP_prst_status) {
		printk("[PCIE] _Write_Vt_PCIDword 000000\n");
		rv = 0xFFFFFFFF;
		goto exit;
	}

	if (readb(BYP_MODE_CFG_REG) == 3 && (fun_num < 5 || fun_num > 6)) {
		printk("[PCIE] _Write_Vt_PCIDword 111111\n");
		rv = 0xFFFFFFFF;
		goto exit;
	}

	EP_dbi_addr |= addr;
	EP_dbi_addr |= (fun_num << 16);
	EP_dbi_addr |= cs2 << 20;

	writel(EP_dbi_addr, EP_DBI_ADDR);
	writel(value, EP_DBI_WDATA);

	cmd |= (first_byte_en << 3);
	cmd |= 5;

	writel(cmd, EP_DBI_CTRL_REG);

	for (tmp = 0; tmp < PCIE_TIME_OUT; tmp++) {
		if (!(readl(EP_DBI_CTRL_REG) & BIT_0) ) {
			rv = PH_SUCCESS;
			break;
		}
	}

	OS_EXIT_CRITICAL;

exit:
	if (rv != PH_SUCCESS)
		printk("[PCIE] _Read_Vt_PCIDword fauled, addr = 0x%x, fun_num = 0x%x\n", addr, fun_num);
	return rv;

}

u32 Read_Vt_PCIDword(volatile USHORT addr, volatile USHORT fun_num, volatile INT32U *value)
{
	return _Read_Vt_PCIDword(addr, fun_num, value, 0);
}
EXPORT_SYMBOL(Read_Vt_PCIDword);

u32 Write_Vt_PCIDword(volatile USHORT addr, volatile USHORT fun_num, volatile ULONG value)
{
	return _Write_Vt_PCIDword(addr, fun_num, value, 0xF, 0);
}

static void pcieh_reset_cfg_space(rtk_pci_dev *pdev)
{
	memset(&pdev->Default_cfg, 0, sizeof(pdev->Default_cfg)); //reset the default table
	memset(&pdev->Vendor_cfg, 0, sizeof(pdev->Vendor_cfg)); //reset the vendor table
}

void pcieh_set_sys_pwr_state(rtk_pci_dev *pdev ,INT8U power_state)
{
	switch (power_state) {
	case SYS_PWR_S0:
		pdev->S3_Flag = 0;
		pdev->S4_Flag = 0;
		pdev->S5_Flag = 0;
		break;
	case SYS_PWR_S3:
		pdev->S3_Flag = 1;
		pdev->S4_Flag = 0;
		pdev->S5_Flag = 0;
		break;
	case SYS_PWR_S4:
		pdev->S3_Flag = 0;
		pdev->S4_Flag = 1;
		pdev->S5_Flag = 0;
		break;
	case SYS_PWR_S5:
		pdev->S3_Flag = 0;
		pdev->S4_Flag = 0;
		pdev->S5_Flag = 1;
		break;
	default:
		break;
	}
}

INT32U pcieh_scan_bus(rtk_pci_dev *pdev)
{
	volatile INT32U ph_result = PH_SUCCESS, i = 0;

	_2_4281_cfg(pdev);

	/*polling the L0 states*/
	do {
		i++;
	} while (!(readb(RC_MAC_STATUS_REG)) && i <= PCIE_SCAN_TIME_OUT);


	if (i > PCIE_TIME_OUT) {
		printk("[PCIE] polling link state failed\n");
		ph_result = PH_ERROR_TIMEOUT;
	} else {
		ph_result |= rc_indi_cfg_init();
		ph_result |= device_scan(pdev);
	}

	return ph_result;
}

INT32U pcieh_enable_dev(rtk_pci_dev *pdev)
{
#if 0
	if (pdev->pdrv->probe)
		pdev->pdrv->probe(pdev);
#endif
	pcieh_set_imr_with_devint();

	return 0;
}

int pcieh_disable_dev(rtk_pci_dev *pdev)
{
#if 0
	if (pdev->pdrv->remove)
		pdev->pdrv->remove(pdev);
#endif
	pcieh_set_imr_wo_devint();

	return 0;
}

static void pcie_dash_set_msg(INT8U mode)
{
	oobm_write_pci_msg(mode);
	oobm_write_pci_cmd(0);
}

INT32U _2_bypass_wo_cfg(rtk_pci_dev *pdev)
{
	OS_ENTER_CRITICAL;

	vdtable_2_wificfg(pdev);

	writeb(0x1, WIFI_CFG0);		//rg_bypass
	writeb(readb(BYP_MODE_CFG_REG)|0x3, BYP_MODE_CFG_REG);
	writeb(readb(DUMMY_REG0) | 0x3, DUMMY_REG0);	//wifidash_cdm_byp_en: cfg tlp bypass wifidash and ASPM bind with fp device

	writeb(readb(ELBI_TRAN_CFG0) | 0x1, ELBI_TRAN_CFG0);	//set  LEBI_RTANS working at by bypass mode(0)
	writeb(readb(CDM_MBOX_CFG0) & 0xFE, CDM_MBOX_CFG0);	//set CDM_MBOX working at 4281 mode(0)

	writeb(0x00, SII_INT_CR);	//device MSG interrupt to 4281 disable;
	writeb(0xFF, SII_MSG_BYP_ENABLE_REG);	//MSG interrupt bypass to chipset enable;
	writeb(0xFF, SII_MSG_SRC_SEL);		//other msg to 4281 mode

	pcieh_set_imr_with_devint();//set interrupt

	writeb(0x3, WIFI_DIRECTION_MODE_CR);	// set mem R/W as direcion patch for dma

	pdev->Bypass_mode_wocfg_flag = 1;

	OS_EXIT_CRITICAL;

	return 1;
}

void pcieh_set_mode(rtk_pci_dev *pdev,INT8U mode)
{
	if (mode == MODE_BYPASS) {

		printk("[PCIE] pcieh_set_mode bypass\r\n");

		pdev->Vendor_cfg[0][1] = 0x00100407;
		pdev->Vendor_cfg[0][0] = pdev->Default_cfg[0][0];
		_pcieh_vt32_write(CMD_STATUS_REG, PHOST_WIFIDASH, pdev->Vendor_cfg[0][1], 0xF, 0);

		_2_bypass_wo_cfg(pdev);
	} else if (mode == MODE_4281) {
		_pcieh_vt32_write(CMD_STATUS_REG, PHOST_WIFIDASH, pdev->Vendor_cfg[0][1]&0xFFFFFFF8, 0xF, 0);
		pdev->Vendor_cfg[0][1] = 0x00100400;

		/* Ted */
#if 0
		pdev->Vendor_cfg[0][0] = pdev->Default_cfg[0][0];
#else
		pdev->Vendor_cfg[0][0] = 0x816510EC;
#endif
		_2_4281_cfg(pdev);
	}
}

void pcie_dash_switch_to_bypass(rtk_pci_dev *pdev)
{
	pcieh_disable_dev(pdev);
	pcieh_set_mode(pdev, MODE_BYPASS);
	pcie_dash_set_msg(BYPASS_MODE_RDY);
}
EXPORT_SYMBOL(pcie_dash_switch_to_bypass);

void pcie_dash_switch_to_4281(rtk_pci_dev *pdev)
{
	INT32U ret = PH_SUCCESS;
	INT8U pci_info;

	pcieh_set_mode(pdev, MODE_4281);
	ret = pcieh_scan_bus(pdev);
	if (ret != PH_SUCCESS)
		printk("[PCIE] scan bus failed\r\n");

	pcie_dash_set_msg(TO_4281_MODE_RDY);
	pcieh_enable_dev(pdev);

	pci_info = oobm_read_pci_info();
	oobm_write_pci_info(pci_info | ~RC_LINK_DOWN);
}
EXPORT_SYMBOL(pcie_dash_switch_to_4281);

void pcie_dash_switch_to_4281_in_sleep(rtk_pci_dev *pdev)
{
	INT32U pci_bus_scan_status = PH_SUCCESS;

	pdev->goto4281mode = 0;

	pci_bus_scan_status = adapter_cfg_init(pdev);
	if (pci_bus_scan_status &
		(PH_ERROR_PAGEFULL | PH_ERROR_WRONGVALUE |PH_ERROR_NO_DEV | PH_ERROR_TIMEOUT))
		goto error;

	pcieh_enable_dev(pdev);
error:
	return 0;
}
EXPORT_SYMBOL(pcie_dash_switch_to_4281_in_sleep);

#if 0
void pcie_dash_chk_mode(struct work_struct *work)
{
	rtk_pci_dev *pdev = container_of(work, rtk_pci_dev, switch_mode_schedule.work);
	volatile INT8U pci_cmd = oobm_read_pci_cmd();

	/*for switch bypass/4281 mode by system resume/boot*/
	if (pci_cmd == TO_BYPASS_MODE_SYS) {
		if (!pdev->Bypass_mode_wocfg_flag)
			pcie_dash_switch_to_bypass(pdev);
		else
			pcie_dash_set_msg(BYPASS_MODE_RDY);

		printk("[PCIE] TO_BYPASS_MODE_SYS Ready\r\n");
	}

	/*switch to 4281 mode by client*/
	else if (pci_cmd == TO_4281_MODE) {
		if (pdev->Bypass_mode_wocfg_flag)
			pcie_dash_switch_to_4281(pdev);
		else
			pcie_dash_set_msg(TO_4281_MODE_RDY);

		printk("[PCIE] TO_4281_MODE Ready\r\n");
	}

	/* switch to bypass mode by client tool*/
	else if (pci_cmd == TO_BYPASS_MODE) {
		if (!pdev->Bypass_mode_wocfg_flag)
			pcie_dash_switch_to_bypass(pdev);
		else
			pcie_dash_set_msg(BYPASS_MODE_RDY);

		printk("[PCIE] TO_BYPASS_MODE Ready\r\n");
	} else if (pdev->goto4281mode) {
		pcie_dash_switch_to_4281_in_sleep(pdev);
		printk("[PCIE] TO_4281_MODE by pcie rst pull low\r\n");
	}
}
EXPORT_SYMBOL(pcie_dash_chk_mode);
#endif

//FUN0 interrupt handler
void bsp_Fun0_handler(void)
{
/*
	volatile FUN0_INT *fun0_ISR;
	volatile INT32U IMRvalue, ISRvalue, i = 0;

	IMRvalue = readw(PCIE_FUN0_IMR);
	writew(0x0000, PCIE_FUN0_IMR);
	ISRvalue = readw(PCIE_FUN0_ISR);
	writew(ISRvalue, PCIE_FUN0_ISR);

	fun0_ISR = (struct FUN0_INT*) & ISRvalue;

	if (fun0_ISR->perstb_r_sts) {
		this_pdev->EP_prst_status = 1;
#if 0
		writeb(0x0, BYP_MODE_CFG_REG);

		Write_Vt_PCIDword(VID_DID, 0x05, 0xFFFFFFFF);
		writeb(0x3, BYP_MODE_CFG_REG);
#endif
	}

	writew(IMRvalue, PCIE_FUN0_IMR);
*/
	volatile FUN0_INT *fun0_ISR;
	volatile u32 IMRvalue, ISRvalue;
	rtk_pci_dev *pdev = this_pdev;

	printk(KERN_NOTICE "[PCIE] bsp_Fun0_handler\n");

	IMRvalue = readw(PCIE_FUN0_IMR);
	writew(0x0000, PCIE_FUN0_IMR);
	ISRvalue = readw(PCIE_FUN0_ISR);
	writew(readw(PCIE_FUN0_ISR), PCIE_FUN0_ISR);	//clean ISR, W1C

	fun0_ISR = (struct FUN0_INT*) & ISRvalue;

	if (fun0_ISR->perstb_f_sts)
	{
		printk(KERN_NOTICE "[PCIE] pcie reset falling!\n");

		pdev->EP_prst_status = 0;

		if (pdev->Bypass_mode_wocfg_flag) {
			pcieh_reset_cfg_space(pdev);

			pdev->goto4281mode = 1;
			schedule_delayed_work(&pdev->switch_mode_schedule, 0);
		}
		else  {
			memcpy(&pdev->Vendor_cfg, &pdev->Default_cfg, sizeof(pdev->Default_cfg)); //adapter_cfg_init will reset cfg
			pdev->Vendor_cfg[0][0] = 0x816510EC;
		}

		writeb(readb(CDM_MBOX_CFG0) & 0xFE, CDM_MBOX_CFG0);	//set CDM_MBOX working at 4281 mode(0)

		oobm_write_pci_msg(0);
		oobm_write_pci_cmd(0);
	}

	if (fun0_ISR->perstb_r_sts)
	{
		printk(KERN_NOTICE "[PCIE] pcie reset rising!\n");

		pdev->EP_prst_status = 1;

		pcieh_vt32_write(INT_REG, PHOST_WIFIDASH, 0x100);//set for interrupt
		cfg_bar_vd2vt(pdev, PHOST_WIFIDASH);

		/* Some PC will pull low the reset pin in booting process, suck like
		  * pull high ->pull low->pull high
		  */
#if 0
		if (pdev->Vendor_cfg[0][0] != 0xFFFFFFFF)
			pdev->Vendor_cfg[0][0] = 0x816510EC;
#endif

		pcieh_set_sys_pwr_state(pdev, SYS_PWR_S0);
	}

	writew(IMRvalue, PCIE_FUN0_IMR);
}
EXPORT_SYMBOL(bsp_Fun0_handler);
