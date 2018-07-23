/*
 * r8169.c: RealTek 8169/8168/8101 ethernet driver.
 *
 * Copyright (c) 2002 ShuChen <shuchen@realtek.com.tw>
 * Copyright (c) 2003 - 2007 Francois Romieu <romieu@fr.zoreil.com>
 * Copyright (c) a lot of people too. Please respect their work.
 *
 * See MAINTAINERS file for support contact information.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_net.h>
#include <linux/of_address.h>
#include <linux/decompress/mm.h>

#include <asm/io.h>
#include <asm/irq.h>

/* Yukuen: Use kthread to watch link status change. 20150206 */
#include <linux/kthread.h>
#include <linux/suspend.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/reset-helper.h>
#include <linux/reset.h>

#include <rtl8117_platform.h>

#define RTL8169_VERSION "2.7LK-NAPI"
#define MODULENAME "r8168oob"
#define PFX MODULENAME ": "

#ifndef __HWPF_H__
#define __HWPF_H__

#define UNICAST_MAR	1
#define MULCAST_MAR	2
#define BROADCAST_MAR	4

#define MAC_ADDR_LEN    6

#define IPv4 4
#define IPv6 6

#define IP_PROTO_ICMP 0x01
#define IP_PROTO_ICMPv6 0x3A
#define IP_PROTO_UDP 0x11
#define IP_PROTO_TCP 0x06

#define IPv4_ADR_LEN 4
#define IPv6_ADR_LEN 16
#define PORT_ADR_LEN 2
#define TYPE_ADR_LEN 2
#define PTL_ADR_LEN 1

typedef struct {
 unsigned char ruleNO;
 unsigned char isSet:1, FRIBNotDrop:1, FROOB:1, IBMAR:1, OOBMAR:1;
	unsigned short RSV1;

 unsigned short MARType;
 unsigned short MACIDBitMap; /*MACID0: IB, MACID1: IB  */
 unsigned short VLANBitMap;
 unsigned short TypeBitMap; /* Type0: IPv4, Type1: IPv6, Type2:ARP, Type3:VLAN */
 unsigned short IPv4PTLBitMap; /* IPv4PTL:TCP, IPv4PTL1:UDP, IPv4PTL2:ICMP, IPv4PTL3:AH, IPv4PTL4:ESP, IPv4PTL5:IPV4-IPv6 */
 unsigned short IPv6PTLBitMap;
 unsigned short DIPv4BitMap;
 unsigned short DIPv6BitMap;
 unsigned int	DPortBitMap;
} TCAMRule;


/*TCAM Entry Setting*/
typedef struct {
	/*when read TCAM data to this variable must know if "DontCareBit" not equal to 0xFFFF, the data just one kind of combination*/
	u16 Value;
	/*If don't care bit exist set "1" to this bit position*/
	/*i.e. 0xFFFF=0000 0000 0000 0000->no dont care bit; 0x0007=0000 0000 0000 0111->BIT1,BIT2,BIT3 is don't care bit*/
	u16 DontCareBit;
	u8 Valid;
} TCAM_Entry_Setting_st;

/*TCAM Property*/
typedef enum{
	TCAM_MAC,
	TCAM_VLAN,
	TCAM_TYPE,
	TCAM_PTLv4,
	TCAM_PTLv6,
	TCAM_SIPv4,
	TCAM_DIPv4,
	TCAM_SIPv6,
	TCAM_DIPv6,
	TCAM_SPORT,
	TCAM_DPORT,
	TCAM_Teredo_SPORT,
	TCAM_Teredo_DPORT,
	TCAM_UDP_ESP_SPORT,
	TCAM_UDP_ESP_DPORT,
	TCAM_OFFSET,
	MAX_TCAM_Entry_Type,
	/*----------------------*/

	TCAM_MARIB,
	TCAM_MAROOB,
} TCAM_Entry_Type_et;

typedef struct {
	u16 Start_In_TCAM[MAX_TCAM_Entry_Type];
	u16 Number_Of_Set[MAX_TCAM_Entry_Type];
	u16 Entry_Per_Set[MAX_TCAM_Entry_Type];
} TCAM_Property_st;

typedef struct {
   TCAM_Entry_Type_et type;
	unsigned int num;
	unsigned char *data;
} TCAM_Allocation;


/*TCAM Rule Action*/
typedef struct {
	u8 number;
	u8 Gamming:1;
	u8 Meter_En:1;
	u8 Meter_No:3;
	u8 Drop:1;
	u8 OOB:1;
	u8 IB:1;
} TCAM_RuleActionSet_st;

typedef enum {
	TCAM_Read = 0,
	TCAM_Write,
	TCAM_RW_MAX,
} TCAM_RW_et;

typedef enum {
	RET_FAIL,
	RET_OK,
} Ret_Code_et;

typedef enum {
	Switch_OFF,
	Switch_ON,
} ONOFF_Switch_et;

typedef enum {
	TCAM_data,
	TCAM_care,
} TCAM_ReadType_et;

typedef enum {
	RULE_NO_0,
	RULE_NO_1,
	RULE_NO_2,
	RULE_NO_3,
	RULE_NO_4,
	RULE_NO_5,
	RULE_NO_6,
	RULE_NO_7,
	RULE_NO_8,
	RULE_NO_9,
	RULE_NO_10,
	RULE_NO_11,
	RULE_NO_12,
	RULE_NO_13,
	RULE_NO_14,
	RULE_NO_15,
	RULE_NO_MAX,
	RULE_SETTING_FAIL,
} Rule_NO_et;
typedef enum{
	MAC_0,/*0*/
	MAC_1,
	MAC_2,
	MAC_3,
	MAC_4,
	MAC_5,
	MAC_6,
	MAC_7,
	MAC_8,
	MAC_9,/*9*/

	MARI,
	MARO,
	BRD,/*12*/

	VLAN_0,
	VLAN_1,
	VLAN_2,/*15*/
	VLAN_3,
	VLAN_4to5,
	VLAN_6to10,
	VLAN_11to15,

	TYPE_0,/*20*/  /*IPv4*/
	TYPE_1,		/*IPv6*/
	TYPE_2,		/*ARP*/
	TYPE_3,		/*VLAN*/
	TYPE_4,
	TYPE_5,
	TYPE_6,
	TYPE_7,
	TYPE_8to11,
	TYPE_12to15,

	PTL_0,/*30*/
	PTL_1,
	PTL_2,
	PTL_3,
	PTL_4,
	PTL_5,
	PTL_6,
	PTL_7,
	PTL_8,
	PTL_9,
	PTL_10,/*40*/
	PTL_11,

	SIP_0,
	SIP_1,
	SIP_2,
	SIP_3,

	DIP_0,
	DIP_1,
	DIP_2,
	DIP_3,
	DIP_4,/*50*/
	DIP_5,
	DIP_6,
	DIP_7,
	DIP_8,
	DIP_9,
	DIP_10,
	DIP_11,

	SPORT_0to4,
	SPORT_5to9,
	SPORT_10to14,/*60*/
	SPORT_15to19,
	SPORT_20to24,
	SPORT_25to29,
	SPORT_30to39,
	SPORT_40to49,
	SPORT_50to59,
	SPORT_60to69,

	DPORT_0to9,
	DPORT_10to19,
	DPORT_20to29,/*70*/
	DPORT_30to39,
	DPORT_40to49,
	DPORT_50to59,
	DPORT_60to69,
	DPORT_70to79,
	DPORT_80to89,
	DPORT_90to99,
	DPORT_100to109,
	DPORT_110to127,

	OFFSET_0to3,/*80*/
	OFFSET_4to7,
	OFFSET_8to11,
	OFFSET_12to15,
	OFFSET_16to19,
	OFFSET_20to23,
	OFFSET_24to27,
	OFFSET_28to31,
	OFFSET_32to35,
	OFFSET_36to39,
	OFFSET_40to43,/*90*/
	OFFSET_44to47,
	OFFSET_48to51,
	OFFSET_52to55,
	OFFSET_56to59,
	OFFSET_60to63,/*95*/

	MAX_RULE_NUMBER,
	OUT_OF_RANGE,
} RuleFormat_et;

#define DWBIT00 0x00000001
#define DWBIT01 0x00000002
#define DWBIT02 0x00000004
#define DWBIT03 0x00000008
#define DWBIT04 0x00000010
#define DWBIT05 0x00000020
#define DWBIT06 0x00000040
#define DWBIT07 0x00000080
#define DWBIT08 0x00000100
#define DWBIT09 0x00000200
#define DWBIT10 0x00000400
#define DWBIT11 0x00000800
#define DWBIT12 0x00001000
#define DWBIT13 0x00002000
#define DWBIT14 0x00004000
#define DWBIT15 0x00008000
#define DWBIT16 0x00010000
#define DWBIT17 0x00020000
#define DWBIT18 0x00040000
#define DWBIT19 0x00080000
#define DWBIT20 0x00100000
#define DWBIT21 0x00200000
#define DWBIT22 0x00400000
#define DWBIT23 0x00800000
#define DWBIT24 0x01000000
#define DWBIT25 0x02000000
#define DWBIT26 0x04000000
#define DWBIT27 0x08000000
#define DWBIT28 0x10000000
#define DWBIT29 0x20000000
#define DWBIT30 0x40000000
#define DWBIT31 0x80000000

/*------------------------------*/
/*				*/
/*		TCAM Entry No.	*/
/*				*/
/*------------------------------*/
#define TCAM_Entry_Number	512
/*MAC*/
#define TCAM_MAC_Start_In_TCAM				0
#define TCAM_MAC_Number_Of_Set				10
#define TCAM_MAC_Entry_Per_Set				3
#define TCAM_MAC_Total_Entry				(TCAM_MAC_Number_Of_Set*TCAM_MAC_Entry_Per_Set)
/*VLAN*/
#define TCAM_VLAN_Start_In_TCAM				(TCAM_MAC_Start_In_TCAM+TCAM_MAC_Total_Entry)
#define TCAM_VLAN_Number_Of_Set				16
#define TCAM_VLAN_Entry_Per_Set				1
#define TCAM_VLAN_Total_Entry				(TCAM_VLAN_Number_Of_Set*TCAM_VLAN_Entry_Per_Set)
/*TYPE*/
#define TCAM_TYPE_Start_In_TCAM				(TCAM_VLAN_Start_In_TCAM+TCAM_VLAN_Total_Entry)
#define TCAM_TYPE_Number_Of_Set				16
#define TCAM_TYPE_Entry_Per_Set				1
#define TCAM_TYPE_Total_Entry				(TCAM_TYPE_Number_Of_Set*TCAM_TYPE_Entry_Per_Set)
/*PTL IPv4*/
#define TCAM_PTLv4_Start_In_TCAM			(TCAM_TYPE_Start_In_TCAM+TCAM_TYPE_Total_Entry)
#define TCAM_PTLv4_Number_Of_Set			12
#define TCAM_PTLv4_Entry_Per_Set			1
#define TCAM_PTLv4_Total_Entry				(TCAM_PTLv4_Number_Of_Set*TCAM_PTLv4_Entry_Per_Set)
/*PTL IPv6*/
#define TCAM_PTLv6_Start_In_TCAM			(TCAM_PTLv4_Start_In_TCAM+TCAM_PTLv4_Total_Entry)
#define TCAM_PTLv6_Number_Of_Set			12
#define TCAM_PTLv6_Entry_Per_Set			1
#define TCAM_PTLv6_Total_Entry				(TCAM_PTLv6_Number_Of_Set*TCAM_PTLv6_Entry_Per_Set)
/*SIPv4*/
#define TCAM_SIPv4_Start_In_TCAM			(TCAM_PTLv6_Start_In_TCAM+TCAM_PTLv6_Total_Entry)
#define TCAM_SIPv4_Number_Of_Set			4
#define TCAM_SIPv4_Entry_Per_Set			2
#define TCAM_SIPv4_Total_Entry				(TCAM_SIPv4_Number_Of_Set*TCAM_SIPv4_Entry_Per_Set)
/*DIPv4*/
#define TCAM_DIPv4_Start_In_TCAM			(TCAM_SIPv4_Start_In_TCAM+TCAM_SIPv4_Total_Entry)
#define TCAM_DIPv4_Number_Of_Set			12
#define TCAM_DIPv4_Entry_Per_Set			2
#define TCAM_DIPv4_Total_Entry				(TCAM_DIPv4_Number_Of_Set*TCAM_DIPv4_Entry_Per_Set)
/*SIPv6*/
#define TCAM_SIPv6_Start_In_TCAM			(TCAM_DIPv4_Start_In_TCAM+TCAM_DIPv4_Total_Entry)
#define TCAM_SIPv6_Number_Of_Set			4
#define TCAM_SIPv6_Entry_Per_Set			8
#define TCAM_SIPv6_Total_Entry				(TCAM_SIPv6_Number_Of_Set*TCAM_SIPv6_Entry_Per_Set)
/*DIPv6*/
#define TCAM_DIPv6_Start_In_TCAM			(TCAM_SIPv6_Start_In_TCAM+TCAM_SIPv6_Total_Entry)
#define TCAM_DIPv6_Number_Of_Set			12
#define TCAM_DIPv6_Entry_Per_Set			8
#define TCAM_DIPv6_Total_Entry				(TCAM_DIPv6_Number_Of_Set*TCAM_DIPv6_Entry_Per_Set)
/*SPORT*/
#define TCAM_SPORT_Start_In_TCAM			(TCAM_DIPv6_Start_In_TCAM+TCAM_DIPv6_Total_Entry)
#define TCAM_SPORT_Number_Of_Set			70
#define TCAM_SPORT_Entry_Per_Set			1
#define TCAM_SPORT_Total_Entry				(TCAM_SPORT_Number_Of_Set*TCAM_SPORT_Entry_Per_Set)
/*DPORT*/
#define TCAM_DPORT_Start_In_TCAM			(TCAM_SPORT_Start_In_TCAM+TCAM_SPORT_Total_Entry)
#define TCAM_DPORT_Number_Of_Set			128
#define TCAM_DPORT_Entry_Per_Set			1
#define TCAM_DPORT_Total_Entry				(TCAM_DPORT_Number_Of_Set*TCAM_DPORT_Entry_Per_Set)
/*Teredo SPORT*/
#define TCAM_Teredo_SPORT_Start_In_TCAM			(TCAM_DPORT_Start_In_TCAM+TCAM_DPORT_Total_Entry)
#define TCAM_Teredo_SPORT_Number_Of_Set			1
#define TCAM_Teredo_SPORT_Entry_Per_Set			1
#define TCAM_Teredo_SPORT_Total_Entry			(TCAM_Teredo_SPORT_Number_Of_Set*TCAM_Teredo_SPORT_Entry_Per_Set)
/*Teredo DPORT*/
#define TCAM_Teredo_DPORT_Start_In_TCAM			(TCAM_Teredo_SPORT_Start_In_TCAM+TCAM_Teredo_SPORT_Total_Entry)
#define TCAM_Teredo_DPORT_Number_Of_Set			1
#define TCAM_Teredo_DPORT_Entry_Per_Set			1
#define TCAM_Teredo_DPORT_Total_Entry			(TCAM_Teredo_DPORT_Number_Of_Set*TCAM_Teredo_DPORT_Entry_Per_Set)
/*UDP_ESP SPORT*/
#define TCAM_UDP_ESP_SPORT_Start_In_TCAM		(TCAM_Teredo_DPORT_Start_In_TCAM+TCAM_Teredo_DPORT_Total_Entry)
#define TCAM_UDP_ESP_SPORT_Number_Of_Set		1
#define TCAM_UDP_ESP_SPORT_Entry_Per_Set		1
#define TCAM_UDP_ESP_SPORT_Total_Entry			(TCAM_UDP_ESP_SPORT_Number_Of_Set*TCAM_UDP_ESP_SPORT_Entry_Per_Set)
/*UDP_ESP DPORT*/
#define TCAM_UDP_ESP_DPORT_Start_In_TCAM		(TCAM_UDP_ESP_SPORT_Start_In_TCAM+TCAM_UDP_ESP_SPORT_Total_Entry)
#define TCAM_UDP_ESP_DPORT_Number_Of_Set		1
#define TCAM_UDP_ESP_DPORT_Entry_Per_Set		1
#define TCAM_UDP_ESP_DPORT_Total_Entry			(TCAM_UDP_ESP_DPORT_Number_Of_Set*TCAM_UDP_ESP_DPORT_Entry_Per_Set)
/*OFFSET*/
#define TCAM_OFFSET_Start_In_TCAM			(TCAM_UDP_ESP_DPORT_Start_In_TCAM+TCAM_UDP_ESP_DPORT_Total_Entry)
#define TCAM_OFFSET_Number_Of_Set			64
#define TCAM_OFFSET_Entry_Per_Set			1
#define TCAM_OFFSET_Total_Entry				(TCAM_OFFSET_Number_Of_Set*TCAM_OFFSET_Entry_Per_Set)


#define IO_TCAM_DATA			0x02B0
#define IO_TCAM_PORT			0x02B4
#define IO_TCAM_DOUT			0x02B8
#define IO_TCAM_VOUT			0x02BC
#define IO_PKT_RULE_ACT0		0x0200
#define IO_PKT_LKBT0_SET0		0x0210
#define IO_PKT_CLR			0x0250
#define IO_PKT_RULE0			0x0300
#define IO_PKT_RULE1			0x0310
#define IO_PKT_RULE_EN			0x02F0

