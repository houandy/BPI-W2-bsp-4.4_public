#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/suspend.h>

#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <asm/io.h>
#include <linux/semaphore.h>
#include <rtl8117_platform.h>

#define NUM_CPUS 1

#define RTK_RISC_FREQ_MAX 4

#define DCO_CALIBRATITION_FREQ 388
#define PRECISION 20

#ifdef REV_B
#define USE_CALIBRATION
#endif

static void uart_reinit(struct cpufreq_freqs *freqs);
u16 cnt_cal_r;
static unsigned int counter=0;

static struct cpufreq_frequency_table *realtek_8117_freq_table = NULL;
static unsigned int freq_table_counter;
static unsigned int uart_baud_rate;
static unsigned int spi_max_freq;

#ifdef USE_CALIBRATION
static struct work_struct work;
static struct timer_list timer;
static void dco_calibate_handler(unsigned long __opaque);
#endif

static int rtl8117_cpufreq_notifier(struct notifier_block *nb,
				 unsigned long val, void *data)
{
	//printk("val is %ld, jiffy %ld\n", val, loops_per_jiffy);
	if (val == CPUFREQ_POSTCHANGE)
		current_cpu_data.udelay_val = loops_per_jiffy;

	return NOTIFY_OK;
}

static struct notifier_block rtl8117_cpufreq_notifier_block = {
	.notifier_call = rtl8117_cpufreq_notifier
};

static void DCO_2_RISC(struct cpufreq_freqs *freqs)
{
    volatile unsigned int temp;
    OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_READ_OP);	
    temp &= ~(1<<10);
    OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_WRITE_OP);
}

static void rtk_show_regs(void)
{
    volatile u32 temp;
    OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_READ_OP);	
    printk("===========Show OOB MAC REGS================\n");
    printk("addr: 0x%x, value: 0x%x.\n", CLKSW_SET_REG, temp);
    printk("===========Show CPU0 REGS===================\n");
    printk("addr: 0x%x, value: 0x%x.\n", 0x04, readl(CPU1_BASE_ADDR+0x04));
    printk("addr: 0x%x, value: 0x%x.\n", 0x08, readl(CPU1_BASE_ADDR+0x08));
    printk("addr: 0x%x, value: 0x%x.\n", 0x0C, readl(CPU1_BASE_ADDR+0x0C));
    printk("addr: 0x%x, value: 0x%x.\n", 0x10, readl(CPU1_BASE_ADDR+0x10));
    printk("addr: 0x%x, value: 0x%x.\n", 0x14, readl(CPU1_BASE_ADDR+0x14));
    printk("===========Show CPU1 REGS===================\n");
    printk("addr: 0x%x, value: 0x%x.\n", 0x0C, readl(CPU2_BASE_ADDR+0x0C));
    printk("===========Show TIMER REGS===================\n");
    printk("addr: 0x%x, value: 0x%x.\n", 0x00, readl(TIMER_BASE_ADDR+TIMER_LC));
    printk("===========Show FLASH REGS===================\n");
    printk("addr: 0x%x, value: 0x%x.\n", FLASH_BAUDR, readl(SPI_BASE_ADDR+FLASH_BAUDR));
}
#ifdef USE_CALIBRATION
static void DCO_2_DCO(struct cpufreq_freqs *freqs)
{
    volatile unsigned int temp;
    unsigned int i, exceed_flag=0;
    unsigned int ref_cnt_temp = 100*PRECISION*DCO_CALIBRATITION_FREQ/25;

        writel(readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)|(1<<EN_DCO_500M), CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);

        do {
            if (counter==64)
                break;
            temp = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)&0xffff81ff|(counter<<REF_DCO_500M);
            writel(temp, CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);
            temp = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)|(1<<REF_DCO_500M_VALID);
            writel(temp, CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);

            udelay(15);
            temp = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)&(~(1 << REF_DCO_500M_VALID))|((100*PRECISION) << FRE_REF_COUNT);//&(~(FREQ_CAL_EN))|(FREQ_CAL_EN);
            writel(temp, CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);
            writel(readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)&(~(FREQ_CAL_EN)), CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);
            writel(readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)|(FREQ_CAL_EN), CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);

            udelay(10);
            while((readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG1)&0x3)!=0x3);
            cnt_cal_r = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG1) >> 16;
            if((cnt_cal_r <= ref_cnt_temp)) {
                if (exceed_flag==1)
                    break;
            }
            else
            {
                exceed_flag=1;
                counter=counter-2;
                if (counter>=0)
                    continue;
                else
                {
                    printk("error: DCO Calibration wrong.\n");
                    break;
                }
            }
        } while(++counter);
        //uart_reinit(freqs);
}
#endif

