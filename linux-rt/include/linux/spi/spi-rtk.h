#ifndef __LINUX_RTK_SPI_H
#define __LINUX_RTK_SPI_H
#include <linux/spi/spi.h>
/*
*register information
*struct defination
*fundamental function(read/write func)
*/
//Flash Controller Register setting
//For 8111EP
#define FLASH_DATA_ADDR            	0xA2000000
#define FLASH_BASE_ADDR            	0xBC000000
//flash control register
#define FLASH_CTRLR0             	0x00
#define FLASH_CTRLR1                0x04
#define FLASH_SSIENR                0x08
#define FLASH_SER                   0x10
#define FLASH_BAUDR                 0x14
#define FLASH_RXFTLR                0x1C
#define FLASH_SR                    0x28
#define FLASH_IMR                   0x2C
#define FLASH_ISR                   0x30
#define FLASH_DR                    0x60
#define FLASH_READ_DUAL_ADDR_DATA	0xE8
#define FLASH_ADDR_CTRLR2         	0x110
#define FLASH_ADDR_LENGTH         	0x118
#define FLASH_AUTO_LENGTH         	0x11C
#define FLASH_VALID_CMD				0x120
#define FLASH_SIZE_CONTRL			0x124
#define FLASH_FLUSH_FIFO			0x128

//flash control register interrupt
#define FLASH_INT_TXEIS             0x01
#define FLASH_INT_TXOIS             0x02
#define FLASH_INT_RXUIS             0x04
#define FLASH_INT_RXOIS             0x08
#define FLASH_INT_RXFIS             0x10
#define FLASH_INT_MSTIS             0x20

//flash command
#define FLASH_READ_COM              0x03
#define FLASH_FAST_READ_COM     	0x0B
#define FLASH_SE_COM                0x20
#define FLASH_BE_COM                0xD8
#define FLASH_CE_COM                0xC7
#define FLASH_WREN_COM              0x06
#define FLASH_WRDI_COM              0x04
#define FLASH_RDSR_COM              0x05
#define FLASH_WRSR_COM              0x01
#define FLASH_RDID_COM              0x9F
#define FLASH_REMS_COM              0x90
#define FLASH_RES_COM               0xAB
#define FLASH_PP_COM                0x02
#define FLASH_DP_COM                0xB9
#define FLASH_SE_PROTECT            0x36
#define FLASH_SE_UNPROTECT          0x39
#define FLASH_CHIP_ERA              0x60
#define FLASH_CHIP_BLK              0x52//erase 32K block command

/* Used for Macronix flashes only. */
#define	OPCODE_EN4B		0xB7	/* Enter 4-byte mode */
#define	OPCODE_EX4B		0xE9	/* Exit 4-byte mode */
/* Used for Spansion flashes only. */
#define	OPCODE_BRWR		0x17	/* Bank register write */


#define FLASH_RDCR_COM				0x15
#define FLASH_WREAR_COM				0xC5
#define FLASH_RDEAR_COM				0xC8
#define FLASH_RSTEN_COM				0x66
#define FLASH_RST_COM				0x99

//#define FLASH_SIZE					4*1024*1024/8
#define FLASH_CHIP_SEC       0x20

/* Erase commands */
#define CMD_ERASE_4K			0x20
#define CMD_ERASE_CHIP			0xc7
#define CMD_ERASE_64K			0xd8

/* Write commands */
#define CMD_WRITE_STATUS		0x01
#define CMD_PAGE_PROGRAM		0x02
#define CMD_WRITE_DISABLE		0x04
#define CMD_WRITE_ENABLE		0x06
#define CMD_QUAD_PAGE_PROGRAM		0x32
#define CMD_WRITE_EVCR			0x61