static const TCAM_Property_st TCAM_Property = {
	{
		TCAM_MAC_Start_In_TCAM,			TCAM_VLAN_Start_In_TCAM,
		TCAM_TYPE_Start_In_TCAM,			TCAM_PTLv4_Start_In_TCAM,
		TCAM_PTLv6_Start_In_TCAM,			TCAM_SIPv4_Start_In_TCAM,
		TCAM_DIPv4_Start_In_TCAM, 			TCAM_SIPv6_Start_In_TCAM,
		TCAM_DIPv6_Start_In_TCAM,			TCAM_SPORT_Start_In_TCAM,
		TCAM_DPORT_Start_In_TCAM,			TCAM_Teredo_SPORT_Start_In_TCAM,
		TCAM_Teredo_DPORT_Start_In_TCAM,	TCAM_UDP_ESP_SPORT_Start_In_TCAM,
		TCAM_UDP_ESP_DPORT_Start_In_TCAM,	TCAM_OFFSET_Start_In_TCAM,
	},
	{
		TCAM_MAC_Number_Of_Set,			TCAM_VLAN_Number_Of_Set,
		TCAM_TYPE_Number_Of_Set,			TCAM_PTLv4_Number_Of_Set,
		TCAM_PTLv6_Number_Of_Set,			TCAM_SIPv4_Number_Of_Set,
		TCAM_DIPv4_Number_Of_Set, 			TCAM_SIPv6_Number_Of_Set,
		TCAM_DIPv6_Number_Of_Set,			TCAM_SPORT_Number_Of_Set,
		TCAM_DPORT_Number_Of_Set,		TCAM_Teredo_SPORT_Number_Of_Set,
		TCAM_Teredo_DPORT_Number_Of_Set,	TCAM_UDP_ESP_SPORT_Number_Of_Set,
		TCAM_UDP_ESP_DPORT_Number_Of_Set,	TCAM_OFFSET_Number_Of_Set,
	},
	{
		TCAM_MAC_Entry_Per_Set,			TCAM_VLAN_Entry_Per_Set,
		TCAM_TYPE_Entry_Per_Set,			TCAM_PTLv4_Entry_Per_Set,
		TCAM_PTLv6_Entry_Per_Set,			TCAM_SIPv4_Entry_Per_Set,
		TCAM_DIPv4_Entry_Per_Set, 			TCAM_SIPv6_Entry_Per_Set,
		TCAM_DIPv6_Entry_Per_Set,			TCAM_SPORT_Entry_Per_Set,
		TCAM_DPORT_Entry_Per_Set,			TCAM_Teredo_SPORT_Entry_Per_Set,
		TCAM_Teredo_DPORT_Entry_Per_Set,	TCAM_UDP_ESP_SPORT_Entry_Per_Set,
		TCAM_UDP_ESP_DPORT_Entry_Per_Set,	TCAM_OFFSET_Entry_Per_Set,
	},
};

/*------------------Software Define-----------------------*/
enum PFRuleIdx {
	ArpFRule = 0,
	OOBUnicastPFRule = 1, /* Packets match OOB Mac Address, IPv4, TCP/UCP/ICMP will be accepted */
	OOBIPv6PFRule = 2, /* Packets match OOB Mac Address/Multicast address, IPv6 will be accepted */
	OOBPortPFRule = 3, /* Filter multicast/broadcast packets(Do not care ip version?)*/
	IBIPv4TCPPortPFRule = 1,
	IBIPv4UDPPortPFRule = 2,
	IBIPv6TCPPortPFRule = 3,
	IBIPv6UDPPortPFRule = 4,
	IBIPv6ICMPPFRule = 5,
	NumOfPFRule,
};
enum TCAMSetIdx {
	TCAMMacIDSet = 0,
	TCAMVLANTagSet,
	TCAMTypeSet,
	TCAMIPv4PTLSet,
	TCAMIPv6PTLSet,
	TCAMDIPv4Set,
	TCAMDIPv6Set,
	TCAMDDPortSet,
	NumOfTCAMSet,
};

enum IPv4ListIdx {
	UniIPv4Addr = 0,
	LBIPv4Addr,
	GBIPv4Addr,
	MIPv4Addr,
	NumOfIPv4Addr,
};

enum IPv6ListIdx {
	UniIPv6Addr = 0,
	LLIPv6Addr,
	NumOfIPv6Addr,
};

enum EthTypeBitMap {
	IPv4TypeBitMap = 1 << 0,
	IPv6TypeBitMap = 1 << 1,
	ARPTypeBitMap = 1 << 2,
	VLANTypeBitMap = 1 << 3,
};

enum MacIDBitMap {
	IBMacBitMap = 1 << 0,
	OOBMacBitMap = 1 << 1,
	Mac8021xBitMap = 1 << 2,
	PTPv1MacBitMap = 1 << 3,
	PTPv2MacBitMap = 1 << 3,
};


enum IPv4AddrBitMap {
	EnableUniIPv4Addr = 1 << 0,
	EnableLBIPv4Addr = 1 << 1,
	EnableGBIPv4Addr = 1 << 2,
	EnableMIPv4Addr = 1 << 3,
};

enum IPv6AddrBitMap {
	EnableUniIPv6Addr = 1 << 0,
	EnableLLLIPv6Addr = 1 << 1,
	EnableMIPv6Addr = 1 << 2,
};

enum IPv4PTLBitMap {
	IPv4PTLTCP = 	1 << 0,
	IPv4PTLUDP = 	1 << 1,
	IPv4PTLICMP = 	1 << 2,
	IPv4PTLAH =	1 << 3,
	IPv4PTLESP =	1 << 4,
	IPv4PTLIPV4IPV6 = 1 << 5,
};

enum IPv6PTLBitMap {
	IPv6PTLTCP =	1 << 0,
	IPv6PTLUDP =	1 << 1,
	IPv6PTLICMP =	1 << 2,
	IPv6PTLAH =	1 << 3,
	IPv6PTLESP =	1 << 4,
	IPv6PTLHopByHop = 1 << 5,
	IPv6PTLRoute = 	1 << 6,
	IPv6PTLDest =	1 << 7,
	IPv6PTLFrag =	1 << 8,

};

#endif

#define CYAN	"\033[0;36m"
#define NONE	"\033[m"

#define R8169_MSG_DEFAULT \
	(NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_IFUP | NETIF_MSG_IFDOWN)

#define TX_SLOTS_AVAIL(tp) \
	(tp->dirty_tx + NUM_TX_DESC - tp->cur_tx)

/* A skbuff with nr_frags needs nr_frags+1 entries in the tx queue */
#define TX_FRAGS_READY_FOR(tp, nr_frags) \
	(TX_SLOTS_AVAIL(tp) >= (nr_frags + 1))

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).*/
/* The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

#define MAX_READ_REQUEST_SHIFT	12

#define TO_BYPASS_MODE 		0x11

#define R8169_REGS_SIZE		256
#define R8169_NAPI_WEIGHT	16
#define NUM_TX_DESC	32	/* Number of Tx descriptor registers */
#define NUM_RX_DESC	32U	/* Number of Rx descriptor registers */
#define R8169_TX_RING_BYTES	(NUM_TX_DESC * sizeof(struct TxDesc))
#define R8169_RX_RING_BYTES	(NUM_RX_DESC * sizeof(struct RxDesc))

#define RTL8169_TX_TIMEOUT	(6 * HZ)
#define RTL8169_PHY_TIMEOUT	(HZ/2)

#define MAX_SKB_FRAGS_OOB	8

#define RTL_OOB_PROC 1
#define NumOfSwisr	10

#ifdef RTL_OOB_PROC
static struct proc_dir_entry *rtw_proc;
#endif

/* write/read MMIO register */
#define RTL_W8(reg, val8)	writeb((val8), ioaddr + (reg))
#define RTL_W16(reg, val16) writew((val16), ioaddr + (reg))
#define RTL_W32(reg, val32) writel((val32), ioaddr + (reg))
#define RTL_R8(reg)		readb(ioaddr + (reg))
#define RTL_R16(reg)		readw(ioaddr + (reg))
#define RTL_R32(reg)		readl(ioaddr + (reg))

enum mac_version {
	RTL_GIGA_MAC_VER_01 = 0,
	RTL_GIGA_MAC_NONE   = 0xff,
};

enum rtl_tx_desc_version {
	RTL_TD_0	= 0,
	RTL_TD_1	= 1,
};

#define JUMBO_1K	ETH_DATA_LEN
#define JUMBO_9K	(9 * 1024 - ETH_HLEN - 2)

#define _R(NAME, TD, FW, SZ, B) {	\
	.name = NAME,		\
	.txd_version = TD,	\
	.fw_name = FW,		\
	.jumbo_max = SZ,	\
	.jumbo_tx_csum = B	\
}

static const struct {
	const char *name;
	enum rtl_tx_desc_version txd_version;
	const char *fw_name;
	u16 jumbo_max;
	bool jumbo_tx_csum;
} rtl_chip_infos[] = {
	/* PCI devices. */
	[RTL_GIGA_MAC_VER_01] =
		_R("RTL8117",		RTL_TD_1, NULL, JUMBO_9K, false),
};

#undef _R

enum cfg_version {
	RTL_CFG_0 = 0x00,
	RTL_CFG_1,
	RTL_CFG_2
};

static const struct of_device_id rtl8168oob_dt_ids[] = {
	{ .compatible = "Realtek,r8117", },
	{},
};

MODULE_DEVICE_TABLE(of, rtl8168oob_dt_ids);

static int rx_buf_sz = 1523;
static int use_dac;
static struct {
	u32 msg_enable;
} debug = { -1 };

enum rtl_registers {
	MAC0		= 0,	/* Ethernet hardware address. */
	MAC4		= 4,
	MAR0		= 8,	/* Multicast filter. */
	TxDescStartAddrLow	= 0x24,
	RxDescAddrLow	= 0x28,
	IntrMask	= 0x2c,
	IntrStatus	= 0x2e,
	TxPoll		= 0x30,
	ChipCmd		= 0x36,
	TxConfig	= 0x40,
	RxConfig	= 0x44,
	CPlusCmd	= 0x48,
	CounterAddrLow	= 0x50,
	IB_ACC_DATA	= 0xa0,
	IB_ACC_SET	= 0xa4,
	ExtIntrMask	= 0x102,
	ExtIntrStatus	= 0x100,
	PHYstatus	= 0x104,
	ISOLATEstatus	= 0x106,
	OOBREG		= 0x128,
	OOBLANWAKE	= 0x154,
	SWISR		= 0x180,
	PCIMSG		= 0x185,
	PCICMD		= 0x186,
	OOBLANWAKEStatus	= 0x188,
};

enum rtl_register_content {
	/* InterruptStatusBits */
	SWInt		= 0x0040,
	LinkChg		= 0x0020,
	RxOverflow	= 0x0010,
	TxErr		= 0x0008,
	TxOK		= 0x0004,
	RxErr		= 0x0002,
	RxOK		= 0x0001,

	/* RxStatusDesc */
	RxBOVF	= (1 << 24),
	RxFOVF	= (1 << 23),
	RxCRC	= (1 << 30),

	/* ChipCmdBits */
	StopReq		= 0x80,
	CmdReset	= 0x10,
	CmdRxEnb	= 0x08,
	CmdTxEnb	= 0x04,
	RxBufEmpty	= 0x01,

	/* TXPoll register p.5 */
	NPQ		= 0x80,		/* Poll cmd on the low prio queue */

	/* rx_mode_bits */
	AcceptErr	= 0x20,
	AcceptRunt	= 0x10,
	AcceptBroadcast	= 0x08,
	AcceptMulticast	= 0x04,
	AcceptMyPhys	= 0x02,
	AcceptAllPhys	= 0x01,
#define RX_CONFIG_ACCEPT_MASK		0x3f

	/* CPlusCmd p.31 */
	RxVlan		= (1 << 6),
	RxChkSum	= (1 << 5),
	PCIDAC		= (1 << 4),
	PCIMulRW	= (1 << 3),
	INTT_0		= 0x0000,
	INTT_1		= 0x0001,
	INTT_2		= 0x0002,
	INTT_3		= 0x0003,

	/* RTL8168OOB_PHYstatus */
	TBI_Enable	= 0x80,
	TxFlowCtrl	= 0x40,
	RxFlowCtrl	= 0x20,
	_1000bpsF	= 0x20,
	_100bps		= 0x10,
	_10bps		= 0x08,
	LinkStatus	= 0x04,

	/* ResetCounterCommand */
	CounterReset	= 0x1,
	/* DumpCounterCommand */
	CounterDump	= 0x8,

	/* OOB Access Inband */
	IBAR_FLAG	= 0x80000000,

	/* External Interrupt */
	IsolateInt	= 0x01,

	/* OOB REG */
	Driver_Rdy	= 0x04,
	DASH_en		= 0x01,
};

enum rtl_desc_bit {
	/* First doubleword. */
	DescOwn		= (1 << 31), /* Descriptor is owned by NIC */
	RingEnd		= (1 << 30), /* End of descriptor ring */
	FirstFrag	= (1 << 29), /* First segment of a packet */
	LastFrag	= (1 << 28), /* Final segment of a packet */
};

/* Generic case. */
enum rtl_tx_desc_bit {
	/* First doubleword. */
	TD_LSO		= (1 << 27),		/* Large Send Offload */
#define TD_MSS_MAX			0x07ffu	/* MSS value */

	/* Second doubleword. */
	TxVlanTag	= (1 << 17),		/* Add VLAN tag */
};

/* 8169, 8168b and 810x except 8102e. */
enum rtl_tx_desc_bit_0 {
	/* First doubleword. */
#define TD0_MSS_SHIFT			16	/* MSS position (11 bits) */
	TD0_TCP_CS	= (1 << 16),		/* Calculate TCP/IP checksum */
	TD0_UDP_CS	= (1 << 17),		/* Calculate UDP/IP checksum */
	TD0_IP_CS	= (1 << 18),		/* Calculate IP checksum */
};

/* 8102e, 8168c and beyond. */
enum rtl_tx_desc_bit_1 {
	/* Second doubleword. */
#define TD1_MSS_SHIFT			18	/* MSS position (11 bits) */
	TD1_IP_CS	= (1 << 29),		/* Calculate IP checksum */
	TD1_TCP_CS	= (1 << 30),		/* Calculate TCP/IP checksum */
	TD1_UDP_CS	= (1 << 31),		/* Calculate UDP/IP checksum */
};

static const struct rtl_tx_desc_info {
	struct {
		u32 udp;
		u32 tcp;
	} checksum;
	u16 mss_shift;
	u16 opts_offset;
} tx_desc_info[] = {
	[RTL_TD_0] = {
		.checksum = {
			.udp	= TD0_IP_CS | TD0_UDP_CS,
			.tcp	= TD0_IP_CS | TD0_TCP_CS
		},
		.mss_shift	= TD0_MSS_SHIFT,
		.opts_offset	= 0
	},
	[RTL_TD_1] = {
		.checksum = {
			.udp	= TD1_IP_CS | TD1_UDP_CS,
			.tcp	= TD1_IP_CS | TD1_TCP_CS
		},
		.mss_shift	= TD1_MSS_SHIFT,
		.opts_offset	= 1
	}
};

enum rtl_rx_desc_bit {
	/* Rx private */
	PID1		= (1 << 22), /* Protocol ID bit 1/2 */
	PID0		= (1 << 21), /* Protocol ID bit 2/2 */

#define RxProtoUDP	(PID1)
#define RxProtoTCP	(PID0)
#define RxProtoIP	(PID1 | PID0)
#define RxProtoMask	RxProtoIP

	IPFail		= (1 << 16), /* IP checksum failed */
	UDPFail		= (1 << 15), /* UDP/IP checksum failed */
	TCPFail		= (1 << 14), /* TCP/IP checksum failed */
	RxVlanTag	= (1 << 16), /* VLAN tag available */
};

#define RsvdMask	0x3fffc000

struct TxDesc {
	__le32 opts1;
	__le32 opts2;
	__le64 addr;
};

struct RxDesc {
	__le32 opts1;
	__le32 opts2;
	__le64 addr;
};

struct ring_info {
	struct sk_buff	*skb;
	u32		len;
	u8		__pad[sizeof(void *) - sizeof(u32)];
};

enum features {
	RTL_FEATURE_WOL		= (1 << 0),
	RTL_FEATURE_MSI		= (1 << 1),
	RTL_FEATURE_GMII	= (1 << 2),
};

struct rtl8169_counters {
	__le64	tx_packets;
	__le64	rx_packets;
	__le64	tx_errors;
	__le32	rx_errors;
	__le16	rx_missed;
	__le16	align_errors;
	__le32	tx_one_collision;
	__le32	tx_multi_collision;
	__le64	rx_unicast;
	__le64	rx_broadcast;
	__le32	rx_multicast;
	__le16	tx_aborted;
	__le16	tx_underun;
};

enum rtl_flag {
	RTL_FLAG_TASK_ENABLED,
	RTL_FLAG_TASK_SLOW_PENDING,
	RTL_FLAG_TASK_RESET_PENDING,
	RTL_FLAG_TASK_PHY_PENDING,
	RTL_FLAG_MAX
};

struct rtl8169_stats {
	u64			packets;
	u64			bytes;
	struct u64_stats_sync	syncp;
};

typedef struct {
	u16 Value;
	void *func;
	u8 Valid;
	void *context;
} SWINT_RECORD;

struct rtl8169_private {
	void __iomem *mmio_addr;	/* memory map physical address */
	struct platform_device *pdev;
	struct net_device *dev;
	struct napi_struct napi;
	spinlock_t lock;
	u32 msg_enable;
	u16 txd_version;
	u16 mac_version;
	u32 cur_rx; /* Index into the Rx descriptor buffer of next Rx pkt. */
	u32 cur_tx; /* Index into the Tx descriptor buffer of next Rx pkt. */
	u32 dirty_tx;
	struct rtl8169_stats rx_stats;
	struct rtl8169_stats tx_stats;
	struct TxDesc *TxDescArray;	/* 256-aligned Tx descriptor ring */
	struct RxDesc *RxDescArray;	/* 256-aligned Rx descriptor ring */
	dma_addr_t TxPhyAddr;
	dma_addr_t RxPhyAddr;
	void *Rx_databuff[NUM_RX_DESC];	/* Rx data buffers */
	struct ring_info tx_skb[NUM_TX_DESC];	/* Tx data buffers */
	struct timer_list timer;
	u16 cp_cmd;

#ifdef RTL_OOB_PROC
	struct proc_dir_entry *dir_dev;
#endif
	u16 event_slow;

	int (*get_settings)(struct net_device *, struct ethtool_cmd *);

	void (*hw_start)(struct net_device *);

	unsigned int (*link_ok)(void __iomem *);
	int (*do_ioctl)(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd);

	struct {
		DECLARE_BITMAP(flags, RTL_FLAG_MAX);
		struct mutex mutex;
		struct work_struct work;
	} wk;

	unsigned features;

