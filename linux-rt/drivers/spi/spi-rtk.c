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
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

/*detail functions*/
static inline u32 rtk_readl(struct rtk_spi *rtks, u32 offset)
{
    return __raw_readl(rtks->base + offset);
}

static inline void rtk_writel(struct rtk_spi *rtks, u32 offset, u32 val)
{
    __raw_writel(val, rtks->base + offset);
}

static inline u16 rtk_readb(struct rtk_spi *rtks, u32 offset)
{
    return __raw_readb(rtks->base + offset);
}

static inline void rtk_writeb(struct rtk_spi *rtks, u32 offset, u16 val)
{
    __raw_writeb(val, rtks->base + offset);
}

static inline struct rtk_spi *rtk_spi_to_hw(struct spi_device *sdev)
{
    return spi_master_get_devdata(sdev->master);
}

static void Check_SPIC_Busy(struct rtk_spi *rtks)
{
    u32   spic_busy;
    do {
        spic_busy=rtk_readl(rtks, FLASH_SR);
    } while(spic_busy!=0x06);
}

static inline void spi_enable_chip(struct rtk_spi *rtks, int enable)
{
    rtk_writel(rtks, FLASH_SSIENR, (enable ? 1 : 0));
}

static void Flash_Exit4byte_Addrmode (struct rtk_spi *rtks)
{
    spi_enable_chip(rtks, 0);
    rtk_writel(rtks, FLASH_SER, 0x01);
    rtk_writel(rtks, FLASH_CTRLR1, 0x00);
    rtk_writel(rtks, FLASH_CTRLR0, 0x000);
    spi_enable_chip(rtks, 1);

    rtk_writeb(rtks, FLASH_DR, OPCODE_EX4B);
    Check_SPIC_Busy(rtks);
}

void Flash_RDID(struct rtk_spi *rtks)
{
    u32   flash_ID,spic_busy;

    spi_enable_chip(rtks, 0);
    rtk_writel(rtks, FLASH_CTRLR0, 0x300);
    rtk_writel(rtks, FLASH_SER, 0x01);
    rtk_writel(rtks, FLASH_CTRLR1, 0x03);
    spi_enable_chip(rtks, 1);
    rtk_writeb(rtks, FLASH_DR, FLASH_RDID_COM);
    do {
        spic_busy=rtk_readl(rtks, FLASH_SR);
        spic_busy=spic_busy & 0x00000001;
    } while(spic_busy!=0x00);
    flash_ID = (rtk_readb(rtks, FLASH_DR)<<16) + (rtk_readb(rtks, FLASH_DR)<<8) + rtk_readb(rtks, FLASH_DR);
    //winbond 256Mb flash default address mode is 4byte address !!!
    if (((flash_ID>>16) == 0xef)&&((flash_ID&0x0000ff)==0x19)) {
        Flash_Exit4byte_Addrmode(rtks);
    }
    rtks->flash_id = flash_ID;
}

static void Flash_RDSR(struct rtk_spi *rtks)
{
    u8   spic_busy, flash_busy;

    do {
        spi_enable_chip(rtks, 0);
        rtk_writel(rtks, FLASH_CTRLR0, 0x300);
        rtk_writel(rtks, FLASH_SER, 0x01);
        rtk_writel(rtks, FLASH_CTRLR1, 0x01);
        spi_enable_chip(rtks, 1);
        rtk_writeb(rtks, FLASH_DR, FLASH_RDSR_COM);
        //check spic busy?
        do {
            spic_busy=rtk_readb(rtks, FLASH_SR);
            spic_busy=spic_busy & 0x01;
        } while(spic_busy!=0x00);
        //check flash busy?
        flash_busy=rtk_readb(rtks, FLASH_DR);
        flash_busy=flash_busy & 0x03;
    } while(flash_busy==0x03);
}