static void spi_flash_clock_reinit(struct cpufreq_freqs *freqs)
{
    u32 baud_old, tmp=0;

    writel(0, SPI_BASE_ADDR + FLASH_SSIENR);
    if (freqs->new == DCO_FREQ)
    {
        tmp = readl(SPI_BASE_ADDR + FLASH_BAUDR)&0xF;
        baud_old = tmp;
        if (tmp == 0)
            tmp =1;
        while (((DCO_FREQ>>1)/tmp) > (spi_max_freq/1000))
        {
            tmp <<=1;
        }
        if (baud_old != tmp)
            writel(tmp, SPI_BASE_ADDR + FLASH_BAUDR);
    }
    /*fixbug: os may hung when spi buad rate = cpu_clock/2*/
    //else if (freqs->new < RISC_FREQ)
    //    writel(0, SPI_BASE_ADDR + FLASH_BAUDR);
    else
        writel(1, SPI_BASE_ADDR + FLASH_BAUDR);
    writel(1, SPI_BASE_ADDR + FLASH_SSIENR);
}

/*
	ref_cnt_temp = 100*388/25, why not choose 400?
	Becuase HW cann't acheive 400MHz, so choose a value close to 400.
*/
static void RISC_2_DCO(struct cpufreq_freqs *freqs)
{
    volatile unsigned int temp;
    unsigned int i, exceed_flag=0;
    unsigned int ref_cnt_temp = 100*PRECISION*DCO_CALIBRATITION_FREQ/25;

    if (!(readl(CPU2_BASE_ADDR + CPU2_DCO_CAL)&0x00000001))
    {
        DCO_2_RISC(freqs);
        writel(readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)|(1<<EN_DCO_500M), CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);

        do {
            if (counter==64)
                break;
            temp = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)&0xffff81ff|(counter<<REF_DCO_500M);
            writel(temp, CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);
            temp = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)|(1<<REF_DCO_500M_VALID);
            writel(temp, CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);

            udelay(15);
            temp = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)&(~(1 << REF_DCO_500M_VALID))|((100*PRECISION) << FRE_REF_COUNT);//&(~(FREQ_CAL_EN))|(FREQ_CAL_EN);
            writel(temp, CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);
            writel(readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)&(~(FREQ_CAL_EN)), CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);
            writel(readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0)|(FREQ_CAL_EN), CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG0);

            udelay(10);
            while((readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG1)&0x3)!=0x3);
            cnt_cal_r = readl(CPU1_BASE_ADDR + CPU1_FREQ_CAL_REG1) >> 16;
            if((cnt_cal_r <= ref_cnt_temp)) {
                if (exceed_flag==1)
                    break;
            }
            else
            {
                exceed_flag=1;
                counter=counter-2;
                if (counter>=0)
                    continue;
                else
                {
                    printk("error: DCO Calibration wrong.\n");
                    break;
                }
            }
        } while(++counter);

        uart_reinit(freqs);
        writel(readl(CPU2_BASE_ADDR + CPU2_DCO_CAL)|0x40000000, CPU2_BASE_ADDR + CPU2_DCO_CAL);
    	OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_READ_OP);	
    	temp |= (1 << 10);
    	OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_WRITE_OP);
    }
}

static inline void timer_reinit(struct cpufreq_freqs *freqs)
{
    if (freqs->new != DCO_FREQ)
        writel(freqs->new*1000/(2*HZ), TIMER_BASE_ADDR+TIMER_LC);
    else
        writel(cnt_cal_r*(1000000/PRECISION)/(4*2*HZ), TIMER_BASE_ADDR+TIMER_LC);
}