	struct mii_if_info mii;
	struct rtl8169_counters counters;
	u32 saved_wolopts;
	u32 opts1_mask;

	/* For link status change. */
	struct task_struct *kthr;
	wait_queue_head_t thr_wait;
	int link_chg;
	/* For isolate status change */
	struct task_struct *kthr_iso;
	wait_queue_head_t thr_wait_iso;
	int isolate_chg;

};

MODULE_AUTHOR("Realtek and the Linux r8169 crew <netdev@vger.kernel.org>");
MODULE_DESCRIPTION("RealTek RTL-8169 Gigabit Ethernet driver");
module_param(use_dac, int, 0);
MODULE_PARM_DESC(use_dac, "Enable PCI DAC. Unsafe on 32 bit PCI slot.");
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
MODULE_LICENSE("GPL");
MODULE_VERSION(RTL8169_VERSION);

static void rtl_lock_work(struct rtl8169_private *tp)
{
	mutex_lock(&tp->wk.mutex);
}

static void rtl_unlock_work(struct rtl8169_private *tp)
{
	mutex_unlock(&tp->wk.mutex);
}

static Rule_NO_et 	Rule_NO;
TCAM_Allocation TCAMMem[NumOfTCAMSet];
TCAMRule TCAMRuleMem[NumOfPFRule];
SWINT_RECORD record[NumOfSwisr] = {0};



/*****************************************************************************
 * Author : Han Wang (HanWang@realtek.com)

 * Create date : 2011/03/23

 * DESCRIPTION: TCAM_OCP_Read

 *****************************************************************************/
Ret_Code_et TCAM_OCP_Read(struct rtl8169_private *tp, u32 entry_number, TCAM_ReadType_et type, TCAM_Entry_Setting_st *pstTCAM_Table)
{
	volatile void *ioaddr = tp->mmio_addr;
	volatile u32 reg_data = 0;

	if (entry_number > TCAM_Entry_Number)
		return RET_FAIL;

	/*TCAM_ACC_FLAG*/
	reg_data |= DWBIT31;
	/*RW_SEL*/
	reg_data &= ~DWBIT30;
	/*TCAM_WM : read type select*/
	if (type == TCAM_data)
		reg_data |= DWBIT13;
	else
		reg_data &= ~DWBIT13;
	/*reg_data |= DWBIT12;*/
	/*TCAM_ADDR*/
	reg_data |= (entry_number & 0x000001FF);

	RTL_W32(IO_TCAM_PORT, reg_data);
	while ((DWBIT31 & RTL_R32(IO_TCAM_PORT)) == DWBIT31) {
	};

	reg_data = 0;
	reg_data = RTL_R32(IO_TCAM_DOUT);

	if (type == TCAM_data)
		pstTCAM_Table->Value = 0x0000FFFF & reg_data;
	else
		pstTCAM_Table->DontCareBit = 0x0000FFFF & (reg_data>>16);

	pstTCAM_Table->Valid = RTL_R32(IO_TCAM_VOUT)&0x01;

	return RET_OK;
}

Ret_Code_et TCAM_OCP_Write(struct rtl8169_private *tp, u16 DataBit, u16 DontCareBit, u8 Valid, u16 entry_number,
										ONOFF_Switch_et data, ONOFF_Switch_et care, ONOFF_Switch_et valid)
{
	volatile void *ioaddr = tp->mmio_addr;
	u32  reg_data = 0;
	TCAM_Entry_Setting_st stTCAM_Table = {0};

	/* patch solution */
	TCAM_OCP_Read(tp, entry_number, TCAM_data, &stTCAM_Table);
	TCAM_OCP_Read(tp, entry_number, TCAM_care, &stTCAM_Table);

	reg_data |= (DontCareBit<<16)&0xFFFF0000;
	reg_data |= DataBit;
	RTL_W32(IO_TCAM_DATA, reg_data);

	reg_data = 0;
	/*TCAM_ACC_FLAG*/
	reg_data |= DWBIT31;
	/*RW_SEL*/
	reg_data |= DWBIT30;
	/*VALID BIT*/
	reg_data |= (Valid&0x01)<<16;
	/*TCAM_WM : valid*/
	if (data == Switch_OFF)
		reg_data |= DWBIT15;
	/*TCAM_WM : care*/
	if (care == Switch_OFF)
		reg_data |= DWBIT14;
	/*TCAM_WM : data*/
	if (valid == Switch_OFF)
		reg_data |= DWBIT13;
	/*TCAM_ADDR*/
	reg_data |= (entry_number & 0x000001FF);

	RTL_W32(IO_TCAM_PORT, reg_data);
	while ((DWBIT31 & RTL_R32(IO_TCAM_PORT)) == DWBIT31) {
	};

	return RET_OK;
}

Ret_Code_et TCAM_AccessEntry(struct rtl8169_private *tp, TCAM_Entry_Type_et Type, u16 Number, u16 Set, TCAM_RW_et RW, TCAM_Entry_Setting_st *value)
{
	u32 entry_number = 0;
	Ret_Code_et ret = RET_OK;

	if (Number >= TCAM_Property.Entry_Per_Set[Type])
		return RET_FAIL;
	if (Set >= TCAM_Property.Number_Of_Set[Type])
		return RET_FAIL;
	if (RW >= TCAM_RW_MAX)
		return RET_FAIL;
	if (value == NULL)
		return RET_FAIL;

	entry_number = TCAM_Property.Start_In_TCAM[Type] + (Set*TCAM_Property.Entry_Per_Set[Type])+Number;

	if (RW == TCAM_Read) {
		ret = TCAM_OCP_Read(tp, entry_number, TCAM_data, value);
	} else
		ret = TCAM_OCP_Write(tp, value->Value, value->DontCareBit, value->Valid, entry_number, Switch_ON, Switch_ON, Switch_ON);

	return ret;
}

void PacketFilterSettingMAC(struct rtl8169_private *tp, u8 MAC_Set, u8 MAC_Address[6], u8 MAC_AcceptAll, ONOFF_Switch_et Valid)
{
	TCAM_Entry_Setting_st  st_TCAM_Entry_Setting = {0,};
	u8 i = 0;

	for (i = 0; i < TCAM_Property.Entry_Per_Set[TCAM_MAC]; i++) {
		st_TCAM_Entry_Setting.Value = MAC_Address[i*2]<<8|MAC_Address[i*2+1];

		if (MAC_AcceptAll)
			st_TCAM_Entry_Setting.DontCareBit = 0xFFFF;
		else
			st_TCAM_Entry_Setting.DontCareBit = 0;

		st_TCAM_Entry_Setting.Valid = Valid;
		TCAM_AccessEntry(tp, TCAM_MAC, i, MAC_Set, TCAM_Write, &st_TCAM_Entry_Setting);
	}
}

void PacketFilterSettingType(struct rtl8169_private *tp, u8 Type_Set, u16 TypeValue, ONOFF_Switch_et Valid)
{
	TCAM_Entry_Setting_st  st_TCAM_Entry_Setting = {0,};

	st_TCAM_Entry_Setting.Value = TypeValue;
	st_TCAM_Entry_Setting.DontCareBit = 0;
	st_TCAM_Entry_Setting.Valid = Valid;
	TCAM_AccessEntry(tp, TCAM_TYPE, 0, Type_Set, TCAM_Write, &st_TCAM_Entry_Setting);
}

void PacketFilterSettingPTLv4(struct rtl8169_private *tp, u8 PTL_Set, u8 PTLv4, ONOFF_Switch_et Valid)
{
	TCAM_Entry_Setting_st  st_TCAM_Entry_Setting = {0,};

	st_TCAM_Entry_Setting.Value = PTLv4;
	st_TCAM_Entry_Setting.DontCareBit = 0xFF00;
	st_TCAM_Entry_Setting.Valid = Valid;
	TCAM_AccessEntry(tp, TCAM_PTLv4, 0, PTL_Set, TCAM_Write, &st_TCAM_Entry_Setting);
}

void PacketFilterSettingIPv4(struct rtl8169_private *tp, u8 IPv4_Set, u8 DIPv4[4], ONOFF_Switch_et Valid)
{
	TCAM_Entry_Setting_st  st_TCAM_Entry_Setting = {0,};
	u8 i = 0;

	for (i = 0; i < TCAM_Property.Entry_Per_Set[TCAM_DIPv4]; i++) {
		st_TCAM_Entry_Setting.Value = DIPv4[i*2]<<8|DIPv4[i*2+1];
		st_TCAM_Entry_Setting.DontCareBit = 0;
		st_TCAM_Entry_Setting.Valid = Valid;
		TCAM_AccessEntry(tp, TCAM_DIPv4, i, IPv4_Set, TCAM_Write, &st_TCAM_Entry_Setting);
	}
}

void PacketFilterSettingPort(struct rtl8169_private *tp, u8 Port_Set, u16 PortValue, ONOFF_Switch_et Valid)
{
	TCAM_Entry_Setting_st  st_TCAM_Entry_Setting = {0,};

	st_TCAM_Entry_Setting.Value = PortValue;
	st_TCAM_Entry_Setting.DontCareBit = 0;
	st_TCAM_Entry_Setting.Valid = Valid;
	TCAM_AccessEntry(tp, TCAM_DPORT, 0, Port_Set, TCAM_Write, &st_TCAM_Entry_Setting);
}


void PacketFillDefault(struct rtl8169_private *tp, u8 MAC_AcceptAll)
{
	volatile void *ioaddr = tp->mmio_addr;
	u8 MAC_Address[6], i;

	/*GetMacAddr_F(MAC_Address, eth0);*/

	for (i = 0; i < MAC_ADDR_LEN; i++)
		MAC_Address[i] = RTL_R8(MAC0 + i);
	PacketFilterSettingMAC(tp, 1, MAC_Address, MAC_AcceptAll, Switch_ON);

	PacketFilterSettingType(tp, 0, 0x0800, Switch_ON);
	PacketFilterSettingType(tp, 2, 0x0806, Switch_ON);

	PacketFilterSettingPTLv4(tp, 0, IP_PROTO_TCP, Switch_ON);
	PacketFilterSettingPTLv4(tp, 1, IP_PROTO_UDP, Switch_ON);

	PacketFilterSettingPort(tp, 0, 0x1BB, Switch_ON);
	PacketFilterSettingPort(tp, 10, 0x44, Switch_ON);
}

Ret_Code_et PacketFilterRuleEn(struct rtl8169_private *tp, Rule_NO_et number, ONOFF_Switch_et OnOff)
{
	volatile void *ioaddr = tp->mmio_addr;
	u32 data = 0;

	if (number > RULE_NO_MAX)
		return RET_FAIL;

	if (OnOff == Switch_ON) {
		data = RTL_R32(IO_PKT_RULE_EN);
		data |= (u32)0x00000001 << number;
		data |= DWBIT31;
		RTL_W32(IO_PKT_RULE_EN, data);
		while ((DWBIT31&RTL_R32(IO_PKT_RULE_EN)) == DWBIT31) {
		};
	} else{
		data = RTL_R32(IO_PKT_RULE_EN);
		data &= ~((u32)0x00000001 << number);
		data |= DWBIT31;
		RTL_W32(IO_PKT_RULE_EN, data);
		while ((DWBIT31&RTL_R32(IO_PKT_RULE_EN)) == DWBIT31) {
		};
	}
	return RET_OK;
}

void TCAM_RuleActionSet(struct rtl8169_private *tp, TCAM_RuleActionSet_st *act)
{
	volatile void *ioaddr = tp->mmio_addr;
	u32 value = 0;
	u32 offset = 0;
	u32 ActValue = 0;

	ActValue = (act->IB | act->OOB<<1 | act->Drop<<2 | act->Meter_No<<3 | act->Meter_En<<6 | act->Gamming<<7);
	offset = (act->number/4)*4;
	value = RTL_R32(IO_PKT_RULE_ACT0+offset);
	value &= ~((u32)0x000000FF<<((act->number%4)*8));
	value |= ActValue<<((act->number%4)*8);
	RTL_W32(IO_PKT_RULE_ACT0+offset, value);
}



Ret_Code_et TCAM_WriteRule(struct rtl8169_private *tp, Rule_NO_et number, RuleFormat_et bit, ONOFF_Switch_et OnOff)
{
	volatile void *ioaddr = tp->mmio_addr;
	u32 rule = 0;

	if (number > RULE_NO_MAX || bit > MAX_RULE_NUMBER)
		return RET_FAIL;

	rule = IO_PKT_RULE0;
	rule = rule + (sizeof(u32)*4*number);
	rule = rule + (sizeof(u32)*(bit>>5));

	if (OnOff == Switch_ON)
		RTL_W32(rule, RTL_R32(rule) | (0x01<<(bit%32)));
	else
		RTL_W32(rule, RTL_R32(rule) & (0x01<<(bit%32)));

	return RET_OK;
}

RuleFormat_et TCAM_GetRuleBit (TCAM_Entry_Type_et Type, u16 Set)
{
	switch (Type) {
	case TCAM_MAC:
		return MAC_0+Set;
	case TCAM_MARIB:
		return MARI;
	case TCAM_MAROOB:
		return MARO;
	case TCAM_VLAN:
		if (Set <= 3)
			return VLAN_0+Set;
		else if (Set <= 5)
			return VLAN_4to5;
		else if (Set <= 10)
			return VLAN_6to10;
		else if (Set <= 15)
			return VLAN_11to15;
		else
			return OUT_OF_RANGE;
	case TCAM_TYPE:
		if (Set <= 7)
			return TYPE_0+Set;
		else if (Set <= 11)
			return TYPE_8to11;
		else if (Set <= 15)
			return TYPE_12to15;
		else
			return OUT_OF_RANGE;
	case TCAM_PTLv4:
	case TCAM_PTLv6:
		return PTL_0+Set;
	case TCAM_SIPv4:
	case TCAM_SIPv6:
		return SIP_0+Set;
	case TCAM_DIPv4:
	case TCAM_DIPv6:
		return DIP_0+Set;
	case TCAM_SPORT:
		if (Set <= 4)
			return SPORT_0to4;
		else if (Set <= 9)
			return SPORT_5to9;
		else if (Set <= 14)
			return SPORT_10to14;
		else if (Set <= 19)
			return SPORT_15to19;
		else if (Set <= 24)
			return SPORT_20to24;
		else if (Set <= 29)
			return SPORT_25to29;
		else if (Set <= 39)
			return SPORT_30to39;
		else if (Set <= 49)
			return SPORT_40to49;
		else if (Set <= 59)
			return SPORT_50to59;
		else if (Set <= 69)
			return SPORT_60to69;
		else
			return OUT_OF_RANGE;
	case TCAM_DPORT:
		if (Set <= 9)
			return DPORT_0to9;
		else if (Set <= 19)
			return DPORT_10to19;
		else if (Set <= 29)
			return DPORT_20to29;
		else if (Set <= 39)
			return DPORT_30to39;
		else if (Set <= 49)
			return DPORT_40to49;
		else if (Set <= 59)
			return DPORT_50to59;
		else if (Set <= 69)
			return DPORT_60to69;
		else if (Set <= 79)
			return DPORT_70to79;
		else if (Set <= 89)
			return DPORT_80to89;
		else if (Set <= 99)
			return DPORT_90to99;
		else if (Set <= 109)
			return DPORT_100to109;
		else if (Set <= 127)
			return DPORT_110to127;
		else
			return OUT_OF_RANGE;
	case TCAM_Teredo_SPORT:
	case TCAM_Teredo_DPORT:
	case TCAM_UDP_ESP_SPORT:
	case TCAM_UDP_ESP_DPORT:
		return 100;
	case TCAM_OFFSET:
		if (Set <= 3)
			return OFFSET_0to3;
		else if (Set <= 7)
			return OFFSET_4to7;
		else if (Set <= 11)
			return OFFSET_8to11;
		else if (Set <= 15)
			return OFFSET_12to15;
		else if (Set <= 19)
			return OFFSET_16to19;
		else if (Set <= 23)
			return OFFSET_20to23;
		else if (Set <= 27)
			return OFFSET_24to27;
		else if (Set <= 31)
			return OFFSET_28to31;
		else if (Set <= 35)
			return OFFSET_32to35;
		else if (Set <= 39)
			return OFFSET_36to39;
		else if (Set <= 43)
			return OFFSET_40to43;
		else if (Set <= 47)
			return OFFSET_44to47;
		else if (Set <= 51)
			return OFFSET_48to51;
		else if (Set <= 55)
			return OFFSET_52to55;
		else if (Set <= 59)
			return OFFSET_56to59;
		else if (Set <= 63)
			return OFFSET_60to63;
		else
			return OUT_OF_RANGE;
	default:
		return OUT_OF_RANGE;
	}
}

void PacketFilterInit(struct rtl8169_private *tp)
{
	volatile void *ioaddr = tp->mmio_addr;
	u32 i = 0, j = 0;

	/*TCAM vaild bit clear and all Leaky Bucket clear*/
	RTL_W32(IO_PKT_CLR, 0x800000FF);

	/*Fill default value*/
	PacketFillDefault(tp, 0);

	/* disable all rule to every rule*/
	for (i = 0; i < RULE_NO_MAX; i++) {
		for (j = 0; j < MAX_RULE_NUMBER; j++) {
			TCAM_WriteRule(tp, i, j, Switch_OFF);
		}
	}

	/*Rule 0 use IB RCR setting and disable all rule*/
	RTL_W32(IO_PKT_RULE_EN, 0x10000000);
	while (DWBIT31 == (DWBIT31 & RTL_R32(IO_PKT_RULE_EN))) {
	};
}

