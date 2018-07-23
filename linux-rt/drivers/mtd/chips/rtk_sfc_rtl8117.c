#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mtd/physmap.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/partitions.h>
//#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/pm.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <mtd/mtd-abi.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/platform_device.h>
//#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include "../mtdcore.h"
#include "rtk_sfc_rtl8117.h"
#include <linux/spi/spi-rtk.h>

static struct rtk_spi sfc;
static struct mtd_info descriptor;
static rtk_sfc_info_t rtk_sfc_info;

static unsigned int SPI_FLASH_SIZE=0x0;
static unsigned char * FLASH_BASE=NULL;

/*--------------------------------------------------------------------------------
  MXIC serial flash information list
  [MXIC MX25L4005]
  erase size: 4KB / 64KB

  [MXIC MX25L8005 / MX25L8006E]
  erase size: 4KB / 64KB

  [MXIC MX25L1605]
  erase size: 4KB / 64KB

  [MXIC MX25L3205]
  erase size: 4KB / 64KB

  [MXIC MX25L6405D]
  erase size: 4KB / 64KB


  [MXIC MX25L6445E]
  erase size: 4KB / 32KB / 64KB

  [MXIC MX25L12845E]
  erase size: 4KB / 32KB / 64KB

  [MXIC MX25L12805E]
  erase size: 4KB / 64KB

  [MXIC MX25L25635E]
  erase size: 4KB / 32KB / 64KB

  [MXIC MX25L6455E]
  erase size:  4KB / 32KB / 64KB

  [MXIC MX25L12855E]
  erase size: 4KB / 32KB / 64KB

  --------------------------------------------------------------------------------*/

static int mxic_init(rtk_sfc_info_t *sfc_info)
{
    unsigned char manufacturer_id, device_id;

    switch(sfc_info->device_id1)
    {
    case 0x20:
        switch(sfc_info->device_id2)
        {
        case 0x13:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L4005 detected.\n");
            SFC_4KB_ERASE;
            sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            sfc_info->mtd_info->size = 0x80000;
            break;
        case 0x14:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L8005/MX25L8006E detected.\n");
            SFC_4KB_ERASE;
            sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            sfc_info->mtd_info->size = 0x100000;
            break;
        case 0x15:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L1605 detected.\n");
            SFC_4KB_ERASE;
            sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            sfc_info->mtd_info->size = 0x200000;
            break;
        case 0x16:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L3205 detected.\n");
            SFC_4KB_ERASE;
            sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_O;
            sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            sfc_info->mtd_info->size = 0x400000;
            break;
        case 0x17:
            manufacturer_id = (sfc.flash_id & 0x00FF0000)>>16;
            device_id = (sfc.flash_id & 0x0000FF00)>>8;
            if(manufacturer_id == 0xc2 && device_id == 0x16)
            {
                printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L6445E detected....\n");
                SFC_4KB_ERASE;
                //sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO;
                sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
                sfc_info->sec_256k_en = NOT_SUPPORTED;
            }
            else
            {
                printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L6405D detected.\n");
                SFC_4KB_ERASE;
                sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
                sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            }
            sfc_info->mtd_info->size = 0x800000;
            break;
        case 0x18:
            manufacturer_id = (sfc.flash_id & 0x00FF0000)>>16;
            device_id = (sfc.flash_id & 0x0000FF00)>>8;
            if(manufacturer_id == 0xc2 && device_id == 0x17) {
                printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L12845E detected.\n");
                printk("not define CONFIG_MTD_UBI.\n");
                SFC_4KB_ERASE;
                sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO|RTK_SFC_ATTR_SUPPORT_DUAL_O;
                sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
                sfc_info->sec_256k_en = NOT_SUPPORTED;
            }
            else {
                printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L12805 detected.\n");
                SFC_4KB_ERASE;
                sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
                sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            }
            sfc_info->mtd_info->size = 0x1000000;
            break;
        case 0x19:
            manufacturer_id = (sfc.flash_id & 0x00FF0000)>>16;
            device_id = (sfc.flash_id & 0x0000FF00)>>8;
            if(manufacturer_id == 0xc2 && device_id == 0x18) {
                printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L25635E detected.\n");
                SFC_4KB_ERASE;
                sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO;
                sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
                sfc_info->sec_256k_en = NOT_SUPPORTED;
                sfc_info->mtd_info->size = 0x2000000;
            }
            else {
                printk(KERN_NOTICE "RtkSFC MTD: MXIC unknown mnftr_id=0x%x, dev_id=0x%x .\n", manufacturer_id, device_id) ;
                SFC_4KB_ERASE;
                sfc_info->sec_4k_en = SUPPORTED;
                sfc_info->sec_256k_en = sfc_info->sec_64k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            }
            break;
        default:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC unknown id2=0x%x detected.\n",
                   sfc_info->device_id2);
            SFC_4KB_ERASE;
            sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = sfc_info->sec_64k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
            break;
        }
        break;
    case 0x26:////add by alexchang 1206-2010
        switch(sfc_info->device_id2)
        {
        case 0x17:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L6455E detected.\n");
            SFC_4KB_ERASE;
            sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = NOT_SUPPORTED;
            sfc_info->mtd_info->size = 0x800000;
            break;

        case 0x18:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC MX25L12855E detected.\n");
            SFC_4KB_ERASE;
            sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
            sfc_info->sec_256k_en = NOT_SUPPORTED;
            sfc_info->mtd_info->size = 0x1000000;
            break;

        default:
            printk(KERN_NOTICE "RtkSFC MTD: MXIC unknown id2=0x%x detected.\n",
                   sfc_info->device_id2);
            break;
        }
        break;
    default:
        printk(KERN_NOTICE "RtkSFC MTD: MXIC unknown id1=0x%x detected.\n",
               sfc_info->device_id1);
        break;
    }

    if (sfc_info->erase_opcode==0xFFFFFFFF)//Set to default.
    {
        SFC_64KB_ERASE;
    }

    return 0;
}

