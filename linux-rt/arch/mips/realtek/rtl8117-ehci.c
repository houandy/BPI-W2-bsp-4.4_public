#include <linux/types.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/usb.h>

#include <rtl8117_platform.h>

#include "rtl8117-ehci.h"

#define MODULENAME "rtl8117-ehci"

static u32 usb_power_gpio = 0;

struct rtl8117_ehci_controller {
	struct platform_device *pdev;
	int irq;
	void __iomem  *ehci_base_addr;
};

struct proc_dir_entry * proc_dir = NULL;
struct proc_dir_entry * proc_entry = NULL;

static char * dir_name = "rtl8117-ehci";
static char * entry_name = "ehci_enabled";

static bool ehci_enabled = 0;
static bool attach_device = 0;

static struct delayed_work usb_power_schedule;

static int rtl8117_mode_select_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, PDE_DATA(inode));
}

static ssize_t rtl8117_mode_select_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char str[128];
	u8 val = ehci_enabled;

	len = sprintf(str, "%x\n", val);

	copy_to_user(buf, str, len);
	if (*ppos == 0)
		*ppos += len;
	else
		len = 0;

	return len;
}

static ssize_t rtl8117_mode_select_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	u8 num;
	u32 val;

	struct rtl8117_ehci_controller *p_rtl8117_ehci_ctl =  ((struct seq_file *)file->private_data)->private;
	struct platform_device *pdev = p_rtl8117_ehci_ctl->pdev;

	if (buf && !copy_from_user(tmp, buf, sizeof(tmp))) {
		num = sscanf(tmp, "%x", &val);
		if ((val == 1) || (val == 0)) {
			ehci_enabled = val;
		} else
			dev_err(&pdev->dev, "write procfs not support value  = %x \n", ehci_enabled);
	}

	rtl8117_ehci_set_otg_power(0);
	msleep(2000);
	rtl8117_ehci_set_otg_power(1);

	printk(KERN_INFO "[EHCI] set echi to %s\n", (ehci_enabled) ? ("inband") : ("oob") );
	return count;
}

static int rtl8117_change_device_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, PDE_DATA(inode));
}

static ssize_t rtl8117_change_device_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char str[128];
	u8 val = attach_device;

	len = sprintf(str, "%x\n", val);

	copy_to_user(buf, str, len);
	if (*ppos == 0)
		*ppos += len;
	else
		len = 0;

	return len;
}

static ssize_t rtl8117_change_device_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	u8 num;
	u32 val;
	int ret;

	struct rtl8117_ehci_controller *p_rtl8117_ehci_ctl =  ((struct seq_file *)file->private_data)->private;
	struct platform_device *pdev = p_rtl8117_ehci_ctl->pdev;

	if (buf && !copy_from_user(tmp, buf, sizeof(tmp))) {
		num = sscanf(tmp, "%x", &val);
		if ((val == 1) || (val == 0)) {
		} else
			dev_err(&pdev->dev, "write procfs not support value  = %x \n", attach_device);
	}

	if (val == 1 && ehci_enabled) {
		ret = rtl8117_ehci_open_file("/dev/sda");
		if (ret == 0) {
			printk(KERN_INFO "[EHCI] attach sda to ehci\n");
			rtl8117_ehci_intep_enabled(1);
		}
	}

	if (val == 0) {
		printk(KERN_INFO "[EHCI] detach sda from ehci\n");
		rtl8117_ehci_close_file();
		rtl8117_ehci_intep_disabled(1);
	}

	attach_device = val;
	return count;
}

void rtl8117_ehci_poweron_request(void)
{
	schedule_delayed_work(&usb_power_schedule, msecs_to_jiffies(2000));
}

void rtl8117_ehci_set_otg_power(bool on)
{
	printk(KERN_INFO "[EHCI] set usb power to %s\n", (on) ? ("high") : ("low") );

	if (gpio_is_valid(usb_power_gpio)) {
		if (on)
			gpio_direction_output(usb_power_gpio, 1);
		else
			gpio_direction_output(usb_power_gpio, 0);
	}
}

static const struct file_operations mode_select_fops =
{
	.owner = THIS_MODULE,
	.open = rtl8117_mode_select_open,
	.read = rtl8117_mode_select_read,
	.write = rtl8117_mode_select_write,
};

static const struct file_operations change_device_fops =
{
	.owner = THIS_MODULE,
	.open = rtl8117_change_device_open,
	.read = rtl8117_change_device_read,
	.write = rtl8117_change_device_write,
};

irqreturn_t rtl8117_ehci_intr(int irq, void *dev)
{
	int handled = 0;

	handled = rtl8117_ehci_intr_handler();
	return IRQ_RETVAL(handled);
}

static void rtl8117_power_work_func_t(struct work_struct *work)
{
	rtl8117_ehci_set_otg_power(1);
	return;
}

