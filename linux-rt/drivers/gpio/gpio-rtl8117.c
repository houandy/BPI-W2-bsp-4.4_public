#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <rtl8117_platform.h>


#define TESTIO_ACT              2
#define DBG_ACT                 10
#define LADPS_EN                0
#define LINKOK_GPIO_EN          9
#define GPI_OE_REG              13

#define GPIO_EN                 0
#define GPIO_OUT_EN             1
#define GPIO_INPUT_VAL          2
#define GPIO_OUTPUT_VAL         3

//#define RTK_GPIO_DEBUG
#ifdef RTK_GPIO_DEBUG
#define RTK_GPIO_DBG(fmt, ...) pr_info("[GPIO DBG] " fmt "\n", ## __VA_ARGS__)
#else
#define RTK_GPIO_DBG(fmt, ...)
#endif

#define RTK_GPIO_INF(fmt, ...) pr_info("[GPIO] " fmt "\n", ## __VA_ARGS__)
#define RTK_GPIO_ERR(fmt, ...) pr_err("[GPIO Error] " fmt "\n", ## __VA_ARGS__)

#define GP_HIGH     1
#define GP_LOW      0
#define GP_DIROUT   1
#define GP_DIRIN    0


struct rtk_gpio_controller {
    struct gpio_chip    chip;
    spinlock_t      lock;
    void __iomem    *gpio_regs_base;
};

#define chip2controller(chip)	container_of(chip, struct rtk_gpio_controller, chip)

static inline void gpio_prepare(struct gpio_chip *chip)
{
    u32 temp = 0;
    OOB_access_IB_reg(UMAC_PAD_CONT_REG, &temp, 0xf, OOB_READ_OP);
    temp &= ~((1<<DBG_ACT)|(1<<TESTIO_ACT));//set dbg_act = 0 & test_io_act = 0
    //Disable JTAG IN and OUT pins
    temp |= ((1 << 7) | (1 << 8));
    OOB_access_IB_reg(UMAC_PAD_CONT_REG, &temp, 0xf, OOB_WRITE_OP);
}

static inline void gpi_chage_mode(struct gpio_chip *chip)
{
    u32 temp = 0;

    OOB_access_IB_reg(UMAC_MISC_1, &temp, 0xf, OOB_READ_OP);
    temp &= ~(1<<LINKOK_GPIO_EN);
    OOB_access_IB_reg(UMAC_MISC_1, &temp, 0xf, OOB_WRITE_OP);

    OOB_access_IB_reg(UMAC_CONFIG6_PMCH, &temp, 0xf, OOB_READ_OP);
    temp &= ~(1<<LADPS_EN);
    OOB_access_IB_reg(UMAC_CONFIG6_PMCH, &temp, 0xf, OOB_WRITE_OP);

    OOB_access_IB_reg(UMAC_PIN_OE_REG, &temp, 0xf, OOB_READ_OP);
    temp &= ~(1<<GPI_OE_REG);
    OOB_access_IB_reg(UMAC_PIN_OE_REG, &temp, 0xf, OOB_WRITE_OP);
}

