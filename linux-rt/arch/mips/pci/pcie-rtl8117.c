/*
 *  Ralink RT3662/RT3883 SoC PCI support
 *
 *  Copyright (C) 2011-2013 Gabor Juhos <juhosg@openwrt.org>
 *
 *  Parts of this file are based on Ralink's 2.6.21 BSP
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>
#include <linux/platform_device.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "pcie-rtl8117-core.h"
#include "pcie-rtl8117-reg.h"

#include <rtl8117_platform.h>

#define RTL_OOB_PROC
#define dir_name "rtl8117-pcie"

struct rtl8117_pci_controller {
	struct platform_device *pdev;
	struct pci_controller pci_controller;
	struct resource io_res;
	struct resource mem_res;
	rtk_pci_dev rtk_pci_dev;
};

static struct rtl8117_pci_controller *rpc;
static bool inband_mode = 0;

void pcibios_scanbus(struct pci_controller *hose);

static int rtl8117_pci_config_read(struct pci_bus *bus, unsigned int devfn,
				   int where, int size, u32 *val);

static int rtl8117_pci_config_write(struct pci_bus *bus, unsigned int devfn,
				    int where, int size, u32 val);

static struct pci_ops rtl8117_pci_ops = {
	.read	= rtl8117_pci_config_read,
	.write	= rtl8117_pci_config_write,
};

static int rtl8117_get_busno(void)
{
	return 0;
}

static int proc_get_rc_table(struct seq_file *m, void *v)
{
	int i, n, max = 0x40;
	u32 dword_rd;

#if 0
	u32 *memory = rtk_pci_dev->Vendor_cfg;
#endif

	seq_puts(m, "\nDump pcie rc cfg tables\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 4 && n < max; i++, n+=4) {
			pcieh_rc32_read(n, &dword_rd);
			//dword_rd = memory[n/4];
			seq_printf(m, "0x%04x ", dword_rd);
		}
	}

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_vt_table(struct seq_file *m, void *v)
{
	int i, n, max = 0x40;
	u32 dword_rd;

#if 0
	u32 *memory = rtk_pci_dev->Vendor_cfg;
#endif

	seq_puts(m, "\nDump pcie vt cfg tables\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 4 && n < max; i++, n+=4) {
			Read_Vt_PCIDword(n, 0x5, &dword_rd);
			//dword_rd = memory[n/4];
			seq_printf(m, "0x%04x ", dword_rd);
		}
	}

	seq_putc(m, '\n');
	return 0;
}

#ifdef RTL_OOB_PROC
static int proc_get_vendor_cfg_table(struct seq_file *m, void *v)
{
	rtk_pci_dev *rtk_pci_dev = &rpc->rtk_pci_dev;
	int i, n, max = 0x300;
	u32 dword_rd;

	u32 *memory =(u32*) rtk_pci_dev->Vendor_cfg;

	seq_puts(m, "\nDump pcie vendor cfg tables\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 4 && n < max; i++, n+=4) {
			dword_rd = memory[n/4];
			seq_printf(m, "0x%04x ", dword_rd);
		}
	}

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_default_cfg_table(struct seq_file *m, void *v)
{
	rtk_pci_dev *rtk_pci_dev = &rpc->rtk_pci_dev;
	int i, n, max = 0x300;
	u32 dword_rd;

	u32 *memory = (u32*)rtk_pci_dev->Default_cfg;

	seq_puts(m, "\nDump pcie default cfg tables\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 4 && n < max; i++, n+=4) {
			dword_rd = memory[n/4];
			seq_printf(m, "0x%04x ", dword_rd);
		}
	}

	seq_putc(m, '\n');
	return 0;
}

int rtl8117_pcie_to_bypass(void)
{
	if (inband_mode == 1)
		return 0;

	pci_stop_root_bus(rpc->pci_controller.bus);
	pci_remove_root_bus(rpc->pci_controller.bus);
	pcie_dash_switch_to_bypass(&rpc->rtk_pci_dev);

	inband_mode = 1;
	return 0;
}

int rtl8117_pcie_to_host(void)
{
	if (inband_mode == 0)
		return 0;

	pcie_dash_switch_to_4281_in_sleep(&rpc->rtk_pci_dev);

	pcibios_scanbus(&rpc->pci_controller);
	inband_mode = 0;
	return 0;
}

static int proc_get_bar1_table(struct seq_file *m, void *v)
{
	int i, n, max = 0x300;
	u32 dword_rd;

	seq_puts(m, "\nDump pcie mmio tables\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 4 && n < max; i++, n+=4) {
			pcieh_mem32_read(0x0, 0xc0004000 + n, &dword_rd);
			seq_printf(m, "0x%04x ", dword_rd);
		}
	}

	seq_putc(m, '\n');
	return 0;
}

static int rtl8117_proc_open(struct inode *inode, struct file *file)
{
	struct net_device *dev = proc_get_parent_data(inode);
	int (*show)(struct seq_file *, void *) = PDE_DATA(inode);

	return single_open(file, show, dev);
}

static const struct file_operations rtl8117_proc_fops = {
	.open		= rtl8117_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * Table of proc files we need to create.
 */