Rule_NO_et PacketFilterSetRule(struct rtl8169_private *tp, TCAMRule *rule, Rule_NO_et rule_no)
{
	u32 i = 0;
	TCAM_RuleActionSet_st act = {0,};

	if (rule_no >= RULE_NO_MAX)
		return RULE_SETTING_FAIL;

	if (rule->MARType & UNICAST_MAR) {
		for (i = 0; i < 16; i++) {
			if (rule->MACIDBitMap& (u16)0x01<<i)
				TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_MAC, i), Switch_ON);
		}
	}

	if (rule->MARType & MULCAST_MAR) {
		if (rule->IBMAR)
			TCAM_WriteRule(tp, rule_no, MARI, Switch_ON);
		if (rule->OOBMAR)
			TCAM_WriteRule(tp, rule_no, MARO, Switch_ON);
	}

	if (rule->MARType & BROADCAST_MAR)
		TCAM_WriteRule(tp, rule_no, BRD, Switch_ON);

	/*VLAN*/
	for (i = 0; i < 16; i++) {
		if (rule->VLANBitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_VLAN, i), Switch_ON);
	}
	/*TYPE*/
	for (i = 0; i < 16; i++) {
		if (rule->TypeBitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_TYPE, i), Switch_ON);
	}
	/*IPv4 Protocol*/
	for (i = 0; i < 16; i++) {
		if (rule->IPv4PTLBitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_PTLv4, i), Switch_ON);
	}
	/*IPv6 Protocol*/
	for (i = 0; i < 16; i++) {
		if (rule->IPv6PTLBitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_PTLv6, i), Switch_ON);
	}
	/*DIPv4*/
	for (i = 0; i < 16; i++) {
		if (rule->DIPv4BitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_DIPv4, i), Switch_ON);
	}
	/*DIPv6*/
	for (i = 0; i < 16; i++) {
		if (rule->DIPv6BitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_DIPv6, i), Switch_ON);
	}
	/*PORT*/
	for (i = 0; i < 32; i++) {
		if (rule->DPortBitMap& (u16)0x01<<i)
			TCAM_WriteRule(tp, rule_no, TCAM_GetRuleBit(TCAM_DPORT, i), Switch_ON);
	}

	/*set action*/
	if (rule->FRIBNotDrop == 0)
		act.IB = 1;
	if (rule->FROOB)
		act.OOB = 1;
	act.number = rule_no;
	TCAM_RuleActionSet(tp, &act);

	/*enable rule*/
	PacketFilterRuleEn(tp, rule_no, Switch_ON);

	return rule_no;
}

/* Clear all port filter rule */
void clearAllPFRule(struct rtl8169_private *tp)
{
	volatile void *ioaddr = tp->mmio_addr;
	u32  i = 0, j = 0;
	TCAM_RuleActionSet_st act = {0,};

	/* TCAM vaild bit clear and all Leaky Bucket clear */
	RTL_W32(IO_PKT_CLR, 0x800000FF);

	/* disable all rule to every rule */
	for (i = 0; i < RULE_NO_MAX; i++) {
		for (j = 0; j < MAX_RULE_NUMBER; j++) {
			TCAM_WriteRule(tp, i, j, Switch_OFF);
		}

		act.number = i;
		TCAM_RuleActionSet(tp, &act);

		PacketFilterRuleEn(tp, i, Switch_OFF);
	}

	Rule_NO = 0;
}

void chgPFRule(struct rtl8169_private *tp, Rule_NO_et ruleNo, TCAMRule *rule)
{
	u32 j = 0;

	PacketFilterRuleEn(tp, ruleNo, Switch_OFF);
	for (j = 0; j < MAX_RULE_NUMBER; j++) {
		TCAM_WriteRule(tp, ruleNo, j, Switch_OFF);
	}

	PacketFilterSetRule(tp, rule, ruleNo);
}

Rule_NO_et setPFRule(struct rtl8169_private *tp, TCAMRule *rule)
{

	if (rule->isSet) {
		chgPFRule(tp, rule->ruleNO, rule);
		return rule->ruleNO;
	} else {
		rule->isSet = 1;
		rule->ruleNO = PacketFilterSetRule(tp, rule, Rule_NO++);
		return rule->ruleNO;
	}
}

void RstSharePFRuleMem(TCAMRule *rstRule, TCAMRule *copyRule)
{
	unsigned char ruleNO = rstRule->ruleNO;
	unsigned char isSet = rstRule->isSet;

	memset(rstRule, 0, sizeof(TCAMRule));
	if (copyRule) {
		memcpy(rstRule, copyRule, sizeof(TCAMRule));
	}
	rstRule->ruleNO = ruleNO;
	rstRule->isSet = isSet;
}

void PFMemInit(void)
{
	memset(TCAMMem, 0, sizeof(TCAMMem));
	memset(TCAMRuleMem, 0, sizeof(TCAMRuleMem));
	/* Init TCAMMem type */
	TCAMMem[TCAMMacIDSet].type = TCAM_MAC;
	TCAMMem[TCAMVLANTagSet].type = TCAM_VLAN;
	TCAMMem[TCAMTypeSet].type = TCAM_TYPE;
	TCAMMem[TCAMIPv4PTLSet].type = TCAM_PTLv4;
	TCAMMem[TCAMIPv6PTLSet].type = TCAM_PTLv6;
	TCAMMem[TCAMDIPv4Set].type = TCAM_DIPv4;
	TCAMMem[TCAMDIPv6Set].type = TCAM_DIPv6;
	TCAMMem[TCAMDDPortSet].type = TCAM_DPORT;

	TCAMMem[TCAMDIPv4Set].num = NumOfIPv4Addr;
	TCAMMem[TCAMDIPv4Set].data = (unsigned char *)malloc(NumOfIPv4Addr*IPv4_ADR_LEN);
	memset(TCAMMem[TCAMDIPv4Set].data, 0, sizeof(NumOfIPv4Addr*IPv4_ADR_LEN));

	TCAMMem[TCAMDIPv6Set].num = NumOfIPv6Addr;
	TCAMMem[TCAMDIPv6Set].data = (unsigned char *)malloc(NumOfIPv6Addr*IPv6_ADR_LEN);
	memset(TCAMMem[TCAMDIPv6Set].data, 0, sizeof(NumOfIPv6Addr*IPv6_ADR_LEN));
}

void SetOOBBasicRule(struct rtl8169_private *tp)
{
	RstSharePFRuleMem(&TCAMRuleMem[ArpFRule], 0);
	RstSharePFRuleMem(&TCAMRuleMem[OOBUnicastPFRule], 0);
	RstSharePFRuleMem(&TCAMRuleMem[OOBIPv6PFRule], 0);
	/* ARP rule */
	TCAMRuleMem[ArpFRule].FROOB = 1;
	TCAMRuleMem[ArpFRule].OOBMAR = 1;
	TCAMRuleMem[ArpFRule].FRIBNotDrop = 1;
	TCAMRuleMem[ArpFRule].MARType = UNICAST_MAR | MULCAST_MAR | BROADCAST_MAR;
	TCAMRuleMem[ArpFRule].MACIDBitMap = OOBMacBitMap ;
	#if 0
	TCAMRuleMem[ArpFRule].TypeBitMap = ARPTypeBitMap;
	TCAMRuleMem[ArpFRule].DIPv4BitMap = EnableUniIPv4Addr;
	#endif
	TCAMRuleMem[ArpFRule].ruleNO = setPFRule(tp, &TCAMRuleMem[ArpFRule]);

	#if 0
	/*Accept IPv4/IPv6 udp unicast packet*/
	TCAMRuleMem[OOBUnicastPFRule].FROOB = 1;
	TCAMRuleMem[OOBUnicastPFRule].FRIBNotDrop = 1;
	TCAMRuleMem[OOBUnicastPFRule].MARType = UNICAST_MAR;
	TCAMRuleMem[OOBUnicastPFRule].MACIDBitMap = OOBMacBitMap;
	TCAMRuleMem[OOBUnicastPFRule].TypeBitMap = IPv4TypeBitMap | IPv6TypeBitMap;
	TCAMRuleMem[OOBUnicastPFRule].DIPv4BitMap = EnableUniIPv4Addr | EnableUniIPv6Addr;
	TCAMRuleMem[OOBUnicastPFRule].IPv4PTLBitMap = IPv4PTLTCP | IPv4PTLUDP | IPv4PTLICMP | IPv6PTLUDP;
	TCAMRuleMem[OOBUnicastPFRule].ruleNO = setPFRule_F(&TCAMRuleMem[OOBUnicastPFRule]);

	/*Accept IPv6 tcp and icmp packet match OOB mac or multicast mac*/
	TCAMRuleMem[OOBIPv6PFRule].FROOB = 1;
	TCAMRuleMem[OOBIPv6PFRule].OOBMAR = 1;
	TCAMRuleMem[OOBIPv6PFRule].FRIBNotDrop = 1;
	TCAMRuleMem[OOBIPv6PFRule].MARType = UNICAST_MAR | MULCAST_MAR;
	TCAMRuleMem[OOBIPv6PFRule].MACIDBitMap = OOBMacBitMap;
	TCAMRuleMem[OOBIPv6PFRule].TypeBitMap = IPv6TypeBitMap;
	TCAMRuleMem[OOBIPv6PFRule].IPv6PTLBitMap = IPv6PTLTCP | IPv6PTLICMP;
	TCAMRuleMem[OOBIPv6PFRule].ruleNO = setPFRule_F(&TCAMRuleMem[OOBIPv6PFRule]);
	#endif

}

void SetIBBasicRule(struct rtl8169_private *tp)
{
	RstSharePFRuleMem(&TCAMRuleMem[OOBIPv6PFRule], 0);
	RstSharePFRuleMem(&TCAMRuleMem[OOBUnicastPFRule], 0);
	RstSharePFRuleMem(&TCAMRuleMem[OOBPortPFRule], 0);

	/*DHCP rule : Accept UDP port 68  unicast/broadcast packet */
	TCAMRuleMem[OOBIPv6PFRule].FROOB = 1;
	TCAMRuleMem[OOBIPv6PFRule].OOBMAR = 1;
	TCAMRuleMem[OOBIPv6PFRule].FRIBNotDrop = 1;
	TCAMRuleMem[OOBIPv6PFRule].MARType = UNICAST_MAR | BROADCAST_MAR;
	TCAMRuleMem[OOBIPv6PFRule].MACIDBitMap = OOBMacBitMap ;
	TCAMRuleMem[OOBIPv6PFRule].TypeBitMap = IPv4TypeBitMap;
	TCAMRuleMem[OOBIPv6PFRule].IPv4PTLBitMap = IPv4PTLUDP;
	TCAMRuleMem[OOBIPv6PFRule].DPortBitMap = 1<<10;
	TCAMRuleMem[OOBIPv6PFRule].ruleNO = setPFRule(tp, &TCAMRuleMem[OOBIPv6PFRule]);

	/*HTTPS rule: Accept TCP port 443 unicast packet*/
	TCAMRuleMem[OOBUnicastPFRule].FROOB = 1;
	TCAMRuleMem[OOBUnicastPFRule].FRIBNotDrop = 0;
	TCAMRuleMem[OOBUnicastPFRule].MARType = UNICAST_MAR;
	TCAMRuleMem[OOBUnicastPFRule].MACIDBitMap = OOBMacBitMap;
	TCAMRuleMem[OOBUnicastPFRule].TypeBitMap = IPv4TypeBitMap;
	TCAMRuleMem[OOBUnicastPFRule].IPv4PTLBitMap = IPv4PTLTCP;
	TCAMRuleMem[OOBUnicastPFRule].DPortBitMap = 1;
	TCAMRuleMem[OOBUnicastPFRule].ruleNO = setPFRule(tp, &TCAMRuleMem[OOBUnicastPFRule]);

	/*ARP rule */
	TCAMRuleMem[OOBPortPFRule].FROOB = 1;
	TCAMRuleMem[OOBPortPFRule].FRIBNotDrop = 1;
	TCAMRuleMem[OOBPortPFRule].MARType = UNICAST_MAR | BROADCAST_MAR;
	TCAMRuleMem[OOBPortPFRule].MACIDBitMap = OOBMacBitMap;
	TCAMRuleMem[OOBPortPFRule].TypeBitMap = ARPTypeBitMap;
	TCAMRuleMem[OOBPortPFRule].ruleNO = setPFRule(tp, &TCAMRuleMem[OOBPortPFRule]);

	PacketFillDefault(tp, 0);
}

void hwPFInit(struct rtl8169_private *tp)
{
	clearAllPFRule(tp);
	PacketFilterInit(tp);
	PFMemInit();
	SetOOBBasicRule(tp);
}

struct rtl_cond {
	bool (*check)(struct rtl8169_private *);
	const char *msg;
};

static void rtl_udelay(unsigned int d)
{
	udelay(d);
}

static bool rtl_loop_wait(struct rtl8169_private *tp, const struct rtl_cond *c,
			  void (*delay)(unsigned int), unsigned int d, int n,
			  bool high)
{
	int i;

	for (i = 0; i < n; i++) {
		delay(d);
		if (c->check(tp) == high)
			return true;
	}
	netif_err(tp, drv, tp->dev, "%s == %d (loop: %d, delay: %d).\n",
		  c->msg, !high, n, d);
	return false;
}
/*
static bool rtl_udelay_loop_wait_high(struct rtl8169_private *tp,
					  const struct rtl_cond *c,
					  unsigned int d, int n)
{
	return rtl_loop_wait(tp, c, rtl_udelay, d, n, true);
}
*/
static bool rtl_udelay_loop_wait_low(struct rtl8169_private *tp,
					 const struct rtl_cond *c,
					 unsigned int d, int n)
{
	return rtl_loop_wait(tp, c, rtl_udelay, d, n, false);
}
/*
static bool rtl_msleep_loop_wait_high(struct rtl8169_private *tp,
					  const struct rtl_cond *c,
					  unsigned int d, int n)
{
	return rtl_loop_wait(tp, c, msleep, d, n, true);
}

static bool rtl_msleep_loop_wait_low(struct rtl8169_private *tp,
					 const struct rtl_cond *c,
					 unsigned int d, int n)
{
	return rtl_loop_wait(tp, c, msleep, d, n, false);
}
*/
#define DECLARE_RTL_COND(name)				\
static bool name ## _check(struct rtl8169_private *);	\
							\
static const struct rtl_cond name = {			\
	.check	= name ## _check,			\
	.msg	= #name					\
};							\
							\
static bool name ## _check(struct rtl8169_private *tp)

static u16 rtl_get_events(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	return RTL_R16(IntrStatus);
}

static void rtl_ack_events(struct rtl8169_private *tp, u16 bits)
{
	void __iomem *ioaddr = tp->mmio_addr;

	RTL_W16(IntrStatus, bits);
	mmiowb();
}

static void rtl_irq_disable(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	RTL_W16(IntrMask, 0);
	RTL_W16(ExtIntrMask, 0);
	mmiowb();
}

static void rtl_irq_enable(struct rtl8169_private *tp, u16 bits)
{
	void __iomem *ioaddr = tp->mmio_addr;

	RTL_W16(IntrMask, bits);
}

#define RTL_EVENT_NAPI_RX	(RxOK | RxErr)
#define RTL_EVENT_NAPI_TX	(TxOK | TxErr)
#define RTL_EVENT_NAPI		(RTL_EVENT_NAPI_RX | RTL_EVENT_NAPI_TX | SWInt)

static void rtl_irq_enable_all(struct rtl8169_private *tp)
{
	rtl_irq_enable(tp, RTL_EVENT_NAPI | tp->event_slow);
}

static void rtl8169_irq_mask_and_ack(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	rtl_irq_disable(tp);
	rtl_ack_events(tp, RTL_EVENT_NAPI | tp->event_slow);
	RTL_R8(ChipCmd);
}


static unsigned int rtl8169_xmii_link_ok(void __iomem *ioaddr)
{
	return RTL_R8(PHYstatus) & LinkStatus;
}

static void __rtl8169_check_link_status(struct net_device *dev,
					struct rtl8169_private *tp,
					void __iomem *ioaddr, bool pm)
{
	if (tp->link_ok(ioaddr)) {
		/* This is to cancel a scheduled suspend if there's one. */
		if (pm)
			pm_request_resume(&tp->pdev->dev);
		netif_carrier_on(dev);
		if (net_ratelimit())
			netif_info(tp, ifup, dev, "link up\n");
	} else {
		netif_carrier_off(dev);
		netif_info(tp, ifdown, dev, "link down\n");
		if (pm)
			pm_schedule_suspend(&tp->pdev->dev, 5000);
	}
}

static void rtl8169_check_link_status(struct net_device *dev,
					  struct rtl8169_private *tp,
					  void __iomem *ioaddr)
{
	__rtl8169_check_link_status(dev, tp, ioaddr, false);
}

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)

static u32 __rtl8169_get_wol(struct rtl8169_private *tp)
{
	u32 wolopts = 0;

	return wolopts;
}

static void rtl8169_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl_lock_work(tp);

	wol->supported = WAKE_ANY;
	wol->wolopts = __rtl8169_get_wol(tp);

	rtl_unlock_work(tp);
}

static void __rtl8169_set_wol(struct rtl8169_private *tp, u32 wolopts)
{
}

static int rtl8169_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl_lock_work(tp);

	if (wol->wolopts)
		tp->features |= RTL_FEATURE_WOL;
	else
		tp->features &= ~RTL_FEATURE_WOL;
	__rtl8169_set_wol(tp, wol->wolopts);

	rtl_unlock_work(tp);

	device_set_wakeup_enable(&tp->pdev->dev, wol->wolopts);

	return 0;
}

static void rtl8169_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, MODULENAME, sizeof(info->driver));
	strlcpy(info->version, RTL8169_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, "RTK8117-ETN", sizeof(info->bus_info));
}

static int rtl8169_get_regs_len(struct net_device *dev)
{
	return R8169_REGS_SIZE;
}

static int rtl8169_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	return 0;
}

static netdev_features_t rtl8169_fix_features(struct net_device *dev,
	netdev_features_t features)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	if (dev->mtu > TD_MSS_MAX)
		features &= ~NETIF_F_ALL_TSO;

	if (dev->mtu > JUMBO_1K &&
		!rtl_chip_infos[tp->mac_version].jumbo_tx_csum)
		features &= ~NETIF_F_IP_CSUM;

	return features;
}