//unprotect the flash
//when write /erase falsh, must call this function firstly
//NOTE: because different rand flash has different manner to declare/set quad enable ,
static void Flash_WRSR_unprotect(struct rtk_spi *rtks)
{
    //Setup SPIC
    spi_enable_chip(rtks, 0);
    rtk_writel(rtks, FLASH_ADDR_LENGTH, 0x01);
    rtk_writel(rtks, FLASH_VALID_CMD, 0x200);
    rtk_writel(rtks, FLASH_SER, 0x01);
    rtk_writel(rtks, FLASH_CTRLR1, 0x00);
    rtk_writel(rtks, FLASH_CTRLR0, 0x000);
    spi_enable_chip(rtks, 1);

    //Write enable in advance
    rtk_writeb(rtks, FLASH_DR, FLASH_WREN_COM);
    Check_SPIC_Busy(rtks);

    spi_enable_chip(rtks, 0);
    rtk_writeb(rtks, FLASH_DR, FLASH_WRSR_COM);
    //0x00: unprotect the flash, SRWD(status register write protect)=0, QE(Quad Enable)=0,BP3~0(level of protected block)=0
    //WEL(write enable latch)=0, WIP(write in progress bit)=0
    rtk_writeb(rtks, FLASH_DR, 0x00);
    spi_enable_chip(rtks, 1);
    Check_SPIC_Busy(rtks);
    Flash_RDSR(rtks);
}

static void spi_hw_init(struct rtk_spi *rtks)
{
    spi_enable_chip(rtks, 0);
    rtk_writel(rtks, FLASH_FLUSH_FIFO, 0x01);
    rtk_writel(rtks, FLASH_CTRLR0, 0x0);
    rtk_writel(rtks, FLASH_ADDR_CTRLR2, 0x81);
    spi_enable_chip(rtks, 1);
    rtks->fifo_len = 0x100;
    Flash_RDID(rtks);
    Flash_WRSR_unprotect(rtks);
}

void Flash_erase(struct rtk_spi *rtks, u32 Address, u32 CMD)
{
    u32   DWtmp;

    //Setup SPIC
    spi_enable_chip(rtks, 0);
    rtk_writel(rtks, FLASH_ADDR_LENGTH, 3);
    rtk_writel(rtks, FLASH_SER, 0x01);
    rtk_writel(rtks, FLASH_CTRLR1, 0x00);
    rtk_writel(rtks, FLASH_CTRLR0, 0x000);
    spi_enable_chip(rtks, 1);

    //Write enable
    rtk_writeb(rtks, FLASH_DR, FLASH_WREN_COM);
    Check_SPIC_Busy(rtks);
    //Set command and address
    DWtmp = Address >> 16;
    DWtmp = DWtmp + (Address & 0x0000ff00);
    DWtmp = DWtmp + ((Address << 16) & 0x00ff0000);
    DWtmp = (DWtmp << 8 )+ CMD;
    rtk_writel(rtks, FLASH_DR, DWtmp);
    Check_SPIC_Busy(rtks);
    //RDSR
    Flash_RDSR(rtks);
}

static void Set_SPIC_Write_channel(struct rtk_spi *rtks)
{
    spi_enable_chip(rtks, 0);
    rtk_writel(rtks, FLASH_CTRLR0, 0x0);
    rtk_writel(rtks, FLASH_CTRLR1, 0x00);
    rtk_writel(rtks, FLASH_ADDR_LENGTH, 0x03);
    rtk_writel(rtks, FLASH_VALID_CMD, 0x200);
    spi_enable_chip(rtks, 1);
}

/*********************************************************
*description:
    user mode to write data to spi flash
*parameter:
    NDF:byte length
    Address:SPI address(relately 24/32-bit address),
    DReadBuffer: write buffer's point
*Note:ex,when write flash men address 0x82000000,then ,address is 0x00;
    the max  NDF is 128 bytes !!!
**********************************************************/
void Flash_write(struct rtk_spi *rtks, u32 NDF, u32 Address, u32 *DReadBuffer)
{
    u32    DWtmp, i;
    u8    *BReadBuffer;
    BReadBuffer=(u8 *)DReadBuffer;

    Set_SPIC_Write_channel(rtks);
    //Write Enable
    rtk_writeb(rtks, FLASH_DR, FLASH_WREN_COM);
    Check_SPIC_Busy(rtks);
    spi_enable_chip(rtks, 0);
    //Write Command and Address
    DWtmp = Address >> 16;
    DWtmp = DWtmp + (Address & 0x0000ff00);
    DWtmp = DWtmp + ((Address << 16) & 0x00ff0000);
    DWtmp = (DWtmp << 8 )+ FLASH_PP_COM;
    rtk_writel(rtks, FLASH_DR, DWtmp);
    if (NDF%4)
    {
        for(i=0; i<NDF; i++)
            rtk_writeb(rtks, FLASH_DR, *(BReadBuffer+i));
    }
    else
    {
        for(i=0; i<NDF/4; i++)
            rtk_writel(rtks, FLASH_DR, *(DReadBuffer+i));
    }
    spi_enable_chip(rtks, 1);

    Check_SPIC_Busy(rtks);
    Flash_RDSR(rtks);
}