struct rtl8117_proc_file {
	char name[12];
	int (*show)(struct seq_file *, void *);
};

static const struct rtl8117_proc_file rtl8117_proc_files[] = {
	{ "ven_cfg", &proc_get_vendor_cfg_table },
	{ "default_cfg", &proc_get_default_cfg_table },
	{ "vt_cfg", &proc_get_vt_table },
	{ "rc_cfg", &proc_get_rc_table },
	{ "bar1_table", &proc_get_bar1_table },
	{ "" }
};

static int rtl8117_pcie_modselect_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, PDE_DATA(inode));
}

static ssize_t rtl8117_pcie_modselect_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char str[128];
	u8 val = inband_mode;

	len = sprintf(str, "%x\n", val);

	copy_to_user(buf, str, len);
	if (*ppos == 0)
		*ppos += len;
	else
		len = 0;

	return len;
}

static ssize_t rtl8117_pcie_modselect_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	u8 num;
	u32 val;

	if (buf && !copy_from_user(tmp, buf, sizeof(tmp))) {
		num = sscanf(tmp, "%x", &val);
		if ((val == 1) || (val == 0)) {
		} else
			printk(KERN_INFO "[PCIE] write procfs not support value  = %x\n", inband_mode);
	}

#if 0
	if (val == 1) {
		printk(KERN_INFO "[PCIE] switch pcie to bypass mode\n");
		//rtl8117_pcie_to_bypass();
	}
#endif

	if (val == 0) {
		printk(KERN_INFO "[PCIE] switch pcie to host mode\n");
		//msleep(2000);
		//rtl8117_pcie_to_host();
	}

#if 0
	inband_mode = val;
#endif
	return count;
}

static const struct file_operations mode_select_fops =
{
	.owner = THIS_MODULE,
	.open = rtl8117_pcie_modselect_open,
	.read = rtl8117_pcie_modselect_read,
	.write = rtl8117_pcie_modselect_write,
};

#endif

static inline struct rtl8117_pci_controller *pci_bus_to_rtl8117_controller(struct pci_bus *bus)
{
	struct pci_controller *hose;

	hose = (struct pci_controller *) bus->sysdata;
	return container_of(hose, struct rtl8117_pci_controller, pci_controller);
}