/*--------------------------------------------------------------------------------
  WINBOND serial flash information list
  [WINBOND 25Q128BVFG]
  erase size:

  [WINBOND W25Q32BV]32 Mbits
  erase size:4KB /32KB /64KB

  [SPANSION S25FL064K ] 64Mbits //SPANSION brand, Winbond OEM
  erase size: 4KB / 32KB / 64KB

  --------------------------------------------------------------------------------*/
static int winbond_init(rtk_sfc_info_t *sfc_info) {
    switch(sfc_info->device_id1) {
        case 0x40:
            switch(sfc_info->device_id2) {
                case 0x14:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND W25Q80BV detected.\n");
                    SFC_4KB_ERASE;
                    sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO|RTK_SFC_ATTR_SUPPORT_DUAL_O;
                    sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
                    sfc_info->sec_256k_en = NOT_SUPPORTED;
                    sfc_info->mtd_info->size = 0x100000;
                    break;
                case 0x19:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND S25FL256K detected.\n");
                    SFC_4KB_ERASE;
                    sfc_info->sec_4k_en = sfc_info->sec_32k_en = sfc_info->sec_64k_en = SUPPORTED;
                    sfc_info->sec_256k_en = NOT_SUPPORTED;
                    sfc_info->mtd_info->size = 0x2000000;
                    //louis
#if 0
                    FLASH_BASE = (unsigned char*)0xbdc00000;
                    FLASH_POLL_ADDR = (unsigned char*)0xbec00000;
#endif
                    break;
                case 0x16:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND W25Q32BV(W25Q32FV) detected.\n");
                    SFC_4KB_ERASE;
                    sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO|RTK_SFC_ATTR_SUPPORT_DUAL_O;
                    sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
                    sfc_info->sec_256k_en = NOT_SUPPORTED;
                    sfc_info->mtd_info->size = 0x400000;
                    break;
                case 0x17:
                    printk(KERN_NOTICE "RtkSFC MTD: SPANSION S25FL064K detected.\n");
                    SFC_4KB_ERASE;
                    sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO|RTK_SFC_ATTR_SUPPORT_DUAL_O;
                    sfc_info->sec_4k_en = sfc_info->sec_32k_en = sfc_info->sec_64k_en = SUPPORTED;
                    sfc_info->sec_256k_en = NOT_SUPPORTED;
                    sfc_info->mtd_info->size = 0x800000;
                    break;
                case 0x18:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND 25Q128BVFG(W25Q128BVFIG) detected.\n");
                    SFC_4KB_ERASE;
                    sfc_info->attr |= RTK_SFC_ATTR_SUPPORT_DUAL_IO|RTK_SFC_ATTR_SUPPORT_DUAL_O;
                    sfc_info->sec_64k_en = sfc_info->sec_4k_en = SUPPORTED;
                    sfc_info->sec_256k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;
                    sfc_info->mtd_info->size = 0x1000000;
                    break;
                default:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND unknown id2=0x%x detected.\n",
                            sfc_info->device_id2);
                    SFC_4KB_ERASE;
                    sfc_info->sec_4k_en = SUPPORTED;
                    sfc_info->sec_256k_en = sfc_info->sec_64k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;

                    break;
            }
            return 0;
        case 0x60:
            switch(sfc_info->device_id2) {
                case 0x16:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND W25Q32FV(Quad Mode) detected.\n");
                    SFC_4KB_ERASE;
                    sfc_info->sec_64k_en = sfc_info->sec_32k_en = sfc_info->sec_4k_en = SUPPORTED;
                    sfc_info->sec_256k_en = NOT_SUPPORTED;
                    sfc_info->mtd_info->size = 0x400000;
                    break;
                default:
                    printk(KERN_NOTICE "RtkSFC MTD: WINBOND unknown id2=0x%x detected.\n",
                            sfc_info->device_id2);
                    SFC_4KB_ERASE;
                    sfc_info->sec_4k_en = SUPPORTED;
                    sfc_info->sec_256k_en = sfc_info->sec_64k_en = sfc_info->sec_32k_en = NOT_SUPPORTED;

                    break;
            }
            return 0;
        default:
            printk(KERN_NOTICE "RtkSFC MTD: WINBOND unknown id1=0x%x detected.\n",
                    sfc_info->device_id1);
            break;
    }
    SFC_4KB_ERASE;
    return 0;
}