#ifdef CONFIG_DEBUG_FS
#define SPI_REGS_BUFSIZE	1024
static ssize_t rtk_spi_show_regs(struct file *file, char __user *user_buf,
                                 size_t count, loff_t *ppos)
{
    struct rtk_spi *rtks = file->private_data;
    char *buf;
    u32 len = 0;
    ssize_t ret;

    buf = kzalloc(SPI_REGS_BUFSIZE, GFP_KERNEL);
    if (!buf)
        return 0;

    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "%s registers:\n", dev_name(&rtks->master->dev));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "=================================\n");
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "CTRL0: \t\t0x%08x\n", rtk_readl(rtks, FLASH_CTRLR0));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "CTRL1: \t\t0x%08x\n", rtk_readl(rtks, FLASH_CTRLR1));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "SSIENR: \t0x%08x\n", rtk_readl(rtks, FLASH_SSIENR));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "SER: \t\t0x%08x\n", rtk_readl(rtks, FLASH_SER));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "BAUDR: \t\t0x%08x\n", rtk_readl(rtks, FLASH_BAUDR));
//	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
//			"TXFTLR: \t0x%08x\n", rtk_readl(rtks, FLASH_TXFLTR));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "RXFTLR: \t0x%08x\n", rtk_readl(rtks, FLASH_RXFTLR));
//	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
//			"TXFLR: \t\t0x%08x\n", rtk_readl(rtks, FLASH_TXFLR));
//	len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
//			"RXFLR: \t\t0x%08x\n", rtk_readl(rtks, FLASH_RXFLR));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "SR: \t\t0x%08x\n", rtk_readl(rtks, FLASH_SR));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "IMR: \t\t0x%08x\n", rtk_readl(rtks, FLASH_IMR));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "ISR: \t\t0x%08x\n", rtk_readl(rtks, FLASH_ISR));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "RDID: \t\t0x%08x\n",  rtks->flash_id);
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "AUTOLEN: \t\t0x%08x\n",  rtk_readl(rtks, FLASH_AUTO_LENGTH));
    len += snprintf(buf + len, SPI_REGS_BUFSIZE - len,
                    "=================================\n");

    ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
    kfree(buf);
    return ret;
}

static const struct file_operations rtk_spi_regs_ops = {
    .owner		= THIS_MODULE,
    .open		= simple_open,
    .read		= rtk_spi_show_regs,
    .llseek		= default_llseek,
};

static int rtk_spi_debugfs_init(struct rtk_spi *rtks)
{
    rtks->debugfs = debugfs_create_dir("rtk_spi", NULL);
    if (!rtks->debugfs)
        return -ENOMEM;

    debugfs_create_file("registers", S_IFREG | S_IRUGO,
                        rtks->debugfs, (void *)rtks, &rtk_spi_regs_ops);
    return 0;
}

static void rtk_spi_debugfs_remove(struct rtk_spi *rtks)
{
    debugfs_remove_recursive(rtks->debugfs);
}

#else
static inline int rtk_spi_debugfs_init(struct rtk_spi *rtks)
{
    return 0;
}

static inline void rtk_spi_debugfs_remove(struct rtk_spi *rtks)
{
}
#endif /* CONFIG_DEBUG_FS */

static void rtk_writer(struct rtk_spi *rtks)
{
    Flash_erase(rtks, (rtks->excha_addr & 0x00FFFFFF), FLASH_CHIP_SEC);
    rtk_flash_write(rtks, rtks->excha_addr, rtks->tx, rtks->len);
}


static int rtk_reader(struct rtk_spi *rtks)
{
    rtk_memcpy((void *)rtks->rx, (void *)rtks->excha_addr, rtks->len);
    return 0;
}

static int rtk_spi_transfer_one(struct spi_master *master,
                                struct spi_device *spi, struct spi_transfer *transfer)
{
    struct rtk_spi *rtks = spi_master_get_devdata(master);

    rtks->tx = (void *)transfer->tx_buf;
    rtks->tx_end = rtks->tx + transfer->len;
    rtks->rx = transfer->rx_buf;
    rtks->rx_end = rtks->rx + transfer->len;
    rtks->len = transfer->len;