/*
	cpu clock is changed from 400 MHz to 250MHz or vice, uart clock frequency can remain.
	frequency is more lower, DLL needs more preciser.
	according to dll=cpu_clock/2/baudrate/16, cpu_clock=62.5MHz, then dll shoule be 0x10, but the serial port will output messay code.
	If buardrate is changed, the dll needs to be re-calcutated.
*/
static void uart_reinit(struct cpufreq_freqs *freqs)
{

    volatile unsigned int temp;
    unsigned int saved_ier;
    if (freqs->new == DCO_FREQ)
    {
        temp =(cnt_cal_r*(1000000/PRECISION)/4/2/uart_baud_rate/16);
        if (((cnt_cal_r*(1000000/PRECISION))%(4*2*uart_baud_rate*16)) > (4*uart_baud_rate*16))
            temp +=1;
    }
    //writel(readl(CPU1_BASE_ADDR + CPU1_CTRL_REG)|0x10, CPU1_BASE_ADDR + CPU1_CTRL_REG);
    saved_ier = readl(UART_BASE_ADDR + UART_IER);
    writel(0x00, UART_BASE_ADDR + UART_IER);
    //fixbug: serial port output messay code when cpu clock is changed.
    //wait until all messges in tx fifo is outputed.
    mdelay(1);
    writel(0x80, UART_BASE_ADDR + UART_LCR);

#if 1
    if (uart_baud_rate==115200)
    {
        if (freqs->new == 62500)
            temp=0x0011;
    }
    else if (uart_baud_rate==57600)
    {
        if (freqs->new == 62500)
            temp=0x0022;
        else if (freqs->new == 31250)
            temp=0x0011;
        else if (freqs->new == 125000)
            temp=0x0044;
        else if (freqs->new == 250000)
            temp=0x0088;
    }
    else if (uart_baud_rate==9600)
    {
        if (freqs->new == 31250)
            temp=0x0066;
    }
#endif
    writel((u8)temp, UART_BASE_ADDR + UART_DLL);
    writel((u8)(temp>>8), UART_BASE_ADDR + UART_DLH);

    writel(0x03, UART_BASE_ADDR + UART_LCR);
    /*for B-cut tx/rx share pin with led*/
    OOB_access_IB_reg(PIN_REG, &temp, 0xf, OOB_READ_OP);	
    temp |=0x40000 ;
    temp &=0xfffdffff;
    OOB_access_IB_reg(PIN_REG, &temp, 0xf, OOB_WRITE_OP);
    //fixbug: if IER&0xf == 0xf, next output message after uart_reinit needs to trigged through key.
    //saved_ier |= 0xf;
    writel(saved_ier, UART_BASE_ADDR + UART_IER);
}

static void RISC_freq_scaling_cpu(struct cpufreq_freqs *freqs)
{
    unsigned int clk_div = 0;
    volatile unsigned int temp;
    temp = readl(CPU1_BASE_ADDR + CPU1_CTRL_REG);
    temp &= ~((0x07)|(1<<11));
    temp |= (1<<6);
    while ((clk_div <= RTK_RISC_FREQ_MAX) && (((RISC_FREQ/(1<<clk_div)) > freqs->new)))
        clk_div += 1;
    temp |= clk_div;
    uart_reinit(freqs);
    writel(temp, CPU1_BASE_ADDR + CPU1_CTRL_REG);
    udelay(5);
}

static void DCO_upgrade(struct cpufreq_freqs *freqs)
{
    spi_flash_clock_reinit(freqs);
    if (freqs->new == DCO_FREQ)
    {
        if (freqs->old != RISC_FREQ)
        {
            freqs->new = RISC_FREQ;
            RISC_freq_scaling_cpu(freqs);
            freqs->new = DCO_FREQ;
            freqs->old = RISC_FREQ;
        }
	    //udelay(5);
        RISC_2_DCO(freqs);
#ifdef USE_CALIBRATION
        timer.expires = jiffies + 1*HZ/10;
        timer.data = 0;
        timer.function = dco_calibate_handler;
        add_timer(&timer);
#endif
    }
    else
        RISC_freq_scaling_cpu(freqs);
}