static int rtk_sfc_read(struct mtd_info *mtd, loff_t from, size_t len,
                        size_t *retlen, u_char *buf)
{
    rtk_sfc_info_t *sfc_info;

    if((sfc_info = (rtk_sfc_info_t*)mtd->priv) == NULL)
        return -EINVAL;
    if(!buf)
        return -EINVAL;

    *retlen = rtk_memcpy((void *)buf, (void *)(FLASH_BASE + from), len);

    return 0;
}

static int rtk_sfc_write(struct mtd_info *mtd, loff_t to, size_t len,
                         size_t *retlen, const u_char *buf)
{
    if(!buf)
        return -EINVAL;
    *retlen = rtk_flash_write(&sfc, (void *)(FLASH_BASE + to), (void *)buf, len);
    return 0;
}

static int rtk_sfc_erase(struct mtd_info *mtd, struct erase_info *instr)
{
    rtk_sfc_info_t *sfc_info;
    unsigned int size;
    volatile unsigned char *addr;
    unsigned int erase_addr;
    unsigned int erase_opcode;
    unsigned int erase_size;

    if((sfc_info = (rtk_sfc_info_t*)(mtd->priv)) == NULL)
        return -EINVAL;
    if(instr->addr + instr->len > mtd->size)
        return -EINVAL;

    addr = FLASH_BASE + (instr->addr);
    size = instr->len;
    erase_addr = (unsigned int)instr->addr;
    erase_opcode = sfc_info->erase_opcode;
    erase_size = mtd->erasesize;

    for(size = instr->len ; size > 0 ; size -= erase_size)
    {
        /* choose erase sector size */
        if (((erase_addr&(0x10000-1)) == 0) && (size >= 0x10000) && (sfc_info->sec_64k_en == SUPPORTED))
        {
            erase_opcode = FLASH_BE_COM;
            erase_size = 0x10000;
        }
        else if (((erase_addr&(0x1000-1)) == 0) && (size >= 0x1000) && (sfc_info->sec_4k_en == SUPPORTED))
        {
            erase_opcode = FLASH_CHIP_SEC;
            erase_size = 0x1000;
        }
        else
        {
            erase_opcode = sfc_info->erase_opcode;
            erase_size = mtd->erasesize;
        }
        Flash_erase(&sfc, (u32)addr & 0x00FFFFFF, erase_opcode);
        addr += erase_size;
        erase_addr += erase_size;
    }

    instr->state = MTD_ERASE_DONE;
    mtd_erase_callback(instr);

    return 0;
}