    /* Handle per transfer options for bpw and speed */
    if ((transfer->speed_hz != 0) && (transfer->speed_hz != rtks->speed_hz))
        rtk_spi_set_frequency(spi, transfer);

    do {
        rtk_writer(rtks);
        rtk_reader(rtks);
        cpu_relax();
    } while (rtks->rx_end > rtks->rx);

    return 0;
}

static void rtk_spi_set_cs(struct spi_device *spi, bool enable)
{
    return;
}

static void rtk_spi_handle_err(struct spi_master *master,
                               struct spi_message *msg)
{
    return;
}

int rtk_spi_set_frequency(struct spi_device *spi,
                                 struct spi_transfer *t)
{
    struct rtk_spi *hw = rtk_spi_to_hw(spi);
    struct rtk_spi_devstate *cs = spi->controller_state;
    unsigned int hz;
    unsigned int rtk_spi_max_clk = hw->cpu_freq>>1;

    hz  = t ? t->speed_hz : spi->max_speed_hz;

    if (!hz)
        hz = spi->max_speed_hz;

    if (hz > rtk_spi_max_clk)
        hz = rtk_spi_max_clk;
    cs->sck_div =1;

    while ((cs->sck_div <= rtk_spi_max_clk) && ((hw->cpu_freq /(2 << cs->sck_div) > hz)))
        cs->sck_div<<=1;
    spi_enable_chip(hw, 0);
    rtk_writel(hw, FLASH_BAUDR, cs->sck_div);
    if (cs->sck_div ==1)
        rtk_writel(hw, FLASH_AUTO_LENGTH, (rtk_readl(hw, FLASH_AUTO_LENGTH)&0xffff0000)|hw->dummy_cycle);
    spi_enable_chip(hw, 1);
    return 0;
}

/* This may be called twice for each spi dev */
static int rtk_spi_setup(struct spi_device *spi)
{
    struct rtk_spi_devstate *cs = spi->controller_state;

    /* allocate settings on the first call */
    if (!cs) {
        cs = kzalloc(sizeof(struct rtk_spi_devstate), GFP_KERNEL);
        if (!cs)
            return -ENOMEM;

        cs->hz = -1;
        spi->controller_state = cs;
    }

    rtk_spi_set_frequency(spi, NULL);
    return 0;
}

static void rtk_spi_cleanup(struct spi_device *spi)
{
    struct rtk_spi_devstate *cs = spi->controller_state;
    if (cs) {
        kfree(cs);
    }
    return;
}

int rtk_spi_add_host_controller(struct device *dev, struct rtk_spi *rtks)
{
    struct spi_master *master;
    int ret;

    BUG_ON(rtks == NULL);

    master = spi_alloc_master(dev, 0);
    if (!master)
        return -ENOMEM;

    rtks->master = master;
    snprintf(rtks->name, sizeof(rtks->name), "rtk_spi%d", rtks->bus_num);
    /*init struct spi_master*/
    master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_LOOP;
    master->bits_per_word_mask = SPI_BPW_MASK(8) | SPI_BPW_MASK(32);
    master->bus_num = rtks->bus_num;
    master->num_chipselect = rtks->num_cs;
    master->setup = rtk_spi_setup;
    master->cleanup = rtk_spi_cleanup;
    master->set_cs = rtk_spi_set_cs;
    master->transfer_one = rtk_spi_transfer_one;
    master->handle_err = rtk_spi_handle_err;
    master->max_speed_hz = rtks->max_freq;
    master->dev.of_node = dev->of_node;

    /* Basic HW init */
    spi_hw_init(rtks);

    spi_master_set_devdata(master, rtks);
    ret = devm_spi_register_master(dev, master);
    if (ret) {
        dev_err(&master->dev, "problem registering spi master\n");
        goto err;
    }

    ret = rtk_spi_debugfs_init(rtks);
    if (ret) {
        dev_err(&master->dev, "problem create debug fs.\n");
        goto err;
    }
    return 0;
err:
    spi_enable_chip(rtks, 0);
    spi_master_put(master);
    return ret;
}
EXPORT_SYMBOL_GPL(rtk_spi_add_host_controller);

void rtk_spi_remove_host_controller(struct rtk_spi *rtks)
{
    rtk_spi_debugfs_remove(rtks);
}
EXPORT_SYMBOL_GPL(rtk_spi_remove_host_controller);