static void __rtl8169_set_features(struct net_device *dev,
				   netdev_features_t features)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	netdev_features_t changed = features ^ dev->features;
	void __iomem *ioaddr = tp->mmio_addr;

	if (!(changed & (NETIF_F_RXALL | NETIF_F_RXCSUM |
			 NETIF_F_HW_VLAN_CTAG_RX)))
		return;

	if (changed & (NETIF_F_RXCSUM | NETIF_F_HW_VLAN_CTAG_RX)) {
		if (features & NETIF_F_RXCSUM)
			tp->cp_cmd |= RxChkSum;
		else
			tp->cp_cmd &= ~RxChkSum;

		if (dev->features & NETIF_F_HW_VLAN_CTAG_RX)
			tp->cp_cmd |= RxVlan;
		else
			tp->cp_cmd &= ~RxVlan;

		RTL_W16(CPlusCmd, tp->cp_cmd);
		RTL_R16(CPlusCmd);
	}
	if (changed & NETIF_F_RXALL) {
		int tmp = (RTL_R16(RxConfig) & ~(AcceptErr | AcceptRunt));
		if (features & NETIF_F_RXALL)
			tmp |= (AcceptErr | AcceptRunt);
		RTL_W16(RxConfig, tmp);
	}
}

static int rtl8169_set_features(struct net_device *dev,
				netdev_features_t features)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl_lock_work(tp);
	__rtl8169_set_features(dev, features);
	rtl_unlock_work(tp);

	return 0;
}


static inline u32 rtl8169_tx_vlan_tag(struct sk_buff *skb)
{
	return (skb_vlan_tag_present(skb)) ?
		TxVlanTag | swab16(skb_vlan_tag_get(skb)) : 0x00;
}

static void rtl8169_rx_vlan_tag(struct RxDesc *desc, struct sk_buff *skb)
{
	u32 opts2 = le32_to_cpu(desc->opts2);

	if (opts2 & RxVlanTag)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), swab16(opts2 & 0xffff));
}

static int rtl8169_gset_xmii(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u8 status;

	status = RTL_R8(PHYstatus);

	if (status & _1000bpsF)
		cmd->speed = SPEED_1000;
	else if (status & _100bps)
		cmd->speed = SPEED_100;
	else if (status & _10bps)
		cmd->speed = SPEED_10;

	return 0;
}

static int rtl8169_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	int rc;

	rtl_lock_work(tp);
	rc = tp->get_settings(dev, cmd);
	rtl_unlock_work(tp);

	return rc;
}

static void rtl8169_get_regs(struct net_device *dev, struct ethtool_regs *regs,
				 void *p)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	if (regs->len > R8169_REGS_SIZE)
		regs->len = R8169_REGS_SIZE;

	rtl_lock_work(tp);
	memcpy_fromio(p, tp->mmio_addr, regs->len);
	rtl_unlock_work(tp);
}

static u32 rtl8169_get_msglevel(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return tp->msg_enable;
}

static void rtl8169_set_msglevel(struct net_device *dev, u32 value)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	tp->msg_enable = value;
}

static const char rtl8169_gstrings[][ETH_GSTRING_LEN] = {
	"tx_packets",
	"rx_packets",
	"tx_errors",
	"rx_errors",
	"rx_missed",
	"align_errors",
	"tx_single_collisions",
	"tx_multi_collisions",
	"unicast",
	"broadcast",
	"multicast",
	"tx_aborted",
	"tx_underrun",
};

static int rtl8169_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(rtl8169_gstrings);
	default:
		return -EOPNOTSUPP;
	}
}

DECLARE_RTL_COND(rtl_counters_cond)
{
	void __iomem *ioaddr = tp->mmio_addr;

	return RTL_R32(CounterAddrLow) & CounterDump;
}

static void rtl8169_update_counters(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct device *d = &tp->pdev->dev;
	struct rtl8169_counters *counters;
	dma_addr_t paddr;
	u32 cmd;

	/*
	 * Some chips are unable to dump tally counters when the receiver
	 * is disabled.
	 */
	if ((RTL_R8(ChipCmd) & CmdRxEnb) == 0)
		return;

	counters = dma_alloc_coherent(d, sizeof(*counters), &paddr, GFP_KERNEL);
	if (!counters)
		return;

	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(CounterAddrLow, cmd);
	RTL_W32(CounterAddrLow, cmd | CounterDump);

	if (rtl_udelay_loop_wait_low(tp, &rtl_counters_cond, 10, 1000))
		memcpy(&tp->counters, counters, sizeof(*counters));

	RTL_W32(CounterAddrLow, 0);

	dma_free_coherent(d, sizeof(*counters), counters, paddr);
}

static void rtl8169_clear_counters(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct device *d = &tp->pdev->dev;
	struct rtl8169_counters *counters;
	dma_addr_t paddr;
	u32 cmd;

	counters = dma_alloc_coherent(d, sizeof(*counters), &paddr, GFP_KERNEL);
	if (!counters)
		return;

	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(CounterAddrLow, cmd);
	RTL_W32(CounterAddrLow, cmd | CounterReset);

	RTL_W32(CounterAddrLow, 0);

	dma_free_coherent(d, sizeof(*counters), counters, paddr);
}

static void rtl8169_get_ethtool_stats(struct net_device *dev,
					  struct ethtool_stats *stats, u64 *data)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	ASSERT_RTNL();

	rtl8169_update_counters(dev);

	data[0] = le64_to_cpu(tp->counters.tx_packets);
	data[1] = le64_to_cpu(tp->counters.rx_packets);
	data[2] = le64_to_cpu(tp->counters.tx_errors);
	data[3] = le32_to_cpu(tp->counters.rx_errors);
	data[4] = le16_to_cpu(tp->counters.rx_missed);
	data[5] = le16_to_cpu(tp->counters.align_errors);
	data[6] = le32_to_cpu(tp->counters.tx_one_collision);
	data[7] = le32_to_cpu(tp->counters.tx_multi_collision);
	data[8] = le64_to_cpu(tp->counters.rx_unicast);
	data[9] = le64_to_cpu(tp->counters.rx_broadcast);
	data[10] = le32_to_cpu(tp->counters.rx_multicast);
	data[11] = le16_to_cpu(tp->counters.tx_aborted);
	data[12] = le16_to_cpu(tp->counters.tx_underun);
}

static void rtl8169_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(data, *rtl8169_gstrings, sizeof(rtl8169_gstrings));
		break;
	}
}

static const struct ethtool_ops rtl8169_ethtool_ops = {
	.get_drvinfo		= rtl8169_get_drvinfo,
	.get_regs_len		= rtl8169_get_regs_len,
	.get_link		= ethtool_op_get_link,
	.get_settings		= rtl8169_get_settings,
	.set_settings		= rtl8169_set_settings,
	.get_msglevel		= rtl8169_get_msglevel,
	.set_msglevel		= rtl8169_set_msglevel,
	.get_regs		= rtl8169_get_regs,
	.get_wol		= rtl8169_get_wol,
	.set_wol		= rtl8169_set_wol,
	.get_strings		= rtl8169_get_strings,
	.get_sset_count		= rtl8169_get_sset_count,
	.get_ethtool_stats	= rtl8169_get_ethtool_stats,
	.get_ts_info		= ethtool_op_get_ts_info,
};

static void rtl_schedule_task(struct rtl8169_private *tp, enum rtl_flag flag)
{
	if (!test_and_set_bit(flag, tp->wk.flags))
		schedule_work(&tp->wk.work);
}

DECLARE_RTL_COND(rtl_ibar_cond)
{
	void __iomem *ioaddr = tp->mmio_addr;

	return RTL_R32(IB_ACC_SET) & IBAR_FLAG;
}
#if 0

static u32 OOB_READ_IB(struct rtl8169_private *tp, u16 addr)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u32 tmp = ~0;

	rtl_lock_work(tp);

	RTL_W32(IB_ACC_SET, (0x800f0000 | addr));

	if (rtl_udelay_loop_wait_low(tp, &rtl_ibar_cond, 100, 20))
		tmp = RTL_R32(IB_ACC_DATA);
	else
		netif_err(tp, drv, tp->dev, "OOB_OOBREAD_IB timeout!\n");

	rtl_unlock_work(tp);

	return tmp;
}

static void OOB_WRITE_IB(struct rtl8169_private *tp, u16 addr, u32 data)
{
	void __iomem *ioaddr = tp->mmio_addr;

	rtl_lock_work(tp);

	RTL_W32(IB_ACC_DATA, data);
	RTL_W32(IB_ACC_SET, (0x808f0000 | addr));
	rtl_udelay_loop_wait_low(tp, &rtl_ibar_cond, 100, 20);

	rtl_unlock_work(tp);
}
#endif

static void rtl_dash_enable(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u8 tmp;

	rtl_lock_work(tp);

	tmp = RTL_R8(OOBREG);
	tmp |= DASH_en;
	RTL_W8(OOBREG, tmp);

	rtl_unlock_work(tp);
}

static void rtl_driver_ready_disable(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u8 tmp;

	rtl_lock_work(tp);

	tmp = RTL_R8(OOBREG);
	tmp &= ~Driver_Rdy;
	RTL_W8(OOBREG, tmp);

	rtl_unlock_work(tp);
}

static void rtl_driver_ready_enable(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u8 tmp;

	rtl_lock_work(tp);

	tmp = RTL_R8(OOBREG);
	tmp |= Driver_Rdy;
	RTL_W8(OOBREG, tmp);

	rtl_unlock_work(tp);
}

static void rtl_use_inband_mac_address(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u32 tmp0, tmp4;

	OOB_access_IB_reg(0xC000, &tmp0, 0xf, OOB_READ_OP);
	OOB_access_IB_reg(0xC004, &tmp4, 0xf, OOB_READ_OP);

	rtl_lock_work(tp);

	RTL_W32(MAC4, tmp4);
	RTL_R32(MAC4);

	RTL_W32(MAC0, tmp0);
	RTL_R32(MAC0);

	rtl_unlock_work(tp);
}


static void rtl_rar_set(struct rtl8169_private *tp, u8 *addr)
{
	void __iomem *ioaddr = tp->mmio_addr;

	rtl_lock_work(tp);

	RTL_W32(MAC4, addr[4] | addr[5] << 8);
	RTL_R32(MAC4);

	RTL_W32(MAC0, addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24);
	RTL_R32(MAC0);

	rtl_unlock_work(tp);
}

DECLARE_RTL_COND(rtl_npq_cond)
{
	void __iomem *ioaddr = tp->mmio_addr;

	return RTL_R8(TxPoll) & NPQ;
}



static void rtl_phy_work(struct rtl8169_private *tp)
{
	/* 8117 remove the patch first*/
	void __iomem *ioaddr = tp->mmio_addr;
	u16 ext_status;

	ext_status = RTL_R16(ExtIntrStatus);
	/*printk(KERN_ERR "[RTK_ETN] Enter %s  %x\n", __func__, ext_status);*/
	if (ext_status & IsolateInt) {
		tp->isolate_chg = 1;
		wake_up(&tp->thr_wait_iso);
	}

	mod_timer(&tp->timer, jiffies + RTL8169_PHY_TIMEOUT);
	return;
}


static void rtl8169_phy_timer(unsigned long __opaque)
{
	struct net_device *dev = (struct net_device *)__opaque;
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl_schedule_task(tp, RTL_FLAG_TASK_PHY_PENDING);
}

static void rtl8169_release_board(struct platform_device *pdev, struct net_device *dev,
				  void __iomem *ioaddr)
{
	free_netdev(dev);
}


static int rtl_set_mac_address(struct net_device *dev, void *p)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	rtl_rar_set(tp, dev->dev_addr);

	return 0;
}

static int rtl8169_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct mii_ioctl_data *data = if_mii(ifr);

	return netif_running(dev) ? tp->do_ioctl(tp, data, cmd) : -ENODEV;
}

static int rtl_xmii_ioctl(struct rtl8169_private *tp,
			  struct mii_ioctl_data *data, int cmd)
{
	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = 32; /* Internal PHY */
		return 0;

	case SIOCGMIIREG:
		return 0;

	case SIOCSMIIREG:
		return 0;
	}
	return -EOPNOTSUPP;
}

static void rtl_wol_suspend_quirk(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_01:
		RTL_W16(RxConfig, RTL_R16(RxConfig) |
			AcceptBroadcast | AcceptMulticast | AcceptMyPhys);
		break;
	default:
		break;
	}
}

static void rtl8169_init_ring_indexes(struct rtl8169_private *tp)
{
	tp->dirty_tx = tp->cur_tx = tp->cur_rx = 0;
}

static void rtl_hw_reset(struct rtl8169_private *tp)
{
	return;
}

static void rtl_rx_close(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	RTL_W16(RxConfig, RTL_R16(RxConfig) & ~RX_CONFIG_ACCEPT_MASK);
}
/*
DECLARE_RTL_COND(rtl_npq_cond)
{
	void __iomem *ioaddr = tp->mmio_addr;

	return RTL_R8(TxPoll) & NPQ;
}
*/

static void rtl8169_hw_reset(struct rtl8169_private *tp)
{
	/* Disable interrupts */
	rtl8169_irq_mask_and_ack(tp);

	rtl_rx_close(tp);

	rtl_hw_reset(tp);
}

static void rtl_set_rx_tx_config_registers(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	/* Set DMA burst size and Interframe Gap Time */
	RTL_W32(TxConfig, 0x30b);
}

static void rtl_hw_start(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	tp->hw_start(dev);

	rtl_irq_enable_all(tp);
	RTL_W16(ExtIntrMask, IsolateInt);
}

static void rtl_set_rx_tx_desc_registers(struct rtl8169_private *tp,
					 void __iomem *ioaddr)
{
	/*
	 * Magic spell: some iop3xx ARM board needs the TxDescAddrHigh
	 * register to be written before TxDescAddrLow to work.
	 * Switching from MMIO to I/O access fixes the issue as well.
	 */

	RTL_W32(TxDescStartAddrLow, ((u64) tp->TxPhyAddr) & DMA_BIT_MASK(32));
	RTL_W32(RxDescAddrLow, ((u64) tp->RxPhyAddr) & DMA_BIT_MASK(32));
}

static void rtl_set_rx_mode(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u32 mc_filter[2];	/* Multicast hash filter */
	int rx_mode;
	u32 tmp = 0;

	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		netif_notice(tp, link, dev, "Promiscuous mode enabled\n");
		rx_mode =
			AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
			AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if ((netdev_mc_count(dev) > multicast_filter_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct netdev_hw_addr *ha;

		rx_mode = AcceptBroadcast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		netdev_for_each_mc_addr(ha, dev) {
			int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
			rx_mode |= AcceptMulticast;
		}
	}

	if (dev->features & NETIF_F_RXALL)
		rx_mode |= (AcceptErr | AcceptRunt);

	tmp = (RTL_R16(RxConfig) & ~RX_CONFIG_ACCEPT_MASK) | rx_mode;

	if (tp->mac_version == RTL_GIGA_MAC_VER_01) {
		u32 data = mc_filter[0];

		mc_filter[0] = swab32(mc_filter[1]);
		mc_filter[1] = swab32(data);
	}

	if (dev->flags & IFF_PROMISC)
		PacketFillDefault(tp, 1);
	else
		PacketFillDefault(tp, 0);

	RTL_W32(MAR0 + 4, 0xffffffff);
	RTL_W32(MAR0 + 0, 0xffffffff);

	RTL_W16(RxConfig, tmp);
}

static void rtl_hw_start_8168(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	rtl_set_rx_tx_config_registers(tp);

	RTL_W8(ChipCmd, 0x0);

	/* Accept all multicast */
	RTL_W32(MAR0 + 0, 0xffffffff);
	RTL_W32(MAR0 + 4, 0xffffffff);

	RTL_W16(CPlusCmd, 0x0021);
	RTL_W32(RxConfig, 0x05f3028E);

	RTL_W16(IntrMask, 0x0000);
	RTL_W16(IntrStatus, 0xFFFF);
	RTL_W16(IntrMask, 0xFFFF);

	while (RTL_R16(ChipCmd) & 0x300) {
		netif_err(tp, drv, dev, " waiting \n");
	}

	RTL_W8(ChipCmd, 0x0C);
}

static int link_status(void *data)
{
	struct net_device *dev = data;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	set_user_nice(current, 5);
	printk("[Ethernet] Watch link status change.\n");

	while (!kthread_should_stop()) {
		wait_event_interruptible(tp->thr_wait, kthread_should_stop() || tp->link_chg);
		if (kthread_should_stop()) {
			break;
		} else if (!tp->link_chg)
			continue;
		else
			tp->link_chg = 0;

		/* Send uevent about link status*/
		if (tp->link_ok(ioaddr)) {
			kobject_uevent(&tp->dev->dev.kobj, KOBJ_LINKUP);
		} else{
			kobject_uevent(&tp->dev->dev.kobj, KOBJ_LINKDOWN);
		}
	}

	do_exit(0);
}

static int isolate_status(void *data)
{
	struct net_device *dev = data;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	set_user_nice(current, 5);
	printk("[Ethernet] Watch isolate status change.\n");

	while (!kthread_should_stop()) {
		wait_event_interruptible(tp->thr_wait_iso, kthread_should_stop() || tp->isolate_chg);
		if (kthread_should_stop()) {
			break;
		} else if (!tp->isolate_chg)
			continue;
		else
			tp->isolate_chg = 0;

		/* Send uevent about isolate status*/
		if (RTL_R16(ExtIntrStatus) & IsolateInt) {
			kobject_uevent(&tp->dev->dev.kobj, KOBJ_ISOLATE);
			RTL_W16(ExtIntrStatus, IsolateInt);
		}
	}

	do_exit(0);
}



static int rtl8169_change_mtu(struct net_device *dev, int new_mtu)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	if (new_mtu < ETH_ZLEN ||
		new_mtu > rtl_chip_infos[tp->mac_version].jumbo_max)
		return -EINVAL;


	dev->mtu = new_mtu;
	netdev_update_features(dev);

	return 0;
}