static void DCO_downgrade(struct cpufreq_freqs *freqs)
{
    unsigned int tmp = freqs->new;
    if (freqs->old == DCO_FREQ)
    {
#ifdef USE_CALIBRATION
        del_timer(&timer);
#endif
        freqs->new = RISC_FREQ;
        uart_reinit(freqs);
        DCO_2_RISC(freqs);
        //fixbug: wait until new value is written to 0xe018.
        //udelay(5);
        if (tmp < RISC_FREQ)
        {
            freqs->old = RISC_FREQ;
            freqs->new = tmp;
            RISC_freq_scaling_cpu(freqs);
        }
    }
    else
    {
        DCO_2_RISC(freqs);
        //mdelay(5);
        RISC_freq_scaling_cpu(freqs);
    }
    //udelay(5);
    spi_flash_clock_reinit(freqs);
}

static unsigned int rtl8117_cpufreq_get_speed(unsigned int cpu)
{
    unsigned int clk_freq=0;
    volatile unsigned int temp;
    if (cpu >= NUM_CPUS)
        return 0;

    OOB_access_IB_reg(CLKSW_SET_REG, &temp, 0xf, OOB_READ_OP);	
    if (temp & (1<<EN_DCO_CLK))
        //choose DCO
        clk_freq = realtek_8117_freq_table[0].frequency;
    else
    {
        //choose RISC clock
        switch (readl(CPU1_BASE_ADDR+CPU1_CTRL_REG)&0x3)
        {
        case 0:
            clk_freq = realtek_8117_freq_table[1].frequency;
            break;
        case 1:
            clk_freq = realtek_8117_freq_table[2].frequency;
            break;
        case 2:
            clk_freq = realtek_8117_freq_table[3].frequency;
            break;
        case 3:
            clk_freq = realtek_8117_freq_table[4].frequency;
            break;
        default:
            clk_freq = realtek_8117_freq_table[freq_table_counter-1].frequency;
            break;
        }
    }
    return clk_freq;
}


static int rtl8117_update_cpu_speed(struct cpufreq_policy *policy, unsigned int idx)
{
    int ret = 0, result = 0;
    struct cpufreq_freqs freqs;

    freqs.old = rtl8117_cpufreq_get_speed(0);
    freqs.new = realtek_8117_freq_table[idx].frequency;
    freqs.flags = 0;
    if (freqs.new == freqs.old)
    {
        return ret;
    }

    cpufreq_freq_transition_begin(policy, &freqs);
    if (freqs.new > freqs.old)
        DCO_upgrade(&freqs);
    else if (freqs.new < freqs.old)
        DCO_downgrade(&freqs);

#ifdef REV_B
    timer_reinit(&freqs);
#endif
    cpufreq_freq_transition_end(policy, &freqs, result);
    return ret;
}

static int rtl8117_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
    return cpufreq_frequency_table_verify(policy, realtek_8117_freq_table);
}

static int rtl8117_cpufreq_driver_init(struct cpufreq_policy *policy)
{
    int ret=0;

    if (policy->cpu >= NUM_CPUS)
        return -EINVAL;
    if (realtek_8117_freq_table == NULL) {
        printk("No frequency information for this CPU\n");
        return -ENODEV;
    }

    policy->cpuinfo.transition_latency = 0;
    ret = cpufreq_generic_init(policy, realtek_8117_freq_table, policy->cpuinfo.transition_latency);

    return ret;
}

static struct cpufreq_driver rtl8117_cpufreq_driver = {
    .flags  	= CPUFREQ_STICKY | CPUFREQ_ASYNC_NOTIFICATION,
    .verify 	= rtl8117_cpufreq_verify_speed,
    .target_index 	= rtl8117_update_cpu_speed,
    .get    	= rtl8117_cpufreq_get_speed,
    .init   	= rtl8117_cpufreq_driver_init,
    .name   	= "rtl8117-cpufreq",
    .attr   	= cpufreq_generic_attr,
};

