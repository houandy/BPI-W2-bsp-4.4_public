/*
 * EHCI HCD (Host Controller Driver) for USB.
 *
 * Bus Glue for Realtek Dmp chips
 *
 * Based on "ehci-orion.c" by Tzachi Perelstein <tzachi@marvell.com>
 *
 * Modified for Realtek Dmp chips
 *
 * This file is licensed under the GPL.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mbus.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/usb/phy.h>
#include <linux/of_gpio.h>
#include <linux/suspend.h>

#include "ehci.h"

#define DRIVER_DESC "EHCI realtek driver"

static const char hcd_name[] = "ehci-rtk";

static struct hc_driver __read_mostly ehci_rtk_hc_driver;

static struct ehci_rtk {
    struct device *dev;
    struct ehci_hcd *ehci;
    int irq;
};

extern int rtk_usb_init_power_on(struct device *usb_dev);

static int ehci_rtk_drv_probe(struct platform_device *pdev)
{
    struct resource res;
    struct usb_hcd *hcd;
    struct ehci_hcd *ehci;
    void __iomem *regs, *reset_reg;
    int irq, err = 0;
    struct usb_phy *phy;
    unsigned long probe_time = jiffies;

    if (usb_disabled())
        return -ENODEV;

    dev_info(&pdev->dev, "Probe Realtek-SoC USB EHCI Host Controller\n");

#ifdef CONFIG_ARCH_RTD119X

    reset_reg = of_iomap(pdev->dev.of_node, 1);
    if (reset_reg == NULL) {
        dev_err(&pdev->dev, "error mapping memory for reset_reg\n");
        err = -EFAULT;
        goto err1;
    }

    /* reset usb mac */
    //writel(readl(IOMEM(0xfe000000)) & ~(BIT(9)), IOMEM(0xfe000000));
    writel(readl(reset_reg) & ~(BIT(9)), reset_reg);
    mdelay(1);
    /* release usb mac */
    //writel(readl(IOMEM(0xfe000000)) | BIT(9), IOMEM(0xfe000000));
    writel(readl(reset_reg) | BIT(9), reset_reg);
    mdelay(10);

#endif

    //phy = devm_usb_get_phy(&pdev->dev, USB_PHY_TYPE_USB2);
    phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
    if (IS_ERR(phy)) {
        dev_err(&pdev->dev, "No usb phy found\n");
        return -ENODEV;
    } else {
        usb_phy_init(phy);
    }

    irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
    if (irq <= 0) {
        dev_err(&pdev->dev,
                "Found HC with no IRQ. Check %s setup!\n",
                dev_name(&pdev->dev));
        err = -ENODEV;
        goto err1;
    }

    if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
        dev_err(&pdev->dev,
                "Found HC with no register addr. Check %s setup!\n",
                dev_name(&pdev->dev));
        err = -ENODEV;
        goto err1;
    }

    /*
     * Right now device-tree probed devices don't get dma_mask
     * set. Since shared usb code relies on it, set it here for
     * now. Once we have dma capability bindings this can go away.
     */
    if (!pdev->dev.dma_mask)
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    if (!pdev->dev.coherent_dma_mask)
        pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

    regs = of_iomap(pdev->dev.of_node, 0);
    if (regs == NULL) {
        dev_err(&pdev->dev, "error mapping memory\n");
        err = -EFAULT;
        goto err3;
    }

    hcd = usb_create_hcd(&ehci_rtk_hc_driver,
                         &pdev->dev, dev_name(&pdev->dev));
    if (!hcd) {
        err = -ENOMEM;
        goto err3;
    }

    hcd->rsrc_start = res.start;
    hcd->rsrc_len = resource_size(&res);
    hcd->regs = regs;

    ehci = hcd_to_ehci(hcd);
    ehci->caps = hcd->regs;

    err = usb_add_hcd(hcd, irq, IRQF_SHARED);
    if (err) {
        dev_err(&pdev->dev, "error add hcd\n");
        goto err4;
    }

    rtk_usb_init_power_on(&pdev->dev);

    dev_info(&pdev->dev, "%s OK (take %d ms)\n", __func__, jiffies_to_msecs(jiffies - probe_time));
    return 0;

err4:
    usb_put_hcd(hcd);
err3:
    irq_dispose_mapping(irq);
