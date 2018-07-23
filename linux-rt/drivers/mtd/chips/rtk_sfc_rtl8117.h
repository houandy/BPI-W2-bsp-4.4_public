#ifndef __LINUX_RTK_SFC_DASH_H
#define __LINUX_RTK_SFC_DASH_H
#define RDID_MANUFACTURER_ID_MASK   0x000000FF
#define RDID_DEVICE_ID_1_MASK       0x0000FF00
#define RDID_DEVICE_ID_2_MASK       0x00FF0000
#define RDID_DEVICE_EID_1_MASK      0x000000FF
#define RDID_DEVICE_EID_2_MASK      0x0000FF00

#define RDID_MANUFACTURER_ID(id)    (id & RDID_MANUFACTURER_ID_MASK)
#define RDID_DEVICE_ID_1(id)        ((id & RDID_DEVICE_ID_1_MASK) >> 8)
#define RDID_DEVICE_ID_2(id)        ((id & RDID_DEVICE_ID_2_MASK) >> 16)
#define RDID_DEVICE_EID_1(id)       (id & RDID_DEVICE_EID_1_MASK)
#define RDID_DEVICE_EID_2(id)       ((id & RDID_DEVICE_EID_2_MASK) >> 8)

#define RTK_SFC_ATTR_NONE                       0x00
#define RTK_SFC_ATTR_SUPPORT_MD_PP              0x01
#define RTK_SFC_ATTR_SUPPORT_DUAL_IO            0x02
#define RTK_SFC_ATTR_SUPPORT_DUAL_O             0x04
#define RTK_SFC_ATTR_SUPPORT_4BYTE_ADDR_MODE    0x80

#define SFC_4KB_ERASE \
{\
    sfc_info->attr      |= RTK_SFC_ATTR_SUPPORT_MD_PP;\
    sfc_info->erase_size    = 0x1000;\
    sfc_info->erase_opcode  = 0x00000020;\
}

#define SFC_64KB_ERASE \
{\
    sfc_info->attr      |= RTK_SFC_ATTR_SUPPORT_MD_PP;\
    sfc_info->erase_size    = 0x10000;\
    sfc_info->erase_opcode  = 0x000000d8;\
}

#define SFC_256KB_ERASE \
{\
    sfc_info->attr      |= RTK_SFC_ATTR_SUPPORT_MD_PP;\
    sfc_info->erase_size    = 0x40000;\
    sfc_info->erase_opcode  = 0x000000d8;\
}

#define SUPPORTED       1
#define NOT_SUPPORTED   0

typedef struct rtk_sfc_info {
    //struct semaphore rtk_sfc_lock;
    u8 manufacturer_id;
    u8 device_id1;
    u8 device_id2;
    u8 attr;
    u32 erase_size;
    u32 erase_opcode;
    u8 sec_256k_en; //256KB size erase support
    u8 sec_64k_en; //64KB size erase support
    u8 sec_32k_en; //32KB size erase support
    u8 sec_4k_en; //4KB size erase support
    struct mtd_info *mtd_info;
}rtk_sfc_info_t;

#define MANUFACTURER_ID_SPANSION    0x01
#define MANUFACTURER_ID_STM         0x20
#define MANUFACTURER_ID_PMC         0x7f
#define MANUFACTURER_ID_SST         0xbf
#define MANUFACTURER_ID_MXIC        0xc2
#define MANUFACTURER_ID_EON         0x1c
#define MANUFACTURER_ID_ATMEL       0x1f
#define MANUFACTURER_ID_WINBOND     0xef //add by alexchang
#define MANUFACTURER_ID_ESMT        0x8c //add by alexchang
#define MANUFACTURER_ID_GD          0xc8 //add by alexchang
#endif