static inline int __rtk_gpio_direction(struct gpio_chip *chip, unsigned offset, bool out, int value)
{
    u32 CTL_REG, CTL_INT_REG, en_int_bit, sta_int_bit;
    int offset_base;
    struct rtk_gpio_controller *p_rtk_gpio_ctl = chip2controller(chip);

    unsigned long flags;
    u32 temp;
    RTK_GPIO_DBG("[%s] offset(%u) out(%u)", __FUNCTION__,offset,out);

    spin_lock_irqsave(&p_rtk_gpio_ctl->lock, flags);
    gpio_prepare(chip);

    if ((offset>=4) && (offset <= 24))
    {
        CTL_REG = GPIO_CTRL2_SET + (((offset-4)/8)<<2);
        offset_base = 4 + ((offset-4)/8)*8;
    }
    else if ((offset>=1) && (offset <= 3))
    {
        CTL_REG = GPIO_CTRL1_SET;
        offset_base = -1;
    }
    else if (offset==0)  //GPI pin
    {
        gpi_chage_mode(chip);
        CTL_REG = GPIO_CTRL1_SET;
        offset_base = 0;
    }

    temp = readl(p_rtk_gpio_ctl->gpio_regs_base + CTL_REG);
    temp &= (~(0x0000000F<<((offset-offset_base)*4+GPIO_EN)));
    temp |= (1<<((offset-offset_base)*4+GPIO_EN));
    if (out)
    {
        temp |= (1<<((offset-offset_base)*4+GPIO_OUT_EN));
        temp |= ((value!=0)?(1<<((offset-offset_base)*4+GPIO_INPUT_VAL)):0);
    }
    else
        temp &= (~(1<<((offset-offset_base)*4+GPIO_OUT_EN)));
    writel(temp, p_rtk_gpio_ctl->gpio_regs_base + CTL_REG);
    if (!out)
    {
#ifdef POSITIVE_DEGE_INTERRUPT
        if ((offset>=1) && (offset <= 7))
        {
            CTL_INT_REG = OOBMAC_EXTR_INT;
            en_int_bit = 20 + offset;
            sta_int_bit = 4 + offset;
        }
        else if ((offset>=8) && (offset <= 23))
        {
            CTL_INT_REG = GPIO_CTRL5_SET;
            en_int_bit = offset + 8;
            sta_int_bit = offset - 8;
        }
        else if (offset == 0)
        {
            CTL_INT_REG = OOBMAC_EXTR_INT;
            en_int_bit = 19;
            sta_int_bit = 3;
        }
#else
        if ((offset>=1) && (offset <= 24))
        {
            CTL_INT_REG = GPIO_CTRL7_SET + ((offset<15)?0:4);
            en_int_bit = 1 + offset + ((offset<15)?16:0);
            sta_int_bit = (1 + offset)%16;
        }
        else if (offset == 0)
        {
            CTL_INT_REG = GPIO_CTRL7_SET;
            en_int_bit = 16;
            sta_int_bit = 0;
        }
#endif
        temp = readl(p_rtk_gpio_ctl->gpio_regs_base + CTL_INT_REG);
        temp &= (~(1<<en_int_bit));
        temp |= ((1<<en_int_bit));
        writel(temp, p_rtk_gpio_ctl->gpio_regs_base + CTL_INT_REG);
        temp &= (~(1<<sta_int_bit));
        temp |= ((1<<sta_int_bit));
        writel(temp, p_rtk_gpio_ctl->gpio_regs_base + CTL_INT_REG);
    }

    RTK_GPIO_DBG("[%s] offset(%u)  addr(0x%08llx)  temp(0x%08x)", __FUNCTION__, offset,
                 (u64)((p_rtk_gpio_ctl->gpio_regs_base) + CTL_REG), temp);
    spin_unlock_irqrestore(&p_rtk_gpio_ctl->lock, flags);

    return 0;
}

static inline int rtk_gpio_ops(struct gpio_chip *chip, unsigned offset, int value, bool set)
{
    u32 temp=0;
    u32 CTL_REG;
    int offset_base;
    struct rtk_gpio_controller *p_rtk_gpio_ctl = chip2controller(chip);
    RTK_GPIO_DBG("[%s] offset(%u) value(%u) set(%u)", __FUNCTION__,offset, value, set);

    if ((offset>=4) && (offset <= 24))
    {
        CTL_REG = GPIO_CTRL2_SET + (((offset-4)/8)<<2);
        offset_base = 4 + ((offset-4)/8)*8;
    }
    else if ((offset>=1) && (offset <= 3))
    {
        CTL_REG = GPIO_CTRL1_SET;
        offset_base = -1;
    }
    else if (offset==0)
    {
        CTL_REG = GPIO_CTRL1_SET;
        offset_base = 0;
    }

    temp = readl(p_rtk_gpio_ctl->gpio_regs_base + CTL_REG);
    if (set)
    {
        temp &= (~(1<<((offset-offset_base)*4+GPIO_INPUT_VAL)));;//clear ori value
        temp |= (value<<((offset-offset_base)*4+GPIO_INPUT_VAL));//set new value
        writel(temp, p_rtk_gpio_ctl->gpio_regs_base + CTL_REG);
    }
    else
        temp &= (1<<((offset-offset_base)*4+GPIO_OUTPUT_VAL));
    return (temp!=0)?1:0;
}