err1:
    dev_err(&pdev->dev, "%s: Probe %s fail, %d\n", __func__,
            dev_name(&pdev->dev), err);

    return err;

}

static int ehci_rtk_drv_remove(struct platform_device *pdev)
{
    struct usb_hcd *hcd = platform_get_drvdata(pdev);

    dev_set_drvdata(&pdev->dev, NULL);

    usb_remove_hcd(hcd);
    usb_put_hcd(hcd);

    return 0;
}

#ifdef CONFIG_PM
static int rtk_ehci_suspend(struct device *dev);
int RTK_rtk_ehci_suspend(struct device *dev)
{
    return rtk_ehci_suspend(dev);
}

static int rtk_ehci_suspend(struct device *dev)
{
    struct usb_hcd *hcd = dev_get_drvdata(dev);
    struct platform_device *pdev = to_platform_device(dev);
    struct usb_phy *phy;
    bool do_wakeup = device_may_wakeup(dev);
    int rc = 0;

    dev_info(dev, "[USB] Enter %s", __func__);

    if (RTK_PM_STATE == PM_SUSPEND_STANDBY) {
        //For idle mode
        dev_info(dev, "[USB] %s Idle mode\n", __func__);
        goto out;
    }
    dev_info(dev, "[USB] %s Suspend mode\n", __func__);

    rc = ehci_suspend(hcd, do_wakeup);

    phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
    if (IS_ERR(phy)) {
        dev_err(&pdev->dev, "No usb phy found\n");
        return -ENODEV;
    } else {
        usb_phy_shutdown(phy);
    }

out:
    dev_info(dev, "[USB] Exit %s", __func__);
    return rc;
}

static int rtk_ehci_resume(struct device *dev);
int RTK_rtk_ehci_resume(struct device *dev)
{
    return rtk_ehci_resume(dev);
}

static int rtk_ehci_resume(struct device *dev)
{
    struct usb_hcd *hcd = dev_get_drvdata(dev);
    struct platform_device *pdev = to_platform_device(dev);
    struct usb_phy *phy;

    dev_info(dev, "[USB] Enter %s", __func__);
    if (RTK_PM_STATE == PM_SUSPEND_STANDBY) {
        //For idle mode
        dev_info(dev, "[USB] %s Idle mode\n", __func__);
        goto out;
    }
    dev_info(dev, "[USB] %s Suspend mode\n", __func__);

    phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
    if (IS_ERR(phy)) {
        dev_err(&pdev->dev, "No usb phy found\n");
        return -ENODEV;
    } else {
        usb_phy_init(phy);
    }

    ehci_resume(hcd, false);

out:
    dev_info(dev, "[USB] Enter %s", __func__);
    return 0;
}

#else
#define rtk_ehci_suspend	NULL
#define rtk_ehci_resume		NULL
#endif

static const struct dev_pm_ops rtk_ehci_pm_ops = {
    .suspend	= rtk_ehci_suspend,
    .resume		= rtk_ehci_resume,
};

static const struct of_device_id ehci_rtk_dt_ids[] = {
    { .compatible = "Realtek,rtk119x-ehci", },
    { .compatible = "Realtek,rtd129x-ehci", },
    {},
};
MODULE_DEVICE_TABLE(of, ehci_rtk_dt_ids);

static struct platform_driver ehci_rtk_driver = {
    .probe		= ehci_rtk_drv_probe,
    .remove		= ehci_rtk_drv_remove,
    .shutdown	= usb_hcd_platform_shutdown,
    .driver = {
        .name	= "rtk-ehci",
        .owner  = THIS_MODULE,
        .pm		= &rtk_ehci_pm_ops,
        .of_match_table = of_match_ptr(ehci_rtk_dt_ids),
    },
};

static int __init ehci_rtk_init(void)
{
    if (usb_disabled())
        return -ENODEV;

    pr_info("%s: " DRIVER_DESC " init\n", hcd_name);

    ehci_init_driver(&ehci_rtk_hc_driver, NULL);
    return platform_driver_register(&ehci_rtk_driver);
}
module_init(ehci_rtk_init);
//late_initcall_sync(ehci_rtk_init);

static void __exit ehci_rtk_cleanup(void)
{
    platform_driver_unregister(&ehci_rtk_driver);
}
module_exit(ehci_rtk_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_ALIAS("platform:rtk-ehci");
MODULE_LICENSE("GPL");