static int rtk_dvfs_frequency_filter(unsigned int frequency)
{
    if ((frequency > realtek_8117_freq_table[0].frequency) || (frequency < realtek_8117_freq_table[freq_table_counter-1].frequency))
        return -EINVAL;
    return 0;
}

static struct of_device_id rtk_dfs_ids[] = {
    { .compatible = "realtek,rtl8117-dfs" },
    { /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, rtk_dfs_ids);

#ifdef USE_CALIBRATION
static void work_handler(struct work_struct *work)
{
    struct cpufreq_freqs freqs;
    freqs.old = rtl8117_cpufreq_get_speed(0);
    freqs.new = DCO_FREQ;
    freqs.flags = 0;
    DCO_2_DCO(&freqs);
#ifdef REV_B
    timer_reinit(&freqs);
    mod_timer(&timer, jiffies + 1*HZ/10);
#endif
}
static void dco_calibate_handler(unsigned long __opaque)
{
    schedule_work(&work);
}
#endif

static int rtk_dfs_probe(struct platform_device *pdev)
{
    int ret=0;
    struct cpufreq_frequency_table *freq;
    const u32 *prop;
    int size;
    int i, err;
    char *endptr=NULL;
    struct device_node *np;
    char *options;

    prop = of_get_property(pdev->dev.of_node, "frequency-table", &size);
    if (prop)
    {
        freq_table_counter = size / (sizeof(u32));
        if (realtek_8117_freq_table == NULL) {
            realtek_8117_freq_table = (struct cpufreq_frequency_table*)
                                      kzalloc(sizeof(struct cpufreq_frequency_table) * (freq_table_counter+1),GFP_KERNEL);
            freq = realtek_8117_freq_table;
            for (i=0; i<freq_table_counter; i+=1) {
                freq->flags = 0;
                freq->driver_data = 0;
                freq->frequency = of_read_number(prop+i,1);
                if (rtk_dvfs_frequency_filter(freq->frequency) != 0)
                    continue;
                freq++;
            }
            freq->flags = 0;
            freq->driver_data = 0;
            freq->frequency = CPUFREQ_TABLE_END;
        }
    } else {
        printk("[%s] frequency-table ERROR! err = %d \n",__func__,err);
        ret = err;
        goto error;
    }

    writel(readl(CPU1_BASE_ADDR + 0x14)|0x3ff, CPU1_BASE_ADDR + 0x14);

    np = of_find_node_by_path("/chosen");
    prop = of_get_property(np, "bootargs", NULL);
    if (prop)
    {
        options = strchr(prop, ',');
        if (options)
            *(options++) = 0;
        uart_baud_rate = simple_strtoul(options, &endptr, 10);
    }

    np = of_find_compatible_node(NULL, NULL, "spi-flash");
    if (of_property_read_u32_index(np, "spi-max-frequency", 0, &spi_max_freq))
    {
        printk(KERN_ERR "Don't know spi-max-frequency.\n");
    }

    //fix: default use RISC clock, but OOB MAC register 0xE018 bit 10 en_dco_clk is 1.
    DCO_2_RISC(NULL);
    /*After register cpufreq driver, scale cpu clock from default 200MHz to 400MHz.*/
    ret = cpufreq_register_notifier(&rtl8117_cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);

    ret = cpufreq_register_driver(&rtl8117_cpufreq_driver);
#ifdef USE_CALIBRATION
    INIT_WORK(&work, work_handler);
    init_timer(&timer);
#endif

error:
    return ret;
}
static void rtk_dfs_shutdown(struct platform_device *pdev)
{
    cpufreq_unregister_notifier(&rtl8117_cpufreq_notifier_block,CPUFREQ_TRANSITION_NOTIFIER);
}

static struct platform_driver realtek_cpufreq_platdrv = {
    .driver = {
        .name   = "rtl8117-cpufreq",
        .owner  = THIS_MODULE,
        .of_match_table = rtk_dfs_ids,
    },
    .probe      = rtk_dfs_probe,
    .shutdown   = rtk_dfs_shutdown,
};

module_platform_driver_probe(realtek_cpufreq_platdrv, rtk_dfs_probe);