static int rtl8117_ehci_attach_usb(void* context)
{
	struct rtl8117_ehci_controller *p_rtl8117_ehci_ctl = (struct rtl8117_ehci_controller *)context;
	kobject_uevent(&p_rtl8117_ehci_ctl->pdev->dev.kobj, KOBJ_ATTACH_USB);
	return 0;
}

static int rtl8117_ehci_deattach_usb(void* context)
{
	struct rtl8117_ehci_controller *p_rtl8117_ehci_ctl = (struct rtl8117_ehci_controller *)context;
	kobject_uevent(&p_rtl8117_ehci_ctl->pdev->dev.kobj, KOBJ_DEATTACH_USB);
	return 0;
}

static int rtl8117_ehci_probe(struct platform_device *pdev)
{
	int ret;
	int retval;
	struct rtl8117_ehci_controller *p_rtl8117_ehci_ctl;

	p_rtl8117_ehci_ctl = kzalloc(sizeof(struct rtl8117_ehci_controller), GFP_KERNEL);
	if (!p_rtl8117_ehci_ctl)
	{
		dev_err(&pdev->dev, "rtl8117 ehci: failed to allocate device structure.\n");
		return -ENOMEM;
	}
	memset(p_rtl8117_ehci_ctl, 0, sizeof(struct rtl8117_ehci_controller));

	p_rtl8117_ehci_ctl->pdev = pdev;

	p_rtl8117_ehci_ctl->ehci_base_addr = of_iomap(pdev->dev.of_node, 0);
	if (!p_rtl8117_ehci_ctl->ehci_base_addr) {
		dev_err(&pdev->dev, "can't request 'ctrl' address\n");
		return -EINVAL;
	}

	p_rtl8117_ehci_ctl->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (p_rtl8117_ehci_ctl->irq < 0) {
		dev_err(&pdev->dev, "missing IRQ resource\n");
		return p_rtl8117_ehci_ctl->irq;
	}

	retval = devm_request_irq(&pdev->dev, p_rtl8117_ehci_ctl->irq, rtl8117_ehci_intr, IRQF_SHARED,
				  dev_name(&pdev->dev), p_rtl8117_ehci_ctl);
	if (retval) {
		dev_err(&pdev->dev, "can't request irq\n");
		return retval;
	}

	usb_power_gpio = of_get_gpio_flags(pdev->dev.of_node, 0, NULL);
	if (gpio_is_valid(usb_power_gpio)) {
		ret = gpio_request(usb_power_gpio, "usb_power_gpio");
		if (ret < 0)
			printk(KERN_ERR "[EHCI] %s: can't request gpio %d\n", __func__, usb_power_gpio);
	} else
		printk(KERN_ERR "[EHCI] %s: gpio %d is not valid\n", __func__, usb_power_gpio);

	proc_dir = proc_mkdir(dir_name, NULL);
	if (!proc_dir)
	{
		dev_err(&pdev->dev,"Create directory \"%s\" failed.\n", dir_name);
		return -1;
	}

	proc_entry = proc_create_data(entry_name, 0666, proc_dir, &mode_select_fops, p_rtl8117_ehci_ctl);
	if (!proc_entry)
	{
		dev_err(&pdev->dev, "Create file \"%s\"\" failed.\n", entry_name);
		return -1;
	}

	proc_entry = proc_create_data("attach_dev", 0666, proc_dir, &change_device_fops, p_rtl8117_ehci_ctl);
	if (!proc_entry)
	{
		dev_err(&pdev->dev, "Create file \"%s\"\" failed.\n", "attach_dev");
		return -1;
	}

	INIT_DELAYED_WORK(&usb_power_schedule, rtl8117_power_work_func_t);

	if (rlt8117_ehci_core_init(pdev) < 0)
		return -1;

	platform_set_drvdata(pdev, p_rtl8117_ehci_ctl);

	rtl8117_ehci_device_init();

	rtl8117_ehci_set_otg_power(1);

	register_swisr(0x70, rtl8117_ehci_attach_usb, p_rtl8117_ehci_ctl);
	register_swisr(0x71, rtl8117_ehci_deattach_usb, p_rtl8117_ehci_ctl);
	return 0;
}

static const struct of_device_id rtl8117_ehci_ids[] = {
	{ .compatible = "realtek,rtl8117-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, rtl8117_ehci_ids);

int rtl8117_ehci_remove(struct platform_device *pdev)
{
	return 0;
}

void rtl8117_set_ehci_enable(bool on)
{
	ehci_enabled = on;
	return;
}

static struct platform_driver rtl8117_ehci_driver = {
	.probe = rtl8117_ehci_probe,
	.remove = rtl8117_ehci_remove,
	.driver = {
		.name = MODULENAME,
		.of_match_table = of_match_ptr(rtl8117_ehci_ids),
	},
};

module_platform_driver(rtl8117_ehci_driver);

MODULE_AUTHOR("Ted Chen <tedchen@realtek.com>");
MODULE_DESCRIPTION("Realtek RTL8117 virtual ehci driver");
MODULE_LICENSE("GPL");
