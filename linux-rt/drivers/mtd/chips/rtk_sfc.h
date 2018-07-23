#ifndef RTK_SERIAL_FLASH_CONTROLLER
#define RTK_SERIAL_FLASH_CONTROLLER
/*
   for HWSD, the serial flash address started from 0x88100000,
   so for 16Mbit Serial Flash, the address ranges from 0x8810-0000 - 0x890f-ffff
 */
/* 2 MBytes */

#ifdef CONFIG_ARCH_RTD129X
#define FLASH_SIZE                  (0x02000000)
#else
#define FLASH_SIZE                  (0x01000000)
#endif

#ifdef CONFIG_ARCH_RTD129X
#define RBUS_OFFSET                 (0x80000000)
#else
#define RBUS_OFFSET                 (0x00000000)
#endif

#define RBUS_BASE_PHYS              (0x18000000) + RBUS_OFFSET
#define RBUS_SIZE                   (0x20000)
#ifdef CONFIG_ARCH_RTD129X
#define NOR_BASE_PHYS               (0x88100000)
#else
#define NOR_BASE_PHYS               (0x18100000)
#endif

#define GET_MAPPED_RBUS_ADDR(reg)   ((unsigned long)rbus_base_virt + (reg - RBUS_BASE_PHYS))

#define REG_READ_U8(register)                   (*(volatile unsigned char *)GET_MAPPED_RBUS_ADDR(register))
#define REG_READ_U16(register)                  (*(volatile unsigned short *)GET_MAPPED_RBUS_ADDR(register))
#define REG_READ_U32(register)                  (*(volatile unsigned int *)GET_MAPPED_RBUS_ADDR(register))

#define REG_WRITE_U8(value, register)           (*(volatile unsigned char *)GET_MAPPED_RBUS_ADDR(register) = value)
#define REG_WRITE_U16(value, register)          (*(volatile unsigned short *)GET_MAPPED_RBUS_ADDR(register) = value)
#define REG_WRITE_U32(value, register)          (*(volatile unsigned int *)GET_MAPPED_RBUS_ADDR(register) = value)


#define MD_PP_DATA_SIZE_SHIFT   8   /* 256 bytes */
#define MD_PP_DATA_SIZE         (1 << MD_PP_DATA_SIZE_SHIFT)

#define SFC_OPCODE          (0x1801a800) + RBUS_OFFSET
#define SFC_CTL             (0x1801a804) + RBUS_OFFSET
#define SFC_SCK             (0x1801a808) + RBUS_OFFSET
#define SFC_CE              (0x1801a80c) + RBUS_OFFSET
#define SFC_WP              (0x1801a810) + RBUS_OFFSET
#define SFC_POS_LATCH       (0x1801a814) + RBUS_OFFSET

#define SFC_WAIT_WR         (0x1801a818) + RBUS_OFFSET
#define SFC_EN_WR           (0x1801a81c) + RBUS_OFFSET
#define SFC_FAST_RD         (0x1801a820) + RBUS_OFFSET
#define SFC_SCK_TAP         (0x1801a824) + RBUS_OFFSET
#define SFP_OPCODE2         (0x1801a828) + RBUS_OFFSET

// System Bridge II
#define SB2_WRAPPER_CTRL    (0x1801a018) + RBUS_OFFSET
#ifdef CONFIG_ARCH_RTD129X
#define SYS_MUXPAD5         (0x9801a914)
#else
#define SYS_MUXPAD5         (0x18000374)
#endif


// Move Data Engine
#define MD_FDMA_DDR_SADDR   (0x1800b088) + RBUS_OFFSET
#define MD_FDMA_FL_SADDR    (0x1800b08c) + RBUS_OFFSET
#define MD_FDMA_CTRL2       (0x1800b090) + RBUS_OFFSET
#define MD_FDMA_CTRL1       (0x1800b094) + RBUS_OFFSET

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


#define RTK_SFC_ATTR_NONE                       0x00
#define RTK_SFC_ATTR_SUPPORT_MD_PP              0x01
#define RTK_SFC_ATTR_SUPPORT_DUAL_IO            0x02
#define RTK_SFC_ATTR_SUPPORT_DUAL_O             0x04
#define RTK_SFC_ATTR_SUPPORT_4BYTE_ADDR_MODE    0x80

#define RTK_SFC_SMALL_PAGE_WRITE_MASK   0x3

typedef enum rtk_sfc_read_mode {
    eREAD_MODE_NORMAL_READ,
    eREAD_MODE_SINGLE_FAST_READ,    
    eREAD_MODE_DUAL_FAST_READ
        //eREAD_MODE_1I1O_FAST_READ,
        //eREAD_MODE_1I2O_FAST_READ,
        //eREAD_MODE_2IO_FAST_READ  
} rtk_sfc_read_mode_t; 

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


#define ENDIAN_SWAP_U16(data)\
    (data>>8) | (data<<8)

#define ENDIAN_SWAP_U32(data)\
    (data>>24) |\
((data<<8) & 0x00FF0000) |\
((data>>8) & 0x0000FF00) |\
(data<<24)

#define ENDIAN_SWAP_U64(data)\
    (data>>56) |\
((data<<40) & 0x00FF000000000000) |\
((data<<24) & 0x0000FF0000000000) |\
((data<<8)  & 0x000000FF00000000) |\
((data>>8)  & 0x00000000FF000000) |\
((data>>24) & 0x0000000000FF0000) |\
((data>>40) & 0x000000000000FF00) |\
(data<<56)

#define SFC_4KB_ERASE \
{\
    printk("SFC_4KB_ERASE\n");\
    sfc_info->attr      |= RTK_SFC_ATTR_SUPPORT_MD_PP;\
    sfc_info->erase_size    = 0x1000;\
    sfc_info->erase_opcode  = 0x00000020;\
}

#define SFC_64KB_ERASE \
{\
    printk("SFC_64KB_ERASE\n");\
    sfc_info->attr      |= RTK_SFC_ATTR_SUPPORT_MD_PP;\
    sfc_info->erase_size    = 0x10000;\
    sfc_info->erase_opcode  = 0x000000d8;\
}

#define SFC_256KB_ERASE \
{\
    printk("SFC_256KB_ERASE\n");\
    sfc_info->attr      |= RTK_SFC_ATTR_SUPPORT_MD_PP;\
    sfc_info->erase_size    = 0x40000;\
    sfc_info->erase_opcode  = 0x000000d8;\
}

#ifdef CONFIG_ARCH_RTD129X
#define SFC_SYNC \
    do {                    \
        asm volatile("DMB SY" : : : "memory"); \
        REG_WRITE_U32(0x00, 0x9801a020);    \
        asm volatile("DMB SY" : : : "memory"); \
    }while(0)
#else
#define SFC_SYNC \
    do {					\
        asm volatile("DMB"); 		\
        REG_WRITE_U32(0x00, 0x1801a020); 	\
        asm volatile("DMB"); 		\
    }while(0)
#endif


#define SYS_REG_TRY_LOCK(delay) 
#define SYS_REG_TRY_UNLOCK  
#define SFC_FLUSH_CACHE(buff, len)
#endif