static inline void rtl8169_make_unusable_by_asic(struct RxDesc *desc)
{
	desc->addr = cpu_to_le64(0x0badbadbadbadbadull);
	desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void rtl8169_free_rx_databuff(struct rtl8169_private *tp,
					 void **data_buff, struct RxDesc *desc)
{
	dma_unmap_single(&tp->pdev->dev, le64_to_cpu(desc->addr), rx_buf_sz,
			 DMA_FROM_DEVICE);

	kfree(*data_buff);
	*data_buff = NULL;
	rtl8169_make_unusable_by_asic(desc);
}

static inline void rtl8169_mark_to_asic(struct RxDesc *desc, u32 rx_buf_sz)
{
	u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

	desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void rtl8169_map_to_asic(struct RxDesc *desc, dma_addr_t mapping,
					   u32 rx_buf_sz)
{
	desc->addr = cpu_to_le64(mapping);
	wmb();
	rtl8169_mark_to_asic(desc, rx_buf_sz);
}

static inline void *rtl8169_align(void *data)
{
	return (void *)ALIGN((long)data, 16);
}

static struct sk_buff *rtl8169_alloc_rx_data(struct rtl8169_private *tp,
						 struct RxDesc *desc)
{
	void *data;
	dma_addr_t mapping;
	struct device *d = &tp->pdev->dev;
	struct net_device *dev = tp->dev;
	int node = dev->dev.parent ? dev_to_node(dev->dev.parent) : -1;

	data = kmalloc_node(rx_buf_sz, GFP_KERNEL, node);
	if (!data)
		return NULL;

	if (rtl8169_align(data) != data) {
		kfree(data);
		data = kmalloc_node(rx_buf_sz + 15, GFP_KERNEL, node);
		if (!data)
			return NULL;
	}

	mapping = dma_map_single(d, rtl8169_align(data), rx_buf_sz,
				 DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(d, mapping))) {
		if (net_ratelimit())
			netif_err(tp, drv, tp->dev, "Failed to map RX DMA!\n");
		goto err_out;
	}

	rtl8169_map_to_asic(desc, mapping, rx_buf_sz);
	return data;

err_out:
	kfree(data);
	return NULL;
}

static void rtl8169_rx_clear(struct rtl8169_private *tp)
{
	unsigned int i;

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (tp->Rx_databuff[i]) {
			rtl8169_free_rx_databuff(tp, tp->Rx_databuff + i,
						tp->RxDescArray + i);
		}
	}
}

static inline void rtl8169_mark_as_last_descriptor(struct RxDesc *desc)
{
	desc->opts1 |= cpu_to_le32(RingEnd);
}

static int rtl8169_rx_fill(struct rtl8169_private *tp)
{
	unsigned int i;

	for (i = 0; i < NUM_RX_DESC; i++) {
		void *data;

		if (tp->Rx_databuff[i])
			continue;

		data = rtl8169_alloc_rx_data(tp, tp->RxDescArray + i);
		if (!data) {
			rtl8169_make_unusable_by_asic(tp->RxDescArray + i);
			goto err_out;
		}
		tp->Rx_databuff[i] = data;
	}

	rtl8169_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);
	return 0;

err_out:
	rtl8169_rx_clear(tp);
	return -ENOMEM;
}

static int rtl8169_init_ring(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_init_ring_indexes(tp);

	memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
	memset(tp->Rx_databuff, 0x0, NUM_RX_DESC * sizeof(void *));

	return rtl8169_rx_fill(tp);
}

static void rtl8169_unmap_tx_skb(struct device *d, struct ring_info *tx_skb,
				 struct TxDesc *desc)
{
	unsigned int len = tx_skb->len;

	dma_unmap_single(d, le64_to_cpu(desc->addr), len, DMA_TO_DEVICE);

	desc->opts1 = 0x00;
	desc->opts2 = 0x00;
	desc->addr = 0x00;
	tx_skb->len = 0;
}

static void rtl8169_tx_clear_range(struct rtl8169_private *tp, u32 start,
				   unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int entry = (start + i) % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		unsigned int len = tx_skb->len;

		if (len) {
			struct sk_buff *skb = tx_skb->skb;

			rtl8169_unmap_tx_skb(&tp->pdev->dev, tx_skb,
						 tp->TxDescArray + entry);
			if (skb) {
				tp->dev->stats.tx_dropped++;
				dev_kfree_skb(skb);
				tx_skb->skb = NULL;
			}
		}
	}
}

static void rtl8169_tx_clear(struct rtl8169_private *tp)
{
	rtl8169_tx_clear_range(tp, tp->dirty_tx, NUM_TX_DESC);
	tp->cur_tx = tp->dirty_tx = 0;
}

static void rtl_reset_work(struct rtl8169_private *tp)
{
	struct net_device *dev = tp->dev;
	int i;

	napi_disable(&tp->napi);
	netif_stop_queue(dev);
	synchronize_sched();

	rtl8169_hw_reset(tp);

	for (i = 0; i < NUM_RX_DESC; i++)
		rtl8169_mark_to_asic(tp->RxDescArray + i, rx_buf_sz);

	rtl8169_tx_clear(tp);
	rtl8169_init_ring_indexes(tp);

	for (i = 0; i < NUM_TX_DESC; i++) {
		tp->TxDescArray[i].opts1 = 0;
		tp->TxDescArray[i].opts2 = 0;
		tp->TxDescArray[i].addr = 0;
	}

	napi_enable(&tp->napi);
	rtl_hw_start(dev);
	netif_wake_queue(dev);
	rtl8169_check_link_status(dev, tp, tp->mmio_addr);
}

static void rtl8169_tx_timeout(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i = 0;
	u32 status1 = 0, status2 = 0;
	u64 addr;

	for (i = 0; i < 256; i = i+4) {
		netif_err(tp, drv, dev, "offset %x value %x\n", i, RTL_R32(i));
	}

	netif_err(tp, drv, dev, "dirty %x cur_tx %x\n", tp->dirty_tx, tp->cur_tx);

	for (i = 0; i < NUM_TX_DESC; i++) {
		status1 = le32_to_cpu(tp->TxDescArray[i].opts1);
		status2 = le32_to_cpu(tp->TxDescArray[i].opts2);
		addr =  le64_to_cpu(tp->TxDescArray[i].addr);
		netif_err(tp, drv, dev, "rtl8169_tx_timeout entry %x status1=%x  status2=%x addr=%lx !\n", i, status1, status2, (long unsigned int)addr);
	}

	rtl_schedule_task(tp, RTL_FLAG_TASK_RESET_PENDING);
}

static int rtl8169_xmit_frags(struct rtl8169_private *tp, struct sk_buff *skb,
				  u32 *opts)
{
	struct skb_shared_info *info = skb_shinfo(skb);
	unsigned int cur_frag, entry;
	struct TxDesc *uninitialized_var(txd);
	struct device *d = &tp->pdev->dev;

	entry = tp->cur_tx;
	for (cur_frag = 0; cur_frag < info->nr_frags; cur_frag++) {
		const skb_frag_t *frag = info->frags + cur_frag;
		dma_addr_t mapping;
		u32 status, len;
		void *addr;

		entry = (entry + 1) % NUM_TX_DESC;

		txd = tp->TxDescArray + entry;
		len = skb_frag_size(frag);
		addr = skb_frag_address(frag);
		mapping = dma_map_single(d, addr, len, DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(d, mapping))) {
			if (net_ratelimit())
				netif_err(tp, drv, tp->dev,
					  "Failed to map TX fragments DMA!\n");
			goto err_out;
		}

		/* Anti gcc 2.95.3 bugware (sic) */
		status = opts[0] | len |
			(RingEnd * !((entry + 1) % NUM_TX_DESC));

		txd->opts1 = cpu_to_le32(status);
		txd->opts2 = cpu_to_le32(opts[1]);
		txd->addr = cpu_to_le64(mapping);

		tp->tx_skb[entry].len = len;
	}

	if (cur_frag) {
		tp->tx_skb[entry].skb = skb;
		txd->opts1 |= cpu_to_le32(LastFrag);
	}

	return cur_frag;

err_out:
	rtl8169_tx_clear_range(tp, tp->cur_tx + 1, cur_frag);
	return -EIO;
}

static bool rtl_skb_pad(struct sk_buff *skb)
{
	if (skb_padto(skb, ETH_ZLEN))
		return false;
	skb_put(skb, ETH_ZLEN - skb->len);
	return true;
}

static bool rtl_test_hw_pad_bug(struct rtl8169_private *tp, struct sk_buff *skb)
{
	return skb->len < ETH_ZLEN && tp->mac_version != RTL_GIGA_MAC_VER_01;
}

static inline bool rtl8169_tso_csum(struct rtl8169_private *tp,
					struct sk_buff *skb, u32 *opts)
{
	const struct rtl_tx_desc_info *info = tx_desc_info + tp->txd_version;
	u32 mss = skb_shinfo(skb)->gso_size;
	int offset = info->opts_offset;

	if (mss) {
		opts[0] |= TD_LSO;
		opts[offset] |= min(mss, TD_MSS_MAX) << info->mss_shift;
	} else if (skb->ip_summed == CHECKSUM_PARTIAL) {
		const struct iphdr *ip = ip_hdr(skb);

		if (unlikely(rtl_test_hw_pad_bug(tp, skb)))
			return skb_checksum_help(skb) == 0 && rtl_skb_pad(skb);

		if (ip->protocol == IPPROTO_TCP)
			opts[offset] |= info->checksum.tcp;
		else if (ip->protocol == IPPROTO_UDP)
			opts[offset] |= info->checksum.udp;
		else
			WARN_ON_ONCE(1);
	} else {
		if (unlikely(rtl_test_hw_pad_bug(tp, skb)))
			return rtl_skb_pad(skb);
	}
	return true;
}

static netdev_tx_t rtl8169_start_xmit(struct sk_buff *skb,
					  struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned int entry = tp->cur_tx % NUM_TX_DESC;
	struct TxDesc *txd = tp->TxDescArray + entry;
	void __iomem *ioaddr = tp->mmio_addr;
	struct device *d = &tp->pdev->dev;
	dma_addr_t mapping;
	u32 status, len;
	u32 opts[2];
	int frags;

	if (unlikely(!TX_FRAGS_READY_FOR(tp, skb_shinfo(skb)->nr_frags))) {
		netif_err(tp, drv, dev, "BUG! Tx Ring full when queue awake!\n");
		goto err_stop_0;
	}

	if (unlikely(le32_to_cpu(txd->opts1) & DescOwn))
		goto err_stop_0;

	opts[1] = cpu_to_le32(rtl8169_tx_vlan_tag(skb));
	opts[0] = DescOwn;

	if (!rtl8169_tso_csum(tp, skb, opts))
		goto err_update_stats;

	len = skb_headlen(skb);
	mapping = dma_map_single(d, skb->data, len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(d, mapping))) {
		if (net_ratelimit())
			netif_err(tp, drv, dev, "Failed to map TX DMA!\n");
		goto err_dma_0;
	}

	tp->tx_skb[entry].len = len;
	txd->addr = cpu_to_le64(mapping);

	frags = rtl8169_xmit_frags(tp, skb, opts);
	if (frags < 0)
		goto err_dma_1;
	else if (frags)
		opts[0] |= FirstFrag;
	else {
		opts[0] |= FirstFrag | LastFrag;
		tp->tx_skb[entry].skb = skb;
	}

	txd->opts2 = cpu_to_le32(opts[1]);

	skb_tx_timestamp(skb);

	wmb();

	/* Anti gcc 2.95.3 bugware (sic) */
	status = opts[0] | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));
	txd->opts1 = cpu_to_le32(status);

	tp->cur_tx += frags + 1;

	wmb();

	RTL_W8(TxPoll, NPQ);

	mmiowb();

	if (!TX_FRAGS_READY_FOR(tp, MAX_SKB_FRAGS_OOB)) {
		/* Avoid wrongly optimistic queue wake-up: rtl_tx thread must
		 * not miss a ring update when it notices a stopped queue.
		 */
		smp_wmb();
		netif_stop_queue(dev);
		/* Sync with rtl_tx:
		 * - publish queue status and cur_tx ring index (write barrier)
		 * - refresh dirty_tx ring index (read barrier).
		 * May the current thread have a pessimistic view of the ring
		 * status and forget to wake up queue, a racing rtl_tx thread
		 * can't.
		 */
		smp_mb();
		if (TX_FRAGS_READY_FOR(tp, MAX_SKB_FRAGS_OOB))
			netif_wake_queue(dev);
	}

	return NETDEV_TX_OK;

err_dma_1:
	rtl8169_unmap_tx_skb(d, tp->tx_skb + entry, txd);
err_dma_0:
	dev_kfree_skb(skb);
err_update_stats:
	dev->stats.tx_dropped++;
	return NETDEV_TX_OK;

err_stop_0:
	netif_stop_queue(dev);
	dev->stats.tx_dropped++;
	return NETDEV_TX_BUSY;
}

static void rtl_tx(struct net_device *dev, struct rtl8169_private *tp)
{
	unsigned int dirty_tx, tx_left;

	dirty_tx = tp->dirty_tx;
	smp_rmb();
	tx_left = tp->cur_tx - dirty_tx;

	while (tx_left > 0) {
		unsigned int entry = dirty_tx % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		u32 status;

		rmb();
		status = le32_to_cpu(tp->TxDescArray[entry].opts1);
		if (status & DescOwn)
			break;

		rtl8169_unmap_tx_skb(&tp->pdev->dev, tx_skb,
					 tp->TxDescArray + entry);
		if (status & LastFrag) {
			u64_stats_update_begin(&tp->tx_stats.syncp);
			tp->tx_stats.packets++;
			tp->tx_stats.bytes += tx_skb->skb->len;
			u64_stats_update_end(&tp->tx_stats.syncp);
			dev_kfree_skb(tx_skb->skb);
			tx_skb->skb = NULL;
		}
		dirty_tx++;
		tx_left--;
	}

	if (tp->dirty_tx != dirty_tx) {
		tp->dirty_tx = dirty_tx;
		/* Sync with rtl8169_start_xmit:
		 * - publish dirty_tx ring index (write barrier)
		 * - refresh cur_tx ring index and queue status (read barrier)
		 * May the current thread miss the stopped queue condition,
		 * a racing xmit thread can only have a right view of the
		 * ring status.
		 */
		smp_mb();
		if (netif_queue_stopped(dev) &&
			TX_FRAGS_READY_FOR(tp, MAX_SKB_FRAGS_OOB)) {
			netif_wake_queue(dev);
		}
		/*
		 * 8168 hack: TxPoll requests are lost when the Tx packets are
		 * too close. Let's kick an extra TxPoll request when a burst
		 * of start_xmit activity is detected (if it is not detected,
		 * it is slow enough). -- FR
		 */
		if (tp->cur_tx != dirty_tx) {
			void __iomem *ioaddr = tp->mmio_addr;

			RTL_W8(TxPoll, NPQ);
		}
	}
}

static inline int rtl8169_fragmented_frame(u32 status)
{
	return (status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag);
}

static inline void rtl8169_rx_csum(struct sk_buff *skb, u32 opts1)
{
	u32 status = opts1 & RxProtoMask;

	if (((status == RxProtoTCP) && !(opts1 & TCPFail)) ||
		((status == RxProtoUDP) && !(opts1 & UDPFail)))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		skb_checksum_none_assert(skb);
}

static struct sk_buff *rtl8169_try_rx_copy(void *data,
					   struct rtl8169_private *tp,
					   int pkt_size,
					   dma_addr_t addr)
{
	struct sk_buff *skb;
	struct device *d = &tp->pdev->dev;

	data = rtl8169_align(data);
	dma_sync_single_for_cpu(d, addr, pkt_size, DMA_FROM_DEVICE);
	prefetch(data);
	skb = netdev_alloc_skb_ip_align(tp->dev, pkt_size);
	if (skb)
		memcpy(skb->data, data, pkt_size);

	dma_sync_single_for_device(d, addr, pkt_size, DMA_FROM_DEVICE);

	return skb;
}

static int rtl_rx(struct net_device *dev, struct rtl8169_private *tp, u32 budget)
{
	unsigned int cur_rx, rx_left;
	unsigned int count;

	cur_rx = tp->cur_rx;

	for (rx_left = min(budget, NUM_RX_DESC); rx_left > 0; rx_left--, cur_rx++) {
		unsigned int entry = cur_rx % NUM_RX_DESC;
		struct RxDesc *desc = tp->RxDescArray + entry;
		u32 status, crc;

		rmb();
		status = le32_to_cpu(desc->opts1) & tp->opts1_mask;
		if (status & DescOwn)
			break;
		crc = le32_to_cpu(desc->opts2);
		if (unlikely(crc & RxCRC)) {
			netif_info(tp, rx_err, dev, "Rx ERROR. status = %08x crc = %08x\n",
				   status, crc);
			dev->stats.rx_errors++;
			dev->stats.rx_crc_errors++;
			if (dev->features & NETIF_F_RXALL)
				goto process_pkt;
		} else {
			struct sk_buff *skb;
			dma_addr_t addr;
			int pkt_size;

process_pkt:
			addr = le64_to_cpu(desc->addr);
			if (likely(!(dev->features & NETIF_F_RXFCS)))
				pkt_size = (status & 0x00003fff) - 4;
			else
				pkt_size = status & 0x00003fff;

			/*
			 * The driver does not support incoming fragmented
			 * frames. They are seen as a symptom of over-mtu
			 * sized frames.
			 */
			if (unlikely(rtl8169_fragmented_frame(status))) {
				dev->stats.rx_dropped++;
				dev->stats.rx_length_errors++;
				goto release_descriptor;
			}

			skb = rtl8169_try_rx_copy(tp->Rx_databuff[entry],
						  tp, pkt_size, addr);
			if (!skb) {
				dev->stats.rx_dropped++;
				goto release_descriptor;
			}

			rtl8169_rx_csum(skb, status);
			skb_put(skb, pkt_size);
			skb->protocol = eth_type_trans(skb, dev);

			rtl8169_rx_vlan_tag(desc, skb);

			napi_gro_receive(&tp->napi, skb);

			u64_stats_update_begin(&tp->rx_stats.syncp);
			tp->rx_stats.packets++;
			tp->rx_stats.bytes += pkt_size;
			u64_stats_update_end(&tp->rx_stats.syncp);
		}
release_descriptor:
		desc->opts2 = 0;
		wmb();
		rtl8169_mark_to_asic(desc, rx_buf_sz);
	}

	count = cur_rx - tp->cur_rx;
	tp->cur_rx = cur_rx;

	return count;
}

static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct rtl8169_private *tp = netdev_priv(dev);
	int handled = 0;
	u16 status;

	status = rtl_get_events(tp);
	if (status && status != 0xffff) {
		status &= RTL_EVENT_NAPI | tp->event_slow;
		if (status) {
			handled = 1;

			rtl_irq_disable(tp);
			napi_schedule(&tp->napi);
		}
	}
	return IRQ_RETVAL(handled);
}