/* Read commands */
#define CMD_READ_ARRAY_SLOW		0x03
#define CMD_READ_ARRAY_FAST		0x0b
#define CMD_READ_DUAL_OUTPUT_FAST	0x3b
#define CMD_READ_DUAL_IO_FAST		0xbb
#define CMD_READ_QUAD_OUTPUT_FAST	0x6b
#define CMD_READ_QUAD_IO_FAST		0xeb
#define CMD_READ_ID			0x9f
#define CMD_READ_STATUS			0x05
#define CMD_READ_STATUS1		0x35
#define CMD_READ_CONFIG			0x35
#define CMD_FLAG_STATUS			0x70
#define CMD_READ_EVCR			0x65

struct rtk_spi_devstate {
    unsigned int	hz;
    u8		sck_div;
};

struct rtk_spi {
    void __iomem *base;
    int len;
    unsigned long excha_addr;

    char			name[16];
    u32 flash_id;
    u32			fifo_len;	/* depth of the FIFO buffer */
    u32			max_freq;	/* max bus freq supported */
    u32			cpu_freq;

    u32			bus_num;
    u32			num_cs;		/* supported slave numbers */
//    u8			type;
    u32			speed_hz;
    u32 dummy_cycle;

    /* data buffers */
    unsigned char *tx;
    unsigned char *rx;
    unsigned char *tx_end;
    unsigned char *rx_end;

    struct spi_master	*master;
    struct device		*dev;
#ifdef CONFIG_DEBUG_FS
    struct dentry *debugfs;
#endif
};


extern int rtk_spi_add_host_controller(struct device *dev, struct rtk_spi *rtks);
extern void rtk_spi_remove_host_controller(struct rtk_spi *rtks);
int rtk_spi_set_frequency(struct spi_device *spi, struct spi_transfer *t);
void Flash_erase(struct rtk_spi *rtks, u32 Address, u32 CMD);
void Flash_write(struct rtk_spi *rtks, u32 NDF, u32 Address, u32 *DReadBuffer);
void Flash_RDID(struct rtk_spi *rtks);


static size_t rtk_memcpy(void *dst, void *src, size_t len)
{
    int i;
    size_t len_align;
    if ((((unsigned long) dst & 0x3) == 0) && (((unsigned long) src & 0x03) == 0) ) {
        len_align = len & ~((unsigned long) 0x03);
        for(i = 0; i < len_align; i += 4)
            *((u32*) ((u32)dst+i)) = *((u32*)((u32)src+i));
        for(; i < len; i ++)
            *((u8*) ((u32)dst+i)) = *((u8*)((u32)src+i));
        return len;
    } else if ((((unsigned long) dst & 0x1) == 0) && (((unsigned long) src & 0x01) == 0)) {
        len_align = len & ~((unsigned long) 0x01);
        for(i = 0; i < len_align; i += 2)
            *((u16*) ((u32)dst+i)) = *((u16*)((u32)src+i));
        for(; i < len; i ++)
            *((u8*) ((u32)dst+i)) = *((u8*)((u32)src+i));
    } else {
        for(i=0; i < len; i ++)
            *((u8*) ((u32)dst+i)) = *((u8*)((u32)src+i));
    }
    return len;
}

static size_t rtk_flash_write(struct rtk_spi *rtks, void *dst, void *src, size_t len)
{
    int retval=0;
    u32 WRSIZE = 128;
    int i=0;
    unsigned int next_len;
    unsigned int cnt;

/*    WRSIZE =128;
    if (len%128 == 0)
        WRSIZE = 128;
    else if (len%4 == 0)
        WRSIZE = 4;
*/
    cnt = len/WRSIZE;
    if ((len%WRSIZE) != 0)
        cnt += 1;
    next_len = WRSIZE;
    do {
        if ((cnt ==1) && ((len%WRSIZE)!=0))
            next_len = len%WRSIZE;
        Flash_write(rtks, next_len, (u32)((unsigned char *)((unsigned long)dst & 0x00FFFFFF)+i*WRSIZE), (u32 *)((unsigned char *)src +i*WRSIZE));
        i++;
    } while(--cnt);
    retval = (i-1)*WRSIZE + next_len;

    return retval;
}

#endif