#if defined(CONFIG_MTD_UBI) || defined(CONFIG_JFFS2_FS)
//fixbug:UBI can accept leb size > 15Kbytes.
static void rtk_sfc_adjust_erase_size(struct mtd_info *mtd)
{
    rtk_sfc_info_t *sfc_info;
    if((sfc_info = (rtk_sfc_info_t*)(mtd->priv)) == NULL)
    {
        printk(KERN_ERR "mtd->priv is NULL.\n");
        return;
    }
    if (sfc_info->erase_size == 0x1000)
    {
        if (sfc_info->sec_64k_en == SUPPORTED)
        {
            SFC_64KB_ERASE;
        }
        else if (sfc_info->sec_256k_en == SUPPORTED)
        {
            SFC_256KB_ERASE;
        }
    }
}
#endif
static const char * const part_probes[] = { "cmdlinepart", "ofpart", NULL };
static int rtk_sfc_mtd_attach(struct mtd_info *mtd_info)
{
    int nr_parts = 0;
    struct mtd_partition *parts;

    /*
     * Partition selection stuff.
     */
#ifdef CONFIG_MTD_CMDLINE_PARTS
    nr_parts = parse_mtd_partitions(&descriptor, part_probes, &parts, 0);
#endif

    if(nr_parts <= 0) {
        if(add_mtd_device(&descriptor)) {
            printk(KERN_WARNING "Rtk SFC: (for SST/SPANSION/MXIC/WINBOND SPI-Flash) Failed to register new device\n");
            return -EAGAIN;
        }
    }
    else
        add_mtd_partitions(&descriptor, parts, nr_parts);

    printk(KERN_INFO "Rtk SFC: (for SST/SPANSION/MXIC SPI Flash)\n");
    return 0;
}

static int rtk_sfc_probe(struct platform_device *pdev)
{
    struct mtd_info *mtd_info;
    rtk_sfc_info_t *sfc_info;
    int ret = 0;

    mtd_info = (struct mtd_info*)&descriptor;
    if((sfc_info = (rtk_sfc_info_t*)mtd_info->priv) == NULL)
        return -ENODEV;
    //get RDID
    Flash_RDID(&sfc);
    sfc_info->manufacturer_id = (sfc.flash_id & 0x00FF0000)>>16;
    sfc_info->device_id2 = RDID_DEVICE_EID_1(sfc.flash_id);
    sfc_info->device_id1 = RDID_DEVICE_EID_2(sfc.flash_id);
    printk(KERN_INFO "--RDID Seq: 0x%x | 0x%x | 0x%x\n",sfc_info->manufacturer_id,sfc_info->device_id1,sfc_info->device_id2);
    //can add other flashes.
    switch(sfc_info->manufacturer_id) {
    case MANUFACTURER_ID_MXIC:
        ret = mxic_init(sfc_info);
        break;
    case MANUFACTURER_ID_WINBOND:
	ret = winbond_init(sfc_info);
	break;
    default:
        printk(KERN_ERR "RtkSFC MTD: Unknown flash type.\n");
        printk(KERN_ERR "Manufacturer's ID = %02X.\n", sfc_info->manufacturer_id);
        return -ENODEV;
        break;
    }

#if defined(CONFIG_MTD_UBI) || defined(CONFIG_JFFS2_FS)
    rtk_sfc_adjust_erase_size(mtd_info);
#endif

    mtd_info->erasesize = sfc_info->erase_size;
    mtd_info->writesize = 0x100;
    mtd_info->writebufsize = 0x100;

    printk(KERN_INFO "Supported Erase Size:%s%s%s%s.\n"
           , sfc_info->sec_256k_en ? " 256KB" : ""
           , sfc_info->sec_64k_en ? " 64KB" : ""
           , sfc_info->sec_32k_en ?" 32KB" : ""
           , sfc_info->sec_4k_en ? " 4KB" : ""
          );

    if((ret = rtk_sfc_mtd_attach(&descriptor)) != 0) {
        printk(KERN_ERR "[%s]Realtek SFC attach fail\n",__FUNCTION__);
        return ret;
    }
    return 0;
}