/*
 * Workqueue context.
 */
static void rtl_slow_event_work(struct rtl8169_private *tp)
{
	struct net_device *dev = tp->dev;
	void __iomem *ioaddr = tp->mmio_addr;
	u16 status;

	status = rtl_get_events(tp) & tp->event_slow;
	rtl_ack_events(tp, status);

	if (status & LinkChg) {
		__rtl8169_check_link_status(dev, tp, tp->mmio_addr, true);

		/* Add for link status change */
		tp->link_chg = 1;
		wake_up(&tp->thr_wait);

		/* prevet ALDPS enter MAC Powercut Tx/Rx disable*/
		/* use MAC reset to set counter to offset 0 */
		if (tp->link_ok(ioaddr)) {
		   rtl_schedule_task(tp, RTL_FLAG_TASK_RESET_PENDING);
		} else{
		   /*mod_timer(&tp->timer, jiffies + RTL8169_PHY_TIMEOUT);*/
		}
	}

	rtl_irq_enable_all(tp);
	/* enable isolate interrupt */
	RTL_W16(ExtIntrMask, IsolateInt);
}

static void rtl_task(struct work_struct *work)
{
	static const struct {
		int bitnr;
		void (*action)(struct rtl8169_private *);
	} rtl_work[] = {
		/* XXX - keep rtl_slow_event_work() as first element. */
		{ RTL_FLAG_TASK_SLOW_PENDING,	rtl_slow_event_work },
		{ RTL_FLAG_TASK_RESET_PENDING,	rtl_reset_work },
		{ RTL_FLAG_TASK_PHY_PENDING,	rtl_phy_work }
	};
	struct rtl8169_private *tp =
		container_of(work, struct rtl8169_private, wk.work);
	struct net_device *dev = tp->dev;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);

	if (!netif_running(dev) ||
		!test_bit(RTL_FLAG_TASK_ENABLED, tp->wk.flags))
		goto out_unlock;

	for (i = 0; i < ARRAY_SIZE(rtl_work); i++) {
		bool pending;

		pending = test_and_clear_bit(rtl_work[i].bitnr, tp->wk.flags);
		if (pending)
			rtl_work[i].action(tp);
	}

out_unlock:
	spin_unlock_irqrestore(&tp->lock, flags);
}

void (*func)(void *);

static void rtl_swint(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u8 status, i;

	status = RTL_R8(SWISR);
	if (status == 0x05) {/* inband inform OOB that driver is ready to service */
		clearAllPFRule(tp);
		SetIBBasicRule(tp);
		rtl_driver_ready_enable(tp);
		printk(KERN_INFO "inband inform OOB that driver is ready to service \n");
	} else if (status == 0x06) {/* inband inform OOB that driver is about to leave */
		hwPFInit(tp);
		rtl_driver_ready_disable(tp);
		printk(KERN_INFO "inband inform OOB that driver is about to leave \n");
	}

	status = RTL_R8(SWISR);
	for (i = 0; i < NumOfSwisr; i++) {
		if (record[i].Valid == 1) {
			if (record[i].Value == status || record[i].Value == 0xFF ) {
				func = record[i].func;
				func(record[i].context);
				printk(KERN_INFO "swisr %x callback \n", record[i].Value);
			}
		}
	}

	RTL_W8(SWISR, 0xFF);
}

static int rtl8169_poll(struct napi_struct *napi, int budget)
{
	struct rtl8169_private *tp = container_of(napi, struct rtl8169_private, napi);
	struct net_device *dev = tp->dev;
	void __iomem *ioaddr = tp->mmio_addr;
	u16 enable_mask = RTL_EVENT_NAPI | tp->event_slow;
	int work_done = 0;
	u16 status;

	status = rtl_get_events(tp);
	rtl_ack_events(tp, status & ~tp->event_slow);

	if (status & RTL_EVENT_NAPI_RX)
		work_done = rtl_rx(dev, tp, (u32) budget);

	if (status & RTL_EVENT_NAPI_TX)
		rtl_tx(dev, tp);

	if (status & SWInt)
		rtl_swint(tp);

	if (status & tp->event_slow) {
		enable_mask &= ~tp->event_slow;

		rtl_schedule_task(tp, RTL_FLAG_TASK_SLOW_PENDING);
	}

	if (work_done < budget) {
		napi_complete(napi);

		rtl_irq_enable(tp, enable_mask);
		RTL_W16(ExtIntrMask, IsolateInt);
		mmiowb();
	}

	return work_done;
}

static void rtl8169_down(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	del_timer_sync(&tp->timer);

	napi_disable(&tp->napi);
	netif_stop_queue(dev);

	rtl8169_hw_reset(tp);
	/*
	 * At this point device interrupts can not be enabled in any function,
	 * as netif_running is not true (rtl8169_interrupt, rtl8169_reset_task)
	 * and napi is disabled (rtl8169_poll).
	 */

	/* Give a racing hard_start_xmit a few cycles to complete. */
	synchronize_sched();

	rtl8169_tx_clear(tp);

	rtl8169_rx_clear(tp);

}

static int rtl8169_close(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct platform_device *pdev = tp->pdev;

	/* Update counters before going down */
	rtl8169_update_counters(dev);

	rtl_lock_work(tp);
	clear_bit(RTL_FLAG_TASK_ENABLED, tp->wk.flags);

	rtl8169_down(dev);
	rtl_unlock_work(tp);

	free_irq(dev->irq, dev);

	dma_free_coherent(&pdev->dev, R8169_RX_RING_BYTES, tp->RxDescArray,
			  tp->RxPhyAddr);
	dma_free_coherent(&pdev->dev, R8169_TX_RING_BYTES, tp->TxDescArray,
			  tp->TxPhyAddr);
	tp->TxDescArray = NULL;
	tp->RxDescArray = NULL;

	return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void rtl8169_netpoll(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_interrupt(tp->pdev->irq, dev);
}
#endif

static int rtl_open(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct platform_device *pdev = tp->pdev;
	int retval = -ENOMEM;

	/*
	 * Rx and Tx descriptors needs 256 bytes alignment.
	 * dma_alloc_coherent provides more.
	 */
	tp->TxDescArray = dma_alloc_coherent(&pdev->dev, R8169_TX_RING_BYTES,
						 &tp->TxPhyAddr, GFP_KERNEL);
	if (!tp->TxDescArray)
		goto err_pm_runtime_put;

	tp->RxDescArray = dma_alloc_coherent(&pdev->dev, R8169_RX_RING_BYTES,
						 &tp->RxPhyAddr, GFP_KERNEL);
	if (!tp->RxDescArray)
		goto err_free_tx_0;

	memset(tp->TxDescArray, 0, R8169_TX_RING_BYTES);
	memset(tp->RxDescArray, 0, R8169_RX_RING_BYTES);

	retval = rtl8169_init_ring(dev);
	if (retval < 0)
		goto err_free_rx_1;

	INIT_WORK(&tp->wk.work, rtl_task);

	smp_mb();

	retval = request_irq(dev->irq, rtl8169_interrupt,
				 (tp->features & RTL_FEATURE_MSI) ? 0 : IRQF_SHARED,
				 dev->name, dev);
	if (retval < 0)
		goto err_release_fw_2;

	rtl_lock_work(tp);

	set_bit(RTL_FLAG_TASK_ENABLED, tp->wk.flags);

	napi_enable(&tp->napi);

	hwPFInit(tp);

	rtl_hw_start(dev);

	netif_start_queue(dev);

	rtl_unlock_work(tp);

	tp->saved_wolopts = 0;

	rtl8169_check_link_status(dev, tp, ioaddr);

	/* start check timer */
	mod_timer(&tp->timer, jiffies + RTL8169_PHY_TIMEOUT);

out:
	return retval;

err_release_fw_2:
	rtl8169_rx_clear(tp);
err_free_rx_1:
	dma_free_coherent(&pdev->dev, R8169_RX_RING_BYTES, tp->RxDescArray,
			  tp->RxPhyAddr);
	tp->RxDescArray = NULL;
err_free_tx_0:
	dma_free_coherent(&pdev->dev, R8169_TX_RING_BYTES, tp->TxDescArray,
			  tp->TxPhyAddr);
	tp->TxDescArray = NULL;
err_pm_runtime_put:
	goto out;
}

static struct rtnl_link_stats64 *
rtl8169_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned int start;

	do {
		start = u64_stats_fetch_begin_irq(&tp->rx_stats.syncp);
		stats->rx_packets = tp->rx_stats.packets;
		stats->rx_bytes	= tp->rx_stats.bytes;
	} while (u64_stats_fetch_retry_irq(&tp->rx_stats.syncp, start));

	do {
		start = u64_stats_fetch_begin_irq(&tp->tx_stats.syncp);
		stats->tx_packets = tp->tx_stats.packets;
		stats->tx_bytes	= tp->tx_stats.bytes;
	} while (u64_stats_fetch_retry_irq(&tp->tx_stats.syncp, start));

	stats->rx_dropped	= dev->stats.rx_dropped;
	stats->tx_dropped	= dev->stats.tx_dropped;
	stats->rx_length_errors = dev->stats.rx_length_errors;
	stats->rx_errors	= dev->stats.rx_errors;
	stats->rx_crc_errors	= dev->stats.rx_crc_errors;
	stats->rx_fifo_errors	= dev->stats.rx_fifo_errors;
	stats->rx_missed_errors = dev->stats.rx_missed_errors;

	return stats;
}

static void rtl8169_net_suspend(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	if (!netif_running(dev))
		return;

	netif_device_detach(dev);
	netif_stop_queue(dev);

	rtl_lock_work(tp);
	napi_disable(&tp->napi);
	clear_bit(RTL_FLAG_TASK_ENABLED, tp->wk.flags);
	rtl_unlock_work(tp);

	rtl8169_hw_reset(tp);
}

#ifdef CONFIG_PM

static int rtl8169_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rtl8169_private *tp = netdev_priv(ndev);
	void __iomem *ioaddr = tp->mmio_addr;

	printk(KERN_ERR "[RTK_ETN] Enter %s\n", __func__);

	rtl8169_net_suspend(ndev);

	/* FIXME: disable LED, current solution is switch pad to GPIO input */
	/* writel(readl(tp->mmio_clkaddr + 0x310) & ~0x3C000000, (tp->mmio_clkaddr + 0x310)); */

	if (RTK_PM_STATE == PM_SUSPEND_STANDBY) {
		printk(KERN_ERR "[RTK_ETN] %s Idle mode\n", __func__);
	} else{
		printk(KERN_ERR "[RTK_ETN] %s Suspend mode\n", __func__);
	}

	printk(KERN_ERR "[RTK_ETN] Exit %s\n", __func__);

	return 0;
}

static void __rtl8169_resume(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	u32 val;

	netif_device_attach(dev);

	rtl_rar_set(tp, tp->dev->dev_addr);

	rtl_lock_work(tp);
	napi_enable(&tp->napi);
	set_bit(RTL_FLAG_TASK_ENABLED, tp->wk.flags);
	rtl_unlock_work(tp);

	rtl_schedule_task(tp, RTL_FLAG_TASK_RESET_PENDING);
}

static int rtl8169_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rtl8169_private *tp = netdev_priv(ndev);
	void __iomem *ioaddr = tp->mmio_addr;

	printk(KERN_ERR "[RTK_ETN] Enter %s\n", __func__);

	if (RTK_PM_STATE == PM_SUSPEND_STANDBY) {
		printk(KERN_ERR "[RTK_ETN] %s Idle mode\n", __func__);
	} else{
		printk(KERN_ERR "[RTK_ETN] %s Suspend mode\n", __func__);
	}

	if (netif_running(ndev))
		__rtl8169_resume(ndev);

	rtl8169_init_phy(ndev, tp);

	printk(KERN_ERR "[RTK_ETN] Exit %s\n", __func__);

	return 0;
}

static __maybe_unused int rtl8169_runtime_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rtl8169_private *tp = netdev_priv(ndev);

	if (!tp->TxDescArray)
		return 0;

	rtl_lock_work(tp);
	tp->saved_wolopts = __rtl8169_get_wol(tp);
	__rtl8169_set_wol(tp, WAKE_ANY);
	rtl_unlock_work(tp);

	rtl8169_net_suspend(ndev);

	return 0;
}

static __maybe_unused int rtl8169_runtime_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rtl8169_private *tp = netdev_priv(ndev);

	if (!tp->TxDescArray)
		return 0;

	rtl_lock_work(tp);
	__rtl8169_set_wol(tp, tp->saved_wolopts);
	tp->saved_wolopts = 0;
	rtl_unlock_work(tp);

	rtl8169_init_phy(ndev, tp);

	__rtl8169_resume(ndev);

	return 0;
}

static __maybe_unused int rtl8169_runtime_idle(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rtl8169_private *tp = netdev_priv(ndev);

	return tp->TxDescArray ? -EBUSY : 0;
}

static const struct dev_pm_ops rtl8169_pm_ops = {
	.suspend		= rtl8169_suspend,
	.resume			= rtl8169_resume,
	.freeze			= rtl8169_suspend,
	.thaw			= rtl8169_resume,
	.poweroff		= rtl8169_suspend,
	.restore		= rtl8169_resume,
	.runtime_suspend	= rtl8169_runtime_suspend,
	.runtime_resume		= rtl8169_runtime_resume,
	.runtime_idle		= rtl8169_runtime_idle,
};

#define RTL8169_PM_OPS	(&rtl8169_pm_ops)

#else /* !CONFIG_PM */

#define RTL8169_PM_OPS	NULL

#endif /* !CONFIG_PM */

#ifdef RTL_OOB_PROC
static int rtl8168oob_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, PDE_DATA(inode));
}

static ssize_t read_proc(struct file *file, char *buf, size_t count, loff_t *offp)
{
	struct net_device *dev = ((struct seq_file *)file->private_data)->private;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	ssize_t len = 0;
	u8 val;
	static int finished;

	if (finished) {
		finished = 0;
		return 0;
	}

	printk(KERN_INFO "read procfs ethernet OOBLANWAKE Status = %x \n", RTL_R32(OOBLANWAKEStatus));
	finished = 1;
	val = RTL_R8(OOBLANWAKEStatus);
	val &= 0x01;

	len = sprintf(buf, "%x\n", val);
	return len;
}

static void clear_swisr_table(void)
{
	u8 i;
	for (i = 0; i < NumOfSwisr; i++)
		record[i].Valid = 0;

	printk(KERN_INFO "clear swisr table\n");
}