static int rtk_gpio_get(struct gpio_chip *chip, unsigned offset)
{
    return rtk_gpio_ops(chip, offset, 0, 0);
}

static void rtk_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
    rtk_gpio_ops(chip, offset, value, 1);
}

static int rtk_gpio_direction_in(struct gpio_chip *chip, unsigned offset)
{
    RTK_GPIO_DBG("[%s] offset(%u)", __FUNCTION__,offset);

    return __rtk_gpio_direction(chip, offset, GP_DIRIN, 0);
}

static int rtk_gpio_direction_out(struct gpio_chip *chip, unsigned offset, int value)
{
    RTK_GPIO_DBG("[%s] offset(%u) value(%d)", __FUNCTION__,offset,value);

    return __rtk_gpio_direction(chip, offset, GP_DIROUT, value);
}

static int rtk_gpio_xlate(struct gpio_chip *gc, const struct of_phandle_args *gpiospec, u32 * flags)
{
    u32 pin;
    RTK_GPIO_DBG("[%s] base(%d), args[0]=%d args[1]=%d args[2]=%d args[3]=%d", __FUNCTION__, gc->base,gpiospec->args[0],gpiospec->args[1],gpiospec->args[2],gpiospec->args[3]);

    if (WARN_ON(gc->of_gpio_n_cells < 1))
        return -EINVAL;

    if (WARN_ON(gpiospec->args_count < gc->of_gpio_n_cells))
        return -EINVAL;

    if (gpiospec->args[0] > gc->ngpio)
        return -EINVAL;

    pin = gpiospec->args[0];

    if (gpiospec->args[1] == GP_DIRIN)	//gpio direction input
    {
        if (rtk_gpio_direction_in(gc, pin))
            printk(KERN_ERR "gpio_xlate: failed to set pin direction in\n");

        if(flags)//low active flag
            *flags = gpiospec->args[3];
    }
    else
    {
        if (gpiospec->args[2] == GP_LOW)	//gpio direction output low
        {
            if (rtk_gpio_direction_out(gc, pin, GP_LOW))
                printk(KERN_ERR "gpio_xlate: failed to set pin direction out \n");
        }
        else if (gpiospec->args[2] == GP_HIGH)
        {
            if (rtk_gpio_direction_out(gc, pin, GP_HIGH))	//gpio direction output high
                printk(KERN_ERR "gpio_xlate: failed to set pin direction out \n");
        }
        else
        {
            printk(KERN_ERR "gpio_xlate: failed to set pin direction out \n");
        }

        if(flags)//low active flag
            *flags = gpiospec->args[3];
    }

    return gpiospec->args[0];
}

void set_default_gpio(struct device_node *node)
{
    int num_gpios, n;
    num_gpios = of_gpio_count(node);
    if (num_gpios > 0)
    {
        for (n = 0; n < num_gpios; n++)
        {
            int gpio = of_get_gpio_flags(node, n, NULL);
            RTK_GPIO_DBG("[%s]  %s  line: %d", __FILE__, __FUNCTION__, __LINE__);
            if (gpio >= 0)
            {
                if (!gpio_is_valid(gpio))
                    pr_err("[GPIO] %s: gpio %d is not valid\n",  __FUNCTION__, gpio);

                if (gpio_request(gpio, node->name))
                    pr_err("[GPIO] %s: gpio %d is in use\n",  __FUNCTION__, gpio);

                gpio_free(gpio);
            }
            else
                pr_err("[GPIO] %s: Could not get gpio from of \n", __FUNCTION__);
        }
    }
    else
        RTK_GPIO_INF("No default gpio need to set");

}

