/*
* Realtek SPI driver
*
* Copyright(c) 2015 Realtek Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spi/spi-rtk.h>

static int rtk_spi_probe(struct platform_device *pdev)
{
    struct rtk_spi *rtk_hw;
    struct device_node *np;
    const u32 *prop;
    //unsigned int num_cs=0;
    int size;

    int err = -ENODEV;

    if (WARN_ON(!(pdev->dev.of_node)))
        return -err;

    rtk_hw = devm_kzalloc(&pdev->dev, sizeof(struct rtk_spi), GFP_KERNEL);
    if (!rtk_hw)
        return -ENOMEM;

    /* Get basic io resource and map it */
    prop = of_get_property(pdev->dev.of_node, "reg", &size);
    if (prop)
    {
            rtk_hw->base = ioremap(of_read_number(prop, 1), of_read_number(prop+1, 1));
            printk(KERN_INFO "[%s] get spi controller base addr : 0x%x \n",__func__, rtk_hw->base);
    } else 
    {
            printk(KERN_ERR "[%s] get spi controller base addr error !!\n",__func__);
    }

    device_property_read_u32(&pdev->dev, "excha-addr", &rtk_hw->excha_addr);
    device_property_read_u32(&pdev->dev, "spi-max-frequency", &rtk_hw->max_freq);
    //device_property_read_u32(&pdev->dev, "num-cs", &num_cs);
    rtk_hw->num_cs = 1;
    rtk_hw->bus_num = 0;

    np =of_get_cpu_node(0, NULL);
    prop = of_get_property(np, "clock-frequency", &size);
    if (prop)
    {
        rtk_hw->cpu_freq = of_read_number(prop, 1);
        printk(KERN_INFO "[%s] get cpu-frequency : %d \n",__func__,rtk_hw->cpu_freq);
    } else
    {
        printk(KERN_ERR "[%s] get cpu-frequency error !! %d \n",__func__,err);
    }

    np = of_find_node_by_name(NULL, "spi-flash");
    prop = of_get_property(np, "spi-dummy-cycles", &size);
    if (prop) {
        rtk_hw->dummy_cycle = of_read_number(prop, 1);
    }

    err = rtk_spi_add_host_controller(&pdev->dev, rtk_hw);
    if (err)
        goto exit;

    platform_set_drvdata(pdev, rtk_hw);
    printk(KERN_INFO "[%s] spi controller driver is registered.\n",__func__);
    return 0;
exit:
    dev_set_drvdata(&pdev->dev, NULL);
    platform_set_drvdata(pdev, NULL);
    return err;
}

static int rtk_spi_remove(struct platform_device *dev)
{
    struct rtk_spi *hw = platform_get_drvdata(dev);
    struct spi_master *master = hw->master;

    platform_set_drvdata(dev, NULL);
    dev_set_drvdata(&dev->dev, NULL);
    spi_master_put(master);
    rtk_spi_remove_host_controller(hw);
    return 0;
}

static const struct of_device_id rtk_spi_match[] = {
    { .compatible = "realtek,rtk-spi", },
    {},
};
MODULE_DEVICE_TABLE(of, rtk_spi_match);

static struct platform_driver rtk_spi_driver = {
    .probe = rtk_spi_probe,
    .remove = rtk_spi_remove,
    .driver = {
        .name = "rtk-spi",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(rtk_spi_match),
    },
};
module_platform_driver(rtk_spi_driver);

MODULE_DESCRIPTION("Realtek SPI controller driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:dash");