int register_swisr(u8 num, int (*callback)(void *), void (*context))
{
	u8 i;

	for (i = 0; i < NumOfSwisr; i++) {
		if (record[i].Valid == 0) {
			record[i].Valid = 1;
			record[i].Value = num;
			record[i].func = callback;
			record[i].context = context;
			printk(KERN_INFO "register_swisr swisr=%x %x %x \n", record[i].Value, (unsigned int)record[i].func, (unsigned int)record[i].context);
			break;
		}
	}

	if (i == NumOfSwisr) {
		printk(KERN_INFO "The swisr table is full, can't regiester the swisr\n");
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL_GPL(register_swisr);

int unregister_swisr(u8 num, int (*callback)(void *), void (*context))
{
	u8 i;

	for (i = 0; i < NumOfSwisr; i++) {
		if (record[i].Valid == 1) {
			if ((record[i].Value == num) && (record[i].func == callback) && (record[i].context == context)) {
				record[i].Valid = 0;
				printk(KERN_INFO "unregister_swisr swisr=%x \n",  record[i].Value);
				break;
			}
		}
	}

	if (i == NumOfSwisr) {
		printk(KERN_INFO "unregister_swisr failed, can't find the swisr value in the table\n");
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL_GPL(unregister_swisr);

static ssize_t write_proc(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct net_device *dev = ((struct seq_file *)file->private_data)->private;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	char tmp[32];
	u8 val, num;
	u32 lanwake;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {
		num = sscanf(tmp, "%x", &lanwake);
		if ((lanwake == 1) || (lanwake == 0)) {
			val = RTL_R8(OOBLANWAKE);
			val &= 0xFE;
			val |= lanwake;
			RTL_W8(OOBLANWAKE, val);
			printk(KERN_INFO "write procfs ethernet OOBLANWAKE = %x \n", val);

			if (lanwake == 1)
				RTL_W8(OOBLANWAKEStatus, 0x01);
		} else
			printk(KERN_INFO "write procfs not support value  = %x \n", lanwake);
	}
	return count;
}

static const struct file_operations proc_fops = {
.open = rtl8168oob_proc_open,
.read = read_proc,
.write = write_proc,
};

static int proc_get_isolatestatus(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u8 byte_rd;

	byte_rd = RTL_R8(ISOLATEstatus);
	byte_rd &= 0x01;

	seq_printf(m, "%x\n", byte_rd);

	return 0;
}

static int proc_get_registers(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	int i, n, max = 0x400;
	u8 byte_rd;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	seq_puts(m, "\nDump OOB MAC Registers\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 16 && n < max; i++, n++) {
			byte_rd = readb(ioaddr + n);
			seq_printf(m, "%02x ", byte_rd);
		}
	}

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_tcamrule(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u16 tmp;
	int i, j, k;
	u32 rule_type;

	tmp = RTL_R16(0x2F0);

	if (tmp == 0) {
		seq_puts(m, "TCAM RULES ARE DISABLED\n");
	} else {
		for (i = 0; i < 16; i++) {
			if (tmp & (0x01 << i)) {
				seq_printf(m, "Rule %x enable\n", i);

				rule_type = RTL_R32(0x300 + i * 16);
				for (j = 0; j < 10; j++) {
					if (rule_type & (0x01 << j)) {
						u32 entry_number;
						u16 Type = TCAM_MAC;
						/* u16 Number = j; */
						TCAM_Entry_Setting_st stTCAM_Table = {0};

						seq_printf(m, "	MAC Rule %x enable\n", j);

						for (k = 0; k < TCAM_Property.Entry_Per_Set[Type]; k++) {
							entry_number = TCAM_Property.Start_In_TCAM[Type] + (j*TCAM_Property.Entry_Per_Set[Type])+k;
							TCAM_OCP_Read(tp, entry_number, TCAM_data, &stTCAM_Table);
							TCAM_OCP_Read(tp, entry_number, TCAM_care, &stTCAM_Table);

							seq_printf(m, "		%04x %04x %x \n", stTCAM_Table.Value, stTCAM_Table.DontCareBit, stTCAM_Table.Valid);
						}
					}
				}

				if (rule_type & (0x01 << MARI))
						seq_printf(m, "	Multicast IB Rule enable\n");
				if (rule_type & (0x01 << MARO))
						seq_printf(m, "	Multicast OOB Rule enable\n");
				if (rule_type & (0x01 << BRD))
						seq_printf(m, "	Broadcast Rule enable\n");

				for (j = 20; j < 30; j++) {
					if (rule_type & (0x01 << j)) {
						u32 entry_number;
						u16 Type = TCAM_TYPE;
						TCAM_Entry_Setting_st stTCAM_Table = {0};
						seq_printf(m, "	TYPE Rule %x enable\n", j-20);
						for (k = 0; k < TCAM_Property.Entry_Per_Set[Type]; k++) {
							entry_number = TCAM_Property.Start_In_TCAM[Type] + ((j-20)*TCAM_Property.Entry_Per_Set[Type])+k;
							TCAM_OCP_Read(tp, entry_number, TCAM_data, &stTCAM_Table);
							TCAM_OCP_Read(tp, entry_number, TCAM_care, &stTCAM_Table);
							seq_printf(m, "		%04x %04x %x \n", stTCAM_Table.Value, stTCAM_Table.DontCareBit, stTCAM_Table.Valid);
						}
					}
				}
				for (j = 30; j < 32; j++) {
					if (rule_type & (0x01 << j)) {
						u32 entry_number;
						u16 Type = TCAM_PTLv4;
						TCAM_Entry_Setting_st stTCAM_Table = {0};
						seq_printf(m, "	PTLv4 Rule %x enable\n", j-30);
						for (k = 0; k < TCAM_Property.Entry_Per_Set[Type]; k++) {
							entry_number = TCAM_Property.Start_In_TCAM[Type] + ((j-30)*TCAM_Property.Entry_Per_Set[Type])+k;
							TCAM_OCP_Read(tp, entry_number, TCAM_data, &stTCAM_Table);
							TCAM_OCP_Read(tp, entry_number, TCAM_care, &stTCAM_Table);
							seq_printf(m, "		%04x %04x %x \n", stTCAM_Table.Value, stTCAM_Table.DontCareBit, stTCAM_Table.Valid);
						}
					}
				}

				rule_type = RTL_R32(0x300 + i * 16 + 8);
				for (j = 4; j < 16; j++) {
					if (rule_type & (0x01 << j)) {
						u32 entry_number;
						u16 Type = TCAM_DPORT;
						TCAM_Entry_Setting_st stTCAM_Table = {0};
						seq_printf(m, "	DPROT Rule %x enable\n", j-4);
						for (k = 0; k < 10; k++) {
							entry_number = TCAM_Property.Start_In_TCAM[Type] + (j-4)*10+k;
							TCAM_OCP_Read(tp, entry_number, TCAM_data, &stTCAM_Table);
							TCAM_OCP_Read(tp, entry_number, TCAM_care, &stTCAM_Table);
							seq_printf(m, "		%04x %04x %x \n", stTCAM_Table.Value, stTCAM_Table.DontCareBit, stTCAM_Table.Valid);
						}
					}
				}
			}
		}
	}

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_tally_counter(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct rtl8169_private *tp = netdev_priv(dev);

	seq_puts(m, "\nDump Tally Counter\n");

	rtl8169_update_counters(dev);

	seq_puts(m, "Statistics\tValue\n----------\t-----\n");
	seq_printf(m, "tx_packets\t%lld\n", le64_to_cpu(tp->counters.tx_packets));
	seq_printf(m, "rx_packets\t%lld\n", le64_to_cpu(tp->counters.rx_packets));
	seq_printf(m, "tx_errors\t%lld\n", le64_to_cpu(tp->counters.tx_errors));
	seq_printf(m, "rx_missed\t%lld\n", le64_to_cpu(tp->counters.rx_missed));
	seq_printf(m, "align_errors\t%lld\n", le64_to_cpu(tp->counters.align_errors));
	seq_printf(m, "tx_one_collision\t%lld\n", le64_to_cpu(tp->counters.tx_one_collision));
	seq_printf(m, "tx_multi_collision\t%lld\n", le64_to_cpu(tp->counters.tx_multi_collision));
	seq_printf(m, "rx_unicast\t%lld\n", le64_to_cpu(tp->counters.rx_unicast));
	seq_printf(m, "rx_broadcast\t%lld\n", le64_to_cpu(tp->counters.rx_broadcast));
	seq_printf(m, "rx_multicast\t%lld\n", le64_to_cpu(tp->counters.rx_multicast));
	seq_printf(m, "tx_aborted\t%lld\n", le64_to_cpu(tp->counters.tx_aborted));
	seq_printf(m, "tx_underun\t%lld\n", le64_to_cpu(tp->counters.tx_underun));

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_txrx_desc(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct rtl8169_private *tp = netdev_priv(dev);
	int i;

	seq_puts(m, "\nDump Tx Descriptor\n");

	seq_printf(m, "cur_tx=%x dirty_tx=%x entry=%x!\n", tp->cur_tx, tp->dirty_tx, tp->dirty_tx % NUM_TX_DESC);
	for (i = 0; i < NUM_TX_DESC; i++) {
		seq_printf(m, "Tx Desc %02x ", i);
		seq_printf(m, "status1=%x ", le32_to_cpu(tp->TxDescArray[i].opts1));
		seq_printf(m, "status2=%x ", le32_to_cpu(tp->TxDescArray[i].opts2));
		seq_printf(m, "addr=%lx !\n", (long unsigned int)le64_to_cpu(tp->TxDescArray[i].addr));
	}

	seq_puts(m, "\nDump Rx Descriptor\n");

	seq_printf(m, "cur_rx=%x  entry=%x!\n", tp->cur_rx, tp->cur_rx % NUM_RX_DESC);
	for (i = 0; i < NUM_RX_DESC; i++) {
		seq_printf(m, "Rx Desc %02x ", i);
		seq_printf(m, "status1=%x ", le32_to_cpu(tp->RxDescArray[i].opts1));
		seq_printf(m, "status2=%x ", le32_to_cpu(tp->RxDescArray[i].opts2));
		seq_printf(m, "addr=%lx !\n", (long unsigned int)le64_to_cpu(tp->RxDescArray[i].addr));
	}

	seq_putc(m, '\n');
	return 0;
}

static int rtl8117_proc_open(struct inode *inode, struct file *file)
{
	struct net_device *dev = proc_get_parent_data(inode);
	int (*show)(struct seq_file *, void *) = PDE_DATA(inode);

	return single_open(file, show, dev);
}

static const struct file_operations rtl8117_proc_fops = {
	.open		= rtl8117_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * Table of proc files we need to create.
 */
struct rtl8117_proc_file {
	char name[12];
	int (*show)(struct seq_file *, void *);
};

static const struct rtl8117_proc_file rtl8117_proc_files[] = {
	{ "isolate", &proc_get_isolatestatus },
	{ "mac_reg", &proc_get_registers },
	{ "tcam", &proc_get_tcamrule},
	{ "tally", &proc_get_tally_counter},
	{ "descriptor", &proc_get_txrx_desc},
	{ "" }
};

#endif

static void rtl_shutdown(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_net_suspend(dev);

	/* Restore original MAC address */
	rtl_rar_set(tp, dev->perm_addr);

	rtl8169_hw_reset(tp);

	if (system_state == SYSTEM_POWER_OFF) {
		if (__rtl8169_get_wol(tp) & WAKE_ANY) {
			rtl_wol_suspend_quirk(tp);
		}
	}
}

static int rtl_remove_one(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

#ifdef RTL_OOB_PROC
	const struct rtl8117_proc_file *f;
	do {
		if (tp->dir_dev == NULL)
			break;

		remove_proc_entry("lanwake", tp->dir_dev);

		for (f = rtl8117_proc_files; f->name[0]; f++)
			remove_proc_entry(f->name, tp->dir_dev);

		if (rtw_proc == NULL)
			break;

		remove_proc_entry(dev->name, rtw_proc);
		remove_proc_entry(MODULENAME, init_net.proc_net);

		rtw_proc = NULL;
	} while (0);
#endif

	cancel_work_sync(&tp->wk.work);

	netif_napi_del(&tp->napi);

	unregister_netdev(dev);

	/* restore original MAC address */
	rtl_rar_set(tp, dev->perm_addr);

	rtl8169_release_board(pdev, dev, tp->mmio_addr);
	platform_set_drvdata(pdev, NULL);

	/* Yukuen: Add for link status uevent. 20150206 */
	if (tp->kthr) {
		kthread_stop(tp->kthr);
		tp->kthr = NULL;
	}

	if (tp->kthr_iso) {
		kthread_stop(tp->kthr_iso);
		tp->kthr_iso = NULL;
	}

	return 0;
}

static const struct net_device_ops rtl_netdev_ops = {
	.ndo_open		= rtl_open,
	.ndo_stop		= rtl8169_close,
	.ndo_get_stats64	= rtl8169_get_stats64,
	.ndo_start_xmit		= rtl8169_start_xmit,
	.ndo_tx_timeout		= rtl8169_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= rtl8169_change_mtu,
	.ndo_fix_features	= rtl8169_fix_features,
	.ndo_set_features	= rtl8169_set_features,
	.ndo_set_mac_address	= rtl_set_mac_address,
	.ndo_do_ioctl		= rtl8169_ioctl,
	.ndo_set_rx_mode	= rtl_set_rx_mode,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= rtl8169_netpoll,
#endif

};

static struct rtl_cfg_info {
	void (*hw_start)(struct net_device *);
	unsigned int region;
	unsigned int align;
	u16 event_slow;
	unsigned features;
	u8 default_ver;
} rtl_cfg_infos[] = {
	[RTL_CFG_0] = {
		.hw_start	= rtl_hw_start_8168,
		.region		= 2,
		.align		= 8,
		.event_slow	= LinkChg | RxOverflow,
		.features	= RTL_FEATURE_GMII | RTL_FEATURE_MSI,
		.default_ver	= RTL_GIGA_MAC_VER_01,
	}
};

static int
rtl_init_one(struct platform_device *pdev)
{
	struct rtl_cfg_info *cfg;
	struct rtl8169_private *tp;
	struct mii_if_info *mii;
	struct net_device *ndev;
	void __iomem *ioaddr;
	int chipset, i;
	int rc;
	int irq;
	const char *mac_addr;
#ifdef RTL_OOB_PROC
	struct proc_dir_entry *dir_dev = NULL;
	struct proc_dir_entry *entry = NULL;
#endif

	if (netif_msg_drv(&debug)) {
		printk(KERN_INFO "%s Gigabit Ethernet driver %s loaded\n",
			   MODULENAME, RTL8169_VERSION);
	}

	cfg = rtl_cfg_infos; /*run 8168_setting*/

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	ioaddr = of_iomap(pdev->dev.of_node, 0);

	ndev = alloc_etherdev(sizeof (*tp));
	if (!ndev) {
		rc = -ENOMEM;
		goto out;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->netdev_ops = &rtl_netdev_ops;
	ndev->irq = irq;
	tp = netdev_priv(ndev);
	tp->dev = ndev;
	tp->pdev = pdev;
	tp->msg_enable = netif_msg_init(debug.msg_enable, R8169_MSG_DEFAULT);
	tp->mac_version = 0;

	mii = &tp->mii;
	mii->dev = ndev;
	mii->phy_id_mask = 0x1f;
	mii->reg_num_mask = 0x1f;
	mii->supports_gmii = !!(cfg->features & RTL_FEATURE_GMII);

	tp->mmio_addr = ioaddr;

	rtl_irq_disable(tp);

	rtl_hw_reset(tp);

	rtl_ack_events(tp, 0xffff);

	chipset = tp->mac_version;
	tp->txd_version = rtl_chip_infos[chipset].txd_version;

	tp->get_settings = rtl8169_gset_xmii;
	tp->link_ok = rtl8169_xmii_link_ok;
	tp->do_ioctl = rtl_xmii_ioctl;

	mutex_init(&tp->wk.mutex);

	/* Get MAC address */
	mac_addr = of_get_mac_address(pdev->dev.of_node);
	if (mac_addr)
		rtl_rar_set(tp, (u8 *)mac_addr);

	rtl_use_inband_mac_address(tp);

	for (i = 0; i < ETH_ALEN; i++)
		ndev->dev_addr[i] = RTL_R8(MAC0 + i);

	rtl_dash_enable(tp);

	ndev->ethtool_ops = &rtl8169_ethtool_ops;
	ndev->watchdog_timeo = RTL8169_TX_TIMEOUT;

	netif_napi_add(ndev, &tp->napi, rtl8169_poll, R8169_NAPI_WEIGHT);

	/* don't enable SG, IP_CSUM and TSO by default - it might not work
	 * properly for all devices */
	ndev->features |= NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |NETIF_F_RXCSUM | NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_CTAG_RX;

	ndev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
		NETIF_F_RXCSUM | NETIF_F_HW_VLAN_CTAG_TX |
		NETIF_F_HW_VLAN_CTAG_RX;
	ndev->vlan_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
		NETIF_F_HIGHDMA;

	ndev->hw_features |= NETIF_F_RXALL;
	ndev->hw_features |= NETIF_F_RXFCS;

	tp->hw_start = cfg->hw_start;
	tp->event_slow = cfg->event_slow;

	tp->opts1_mask = ~(RxBOVF | RxFOVF);

	init_timer(&tp->timer);
	tp->timer.data = (unsigned long) ndev;
	tp->timer.function = rtl8169_phy_timer;

	rc = register_netdev(ndev);
	if (rc < 0)
		goto err_out_msi_4;

	//clear_swisr_table();
	rtl8169_clear_counters(ndev);
	platform_set_drvdata(pdev, ndev);

	netif_info(tp, probe, ndev, "%s at 0x%p, %pM, XID %08x IRQ %d\n",
		   rtl_chip_infos[chipset].name, ioaddr, ndev->dev_addr,
		   (u32)(RTL_R32(TxConfig) & 0x9cf0f8ff), ndev->irq);
	if (rtl_chip_infos[chipset].jumbo_max != JUMBO_1K) {
		netif_info(tp, probe, ndev, "jumbo features [frames: %d bytes, "
			   "tx checksumming: %s]\n",
			   rtl_chip_infos[chipset].jumbo_max,
			   rtl_chip_infos[chipset].jumbo_tx_csum ? "ok" : "ko");
	}

	device_set_wakeup_enable(&pdev->dev, tp->features & RTL_FEATURE_WOL);

	netif_carrier_off(ndev);

#ifdef RTL_OOB_PROC
	do {
		const struct rtl8117_proc_file *f;

		/* create /proc/net/$rtw_proc_name */
		rtw_proc = proc_mkdir_data(MODULENAME, S_IRUGO|S_IXUGO, init_net.proc_net, NULL);

		if (rtw_proc == NULL) {
			printk(KERN_INFO "procfs:create /proc/net/%s failed\n", MODULENAME);
			break;
		}

		/* create /proc/net/$rtw_proc_name/$dev->name */
		if (tp->dir_dev == NULL) {
			tp->dir_dev = proc_mkdir_data(ndev->name, S_IFDIR | S_IRUGO | S_IXUGO, rtw_proc, ndev);
			dir_dev = tp->dir_dev;

			if (dir_dev == NULL) {
				printk(KERN_INFO "procfs:create /proc/net/%s/%s failed\n", MODULENAME, ndev->name);
				if (rtw_proc) {
					remove_proc_entry(MODULENAME, init_net.proc_net);
					rtw_proc = NULL;
				}
				break;
			}
		} else {
			break;
		}

		entry = proc_create_data("lanwake", S_IFREG | S_IRUGO,
			dir_dev, &proc_fops, ndev);

		if (!entry) {
			printk(KERN_INFO "procfs:create /proc/net/%s/%s/lanwake failed\n", MODULENAME, ndev->name);
			break;
		}

		for (f = rtl8117_proc_files; f->name[0]; f++) {
			if (!proc_create_data(f->name, S_IFREG | S_IRUGO, dir_dev,
						&rtl8117_proc_fops, f->show)) {
				printk("Unable to initialize /proc/net/%s/%s/%s\n", MODULENAME, ndev->name, f->name);
			}
		}
	} while (0);
#endif


	/* Yukuen: Add for link status uevent. 20150206 */
	init_waitqueue_head(&tp->thr_wait);
	tp->link_chg = 0;
	tp->kthr = NULL;
	tp->kthr = kthread_create(link_status, (void *)ndev, "%s-linkstatus", ndev->name);
	if (IS_ERR(tp->kthr)) {
		tp->kthr = NULL;
		printk (KERN_WARNING "%s: unable to start kthread for link status.\n",
				ndev->name);
	} else{
		wake_up_process(tp->kthr);  /* run thread function */
	}

	init_waitqueue_head(&tp->thr_wait_iso);
	tp->isolate_chg = 0;
	tp->kthr_iso = NULL;
	tp->kthr_iso = kthread_create(isolate_status, (void *)ndev, "%s-isolatestatus", ndev->name);
	if (IS_ERR(tp->kthr_iso)) {
		tp->kthr_iso = NULL;
		printk (KERN_WARNING "%s: unable to start kthread for isolate status.\n",
				ndev->name);
	} else{
		wake_up_process(tp->kthr_iso);  /* run thread function */
	}

out:
	return rc;

err_out_msi_4:
	netif_napi_del(&tp->napi);
	free_netdev(ndev);
	goto out;
}

static struct platform_driver rtl8168_oob_driver = {
	.probe		= rtl_init_one,
	.remove		= rtl_remove_one,
	.shutdown	= rtl_shutdown,
	.driver = {
		.name		= MODULENAME,
		.owner		= THIS_MODULE,
		.pm			= RTL8169_PM_OPS,
		.of_match_table = of_match_ptr(rtl8168oob_dt_ids),
	},
};

module_platform_driver(rtl8168_oob_driver);