static const struct of_device_id rtk_gpio_of_match[] = {
    {.compatible = "realtek,rtl8117-gpio",},
    { /* Sentinel */ },
};

static int rtk_gpio_probe(struct platform_device *pdev)
{
    void __iomem *gpio_regs_base;
    struct rtk_gpio_controller *p_rtk_gpio_ctl;
    u32 gpio_numbers;
    u32 gpio_base;
    struct device_node *node = NULL;

    RTK_GPIO_DBG("[%s]", __FUNCTION__);
    node = pdev->dev.of_node;
    if (!node)
    {
        printk(KERN_ERR "rtk gpio: failed to allocate device structure.\n");
        return -ENODEV;
    }

    RTK_GPIO_DBG("[%s] node name = [%s]", __FUNCTION__, node->name);

    p_rtk_gpio_ctl = kzalloc(sizeof(struct rtk_gpio_controller), GFP_KERNEL);
    if (!p_rtk_gpio_ctl)
    {
        printk(KERN_ERR "rtk gpio: failed to allocate device structure.\n");
        return -ENOMEM;
    }
    memset(p_rtk_gpio_ctl, 0, sizeof(struct rtk_gpio_controller));

    if (of_property_read_u32_array(node, "Realtek,gpio_numbers", &gpio_numbers, 1))
    {
        printk(KERN_ERR "Don't know gpio group number.\n");
        return -EINVAL;
    }

    if (of_property_read_u32_index(node, "Realtek,gpio_base", 0, &gpio_base))
    {
        printk(KERN_ERR "Don't know gpio group number.\n");
        return -EINVAL;
    }

    p_rtk_gpio_ctl->chip.label = node->name;
    p_rtk_gpio_ctl->chip.request = gpiochip_generic_request;
    p_rtk_gpio_ctl->chip.free = gpiochip_generic_free;
    p_rtk_gpio_ctl->chip.direction_input = rtk_gpio_direction_in;
    p_rtk_gpio_ctl->chip.get = rtk_gpio_get;
    p_rtk_gpio_ctl->chip.direction_output = rtk_gpio_direction_out;
    p_rtk_gpio_ctl->chip.set = rtk_gpio_set;
    p_rtk_gpio_ctl->chip.base = gpio_base;
    p_rtk_gpio_ctl->chip.ngpio = gpio_numbers;
    of_property_read_u32_index(node, "#gpio-cells", 0, &p_rtk_gpio_ctl->chip.of_gpio_n_cells);
    p_rtk_gpio_ctl->chip.of_xlate = rtk_gpio_xlate;
    p_rtk_gpio_ctl->chip.of_node = node;

    spin_lock_init(&p_rtk_gpio_ctl->lock);

    gpio_regs_base = of_iomap(node, 0);
    if (!gpio_regs_base)
    {
        printk(KERN_ERR "unable to map gpio_regs_base registers\n");
        return -EINVAL;
    }

    p_rtk_gpio_ctl->gpio_regs_base = (void __iomem *)gpio_regs_base;

    if (gpiochip_add(&p_rtk_gpio_ctl->chip))
    {
        printk("[%s]  %s  line: %d \n", __FILE__, __FUNCTION__, __LINE__);

    }

    platform_set_drvdata(pdev,p_rtk_gpio_ctl);

    /* Initial default gpios */
    set_default_gpio(node);

    return 0;
}

static struct platform_driver rtk_gpio_driver = {
    .driver = {
        .name = "rtk_gpio",
        .owner = THIS_MODULE,
        .of_match_table = rtk_gpio_of_match,
    },
    .probe = rtk_gpio_probe,
};

static int rtk_gpio_drv_reg(void)
{
    return platform_driver_register(&rtk_gpio_driver);
}

postcore_initcall(rtk_gpio_drv_reg);