static int rtl8117_pci_config_read(struct pci_bus *bus, unsigned int devfn,
				   int where, int size, u32 *val)
{
	u32 tmp = 0, ret = 0;

	struct rtl8117_pci_controller *rtl8117_pc = pci_bus_to_rtl8117_controller(bus);
	u8 * cfg_memory = (u8*)rtl8117_pc->rtk_pci_dev.Vendor_cfg;
	bool backup_cfg = false;

	if (bus->number != 0 || PCI_SLOT(devfn) != 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* cfg0 is always set to 0x8165*/
	if ((where > 3) && (where <= 0x200)) {
		cfg_memory += where;
		backup_cfg = true;
	}
	else if (where >= 0x700) { /* 0x0x700~0x7FC */
		cfg_memory += (where - 0x500);
		backup_cfg = true;
	}

	switch (size) {
	case 1:
		ret = pcieh_cfg8_read(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, &tmp);
		break;
	case 2:
		ret = pcieh_cfg16_read(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, &tmp);
		break;
	case 4:
		ret = pcieh_cfg32_read(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, &tmp);
		break;
	}

	if (ret != PH_SUCCESS)
		return PCIBIOS_SET_FAILED;

	*val = tmp;

	if (backup_cfg)
		memcpy(cfg_memory, &tmp, size);

	printk(KERN_DEBUG "[PCIE] read config slot = 0x%x, , func = 0x%x, reg = 0x%x, value = 0x%x\n", PCI_SLOT(devfn),PCI_FUNC(devfn), where, *val);

	return PCIBIOS_SUCCESSFUL;
}

static int rtl8117_pci_config_write(struct pci_bus *bus, unsigned int devfn,
				    int where, int size, u32 val)
{
	u32 ret = 0;

	struct rtl8117_pci_controller *rtl8117_pc = pci_bus_to_rtl8117_controller(bus);
	u8* cfg_memory = (void*)&rtl8117_pc->rtk_pci_dev.Vendor_cfg;

	bool backup_cfg = false;

	if (bus->number != 0 || PCI_SLOT(devfn) != 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	printk(KERN_DEBUG "[PCIE] write config slot = 0x%x, , func = 0x%x, reg = 0x%x, value = 0x%x\n", PCI_SLOT(devfn),PCI_FUNC(devfn), where, val);

	if (where <= 0x200) {
		cfg_memory += where;
		backup_cfg = true;
	}
	else if (where >= 0x700) { /* 0x0x700~0x7FC */
		cfg_memory += (where - 0x500);
		backup_cfg = true;
	}

	switch (size) {
	case 1:
		ret = pcieh_cfg8_write(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, (u8)val);
		break;
	case 2:
		ret = pcieh_cfg16_write(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, (u16)val);
		break;
	case 4:
		ret = pcieh_cfg32_write(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, (u32)val);
		break;
	}

	if (ret != PH_SUCCESS)
		return PCIBIOS_SET_FAILED;

	if (backup_cfg)
		memcpy(cfg_memory, &val, size);

	return PCIBIOS_SUCCESSFUL;
}

irqreturn_t rtl8117_pci_intr(int irq, void *dev)
{
	PH_INT PH_ISR;
	volatile INT32U PCIE_PHost_ISRvalue;
	rtk_pci_dev *this_pdev = (rtk_pci_dev *)dev;

	int handled = 0;

	this_pdev->IMRvalue = readl(WIFI_IMR);	//back up wifi dash imr
	PCIE_PHost_ISRvalue = readl(WIFI_ISR);
	PCIE_PHost_ISRvalue &= this_pdev->IMRvalue;
	writel(0x00000000, WIFI_IMR);	//clear wifi dash imr

	//get ISR result and reset the ISR
	memcpy(&PH_ISR, (PH_INT *)(&PCIE_PHost_ISRvalue), sizeof(PH_INT));

#if 0
	printk(KERN_DEBUG "[PCIE] rtl8117_pci_intr PH_ISR = 0x%x\n", PH_ISR);
#endif

	if (PH_ISR.cdm_rw && (readl(CDM_MBOX_CTRL) & BIT_31)) {
		config_rw_handler(this_pdev);
		handled = 1;
	}

#if 0
	//device interrupt
	if (PH_ISR.sii_rc_inta || PH_ISR.sii_rc_intb || PH_ISR.sii_rc_intc || PH_ISR.sii_rc_intd)
		pcieh_interrupt_handler_callback(pdev);
#endif

	//rc link request reset
	if (PH_ISR.rc_link_down) {
		rc_link_down_handler(this_pdev);
		handled = 1;
#if 0
		printk(KERN_DEBUG "[PCIE] PH_ISR.rc_link_down!\n");
#endif
	}

	//EP link request reset
	if (PH_ISR.ep_link_down) {
		writel(readl(WIFI_ISR)| BIT_16, WIFI_ISR); // clean ISR
		this_pdev->nEP_link_down++;
		handled = 1;
#if 0
		printk(KERN_DEBUG "[PCIE] PH_ISR.ep_link_down!\n");
#endif
	}

	writel(this_pdev->IMRvalue, WIFI_IMR);

	return IRQ_RETVAL(handled);
}

irqreturn_t rtl8117_pci_rst_intr(int irq, void *dev)
{
	int handled = 0;
	rtk_pci_dev *this_pdev = (rtk_pci_dev *)dev;

	u8 pcie_func0_isrvalue = readb(PCIE_FUN0_ISR);
	u8 pcie_func0_imrvalue = readb(PCIE_FUN0_IMR);

#if 0
	printk(KERN_DEBUG "[PCIE] rtl8117_pci_rst_intr");
#endif

	//Note:pice and echi isr value may be enable in the same time
	if (pcie_func0_isrvalue & pcie_func0_imrvalue) {
		bsp_Fun0_handler();
		handled = 1;
	}

	return IRQ_RETVAL(handled);
}

int rtl8117_bypass_handler(void* context)
{
	rtk_pci_dev *rtk_pci_dev = &rpc->rtk_pci_dev;
	u8 pci_cmd = oobm_read_pci_cmd();

	if (pci_cmd == 0x13) {
		rtk_pci_dev->goto4281mode = 0;
		schedule_delayed_work(&rtk_pci_dev->switch_mode_schedule, 0);
	}

	return 0;
}

void rtl8117_switch_handler(struct work_struct *work)
{
	rtk_pci_dev *pdev = container_of(work, rtk_pci_dev, switch_mode_schedule.work);

	if (pdev->goto4281mode) {
		printk("[PCIE] switch to pcie 4281 sleep mode\r\n");

		pcie_dash_switch_to_4281_in_sleep(pdev);
		pcibios_scanbus(&rpc->pci_controller);
	}
	else {
		printk("[PCIE] switch to pcie bypass mode\r\n");

		pci_stop_root_bus(rpc->pci_controller.bus);
		pci_remove_root_bus(rpc->pci_controller.bus);
		pcie_dash_switch_to_bypass(&rpc->rtk_pci_dev);
	}
}

static int rtl8117_pci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq;
	int retval;

#ifdef RTL_OOB_PROC
	struct proc_dir_entry * proc_dir = NULL;
#endif

	rpc = devm_kzalloc(dev, sizeof(*rpc), GFP_KERNEL);
	if (!rpc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	rpc->pdev = pdev;
	rpc->rtk_pci_dev.pci_base = of_iomap(pdev->dev.of_node, 0);

	if (IS_ERR(rpc->rtk_pci_dev.pci_base))
		return PTR_ERR(rpc->rtk_pci_dev.pci_base);

	rpc->rtk_pci_dev.oob_mac_base = of_iomap(pdev->dev.of_node, 1);

	if (IS_ERR(rpc->rtk_pci_dev.oob_mac_base))
		return PTR_ERR(rpc->rtk_pci_dev.oob_mac_base);

	rpc->rtk_pci_dev.fun0_base_addr = of_iomap(pdev->dev.of_node, 2);

	if (IS_ERR(rpc->rtk_pci_dev.fun0_base_addr))
		return PTR_ERR(rpc->rtk_pci_dev.fun0_base_addr);

	irq = 11;
	if (irq < 0) {
		dev_err(&pdev->dev, "missing IRQ resource\n");
		return irq;
	}

	retval = devm_request_irq(&pdev->dev, irq,
			rtl8117_pci_intr, IRQF_SHARED,
				  dev_name(&pdev->dev), &rpc->rtk_pci_dev);
	if (retval)
		return retval;

	irq = 8;
	if (irq < 0) {
		dev_err(&pdev->dev, "missing IRQ resource\n");
		return irq;
	}

	retval = devm_request_irq(&pdev->dev, irq,
				  rtl8117_pci_rst_intr, IRQF_SHARED,
				  dev_name(&pdev->dev), &rpc->rtk_pci_dev);
	if (retval)
		return retval;

	init_pci_setup(&rpc->rtk_pci_dev);
	pcieh_init(&rpc->rtk_pci_dev);

	rpc->pci_controller.pci_ops = &rtl8117_pci_ops;
	rpc->pci_controller.io_resource = &rpc->io_res;
	rpc->pci_controller.mem_resource = &rpc->mem_res;
	rpc->pci_controller.get_busno = rtl8117_get_busno;

	/* Load PCI I/O and memory resources from DT */
	pci_load_of_ranges(&rpc->pci_controller,
			   pdev->dev.of_node);

	ioport_resource.start = rpc->io_res.start;
	ioport_resource.end = rpc->io_res.end;

	register_pci_controller(&rpc->pci_controller);

#ifdef RTL_OOB_PROC
	do {
		const struct rtl8117_proc_file *f;

		proc_dir = proc_mkdir(dir_name, NULL);
		if (!proc_dir)
		{
			dev_err(&pdev->dev,"Create directory \"%s\" failed.\n", dir_name);
			return -1;
		}

		for (f = rtl8117_proc_files; f->name[0]; f++) {
			if (!proc_create_data(f->name, S_IFREG | S_IRUGO, proc_dir,
						&rtl8117_proc_fops, f->show)) {
				dev_err(&pdev->dev, "Create file \"%s\"\" failed.\n", "attach_dev");
			}
		}
#if 0
		if (!proc_create_data("inband_mode", 0666, proc_dir, &mode_select_fops, rpc)) {
			dev_err(&pdev->dev, "Create file \"%s\"\" failed.\n", "inband_mode");
		}
#endif
	} while (0);
#endif

	INIT_DELAYED_WORK(&rpc->rtk_pci_dev.switch_mode_schedule, rtl8117_switch_handler);
	register_swisr(0xff, rtl8117_bypass_handler, rpc);

	return 0;
}

static const struct of_device_id rtl8117_pci_ids[] = {
	{ .compatible = "realtek,rtl8117-pcie" },
	{},
};
MODULE_DEVICE_TABLE(of, rtl8117_pci_ids);

int rtl8117_pci_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pci_stop_root_bus(rpc->pci_controller.bus);
	pcieh_disable_pwr_seq();
	pci_remove_root_bus(rpc->pci_controller.bus);

	devm_kfree(dev, rpc);
	rpc = NULL;
	return 0;
}

static struct platform_driver rtl8117_pci_driver = {
	.probe = rtl8117_pci_probe,
	.remove = rtl8117_pci_remove,
	.driver = {
		.name = "rtl8117-pcie",
		.of_match_table = of_match_ptr(rtl8117_pci_ids),
	},
};

module_platform_driver(rtl8117_pci_driver);

MODULE_AUTHOR("Ted Chen <tedchen@realtek.com>");
MODULE_DESCRIPTION("Realtek RTL8117 pcie host controller driver");
MODULE_LICENSE("GPL");