static struct resource physmap_flash_resource[1];
static struct platform_device *rtkSFC_device;
static struct platform_driver rtkSFC_driver = {
    .probe      = rtk_sfc_probe,
    .driver     =
    {
        .name   = "RtkSFC",
        .owner  = THIS_MODULE,
    },
};

static int rtk_sfc_init_RtkSFC(void)
{
    struct mtd_info *mtd_info;
    rtk_sfc_info_t *sfc_info;
    int rc = 0;

    mtd_info = (struct mtd_info*)&descriptor;

    if((sfc_info = (rtk_sfc_info_t*)mtd_info->priv) == NULL)
        return -ENODEV;

    //register new platform device and platform driver.
    //when new platform device is registered, the probe function of the platform driver will de called.
    rc = platform_driver_register(&rtkSFC_driver);
    if (!rc) {
        rtkSFC_device = platform_device_alloc("RtkSFC", 0);
        physmap_flash_resource[0].start = (unsigned long)FLASH_BASE;
        physmap_flash_resource[0].end = physmap_flash_resource[0].start + SPI_FLASH_SIZE - 1;
        rtkSFC_device->num_resources = ARRAY_SIZE(physmap_flash_resource);
        rtkSFC_device->resource = physmap_flash_resource;

        if (rtkSFC_device) {
            rc = platform_device_add(rtkSFC_device);
            if ((sfc_info->manufacturer_id == 0xff)&&
                    (sfc_info->device_id2 == 0xff) &&
                    (sfc_info->device_id1 == 0xff))
                rc = -1;
        }
        else
            rc = -ENOMEM;
        if (rc < 0) {
            platform_device_put(rtkSFC_device);
            platform_driver_unregister(&rtkSFC_driver);
        }
    }
    if (rc < 0) {
        printk(KERN_ERR "Realtek SFC Driver installation fails.\n\n");
        return -ENODEV;
    } else
        printk(KERN_INFO "Realtek SFC Driver is successfully installing.\n\n");
    return rc;
}

static int __init rtk_sfc_init(void)
{
    const u32 *prop;
    int size;
    struct device_node *np;

    printk(KERN_INFO "RtkSFC MTD init ...\n");

    np = of_find_node_by_name(NULL, "rtk_spi");
    prop = of_get_property(np, "reg", &size);
    if (prop)
    {
        sfc.base = ioremap(of_read_number(prop, 1), of_read_number(prop+1, 1));
        //printk(KERN_INFO "[%s] get spi controller base addr : 0x%x \n",__func__, (unsigned int)sfc.base);
    } else
    {
        printk(KERN_ERR "[%s] get spi controller base addr error !!\n",__func__);
    }

    np = of_find_node_by_name(NULL, "spi-flash");
    prop = of_get_property(np, "memory-map", &size);
    FLASH_BASE = of_read_number(prop, 1);
    SPI_FLASH_SIZE = of_read_number(prop+1, 1);


    /* prepare mtd_info */
    memset(&descriptor, 0, sizeof(struct mtd_info));
    memset(&rtk_sfc_info, 0, sizeof(rtk_sfc_info_t));
    rtk_sfc_info.mtd_info = &descriptor;

    descriptor.priv = &rtk_sfc_info;
    descriptor.name = "RtkSFC";
    descriptor.size = SPI_FLASH_SIZE;
    descriptor.flags = MTD_WRITEABLE;
    descriptor._erase = rtk_sfc_erase;
    descriptor._read = rtk_sfc_read;
    descriptor._write = rtk_sfc_write;
    descriptor.owner = THIS_MODULE;
    descriptor.type = MTD_DATAFLASH;//MTD_DATAFLASH for general serial flash
    descriptor.numeraseregions = 0;
    descriptor.oobsize = 0;
    rtk_sfc_init_RtkSFC();
    return 0;
}


static void __exit rtk_sfc_exit(void)
{
    del_mtd_device(&descriptor);
}

module_init(rtk_sfc_init);
module_exit(rtk_sfc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chih-pin Wu <wucp@realtek.com.tw>");
MODULE_DESCRIPTION("MTD chip driver for Realtek Rtk Serial Flash Controller");
