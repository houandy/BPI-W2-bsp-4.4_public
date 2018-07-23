#include <linux/types.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <rtl8117_platform.h>

#include "rtl8117-ehci.h"

struct rtl8117_ehci_core {
	struct platform_device *pdev;

	/* control out desc */
	u8* ctl_outdesc_addr;
	dma_addr_t ctl_outdesc_phy;
	u8 ctl_outdesc_index;

	/* control in desc */
	u8* ctl_indesc_addr;
	dma_addr_t ctl_indesc_phy;
	u8 ctl_indesc_index;

	/* bulk out desc */
	u8* bulk_outdesc_addr;
	dma_addr_t bulk_outdesc_phy;
	u8 bulk_outdesc_index;

	/* bulk in desc */
	u8* bulk_intdesc_addr;
	dma_addr_t bulk_indesc_phy;
	u8 bulk_indesc_index;

	/* control out buffer */
	u8* ctl_outbuf_addr;
	dma_addr_t ctl_outbuf_phy;

	/* bulk out buffer */
	u8* bulk_outbuf_addr;
	dma_addr_t bulk_outbuf_phy;
};

static struct rtl8117_ehci_core ehci_core;

//OOB writes IB IO
void Wt_IBIO(u8 phyaddr,u32 Wt_Data)
{
	u32  temp_count;
	u8 offset;
	offset = phyaddr;

	rtl8117_ehci_writel(Wt_Data, OOBACTDATA);
	rtl8117_ehci_writel(0x8000F000|offset, OOBACTADDR);

	temp_count=0;
	while((ehci_readl(OOBACTADDR)>>31)==1 && temp_count<1000)
		temp_count++;
}

//OOB writes IB PCIE configuration space
void Wt_PCIECS(u8 phyaddr,u32 Wt_Data)
{
	u8  temp_count;
	u8 offset;
	offset = phyaddr;

	rtl8117_ehci_writel(Wt_Data, DBIACTDATA);
	rtl8117_ehci_writel(0x8000F000|offset, DBIACTADDR);

	temp_count=0;

	while((ehci_readl(DBIACTADDR)>>31)==1&&temp_count<1000)
	{
		temp_count++;
	};
}

void rtl8117_ehci_init(void)
{
	int i;

	outindesc_r *ctloutdesc = (outindesc_r *)(ehci_core.ctl_outdesc_addr);
	outindesc_r *ctlindesc = (outindesc_r *)(ehci_core.ctl_indesc_addr);
	outindesc_r *outdesc = (outindesc_r *)(ehci_core.bulk_outdesc_addr);
	outindesc_r *indesc = (outindesc_r *)(ehci_core.bulk_intdesc_addr);

	//Reset Index
	ehci_core.ctl_outdesc_index=0;
	ehci_core.ctl_indesc_index=0;
	ehci_core.bulk_outdesc_index=0;
	//BulkDescOUTCur = 0;
	ehci_core.bulk_indesc_index=0;

	printk(KERN_INFO "[EHCI] enter rtl8117_ehci_init\n");

	//for FPGA test(patch hardware bug)
#if 0
	Wt_IBIO(HCSPARAMS,0x00000001);
	//Wt_IBIO(PORTSC,D_tmp|0x00000001);
#else
	Wt_IBIO(HCSPARAMS,0x00000003);
#endif

	rtl8117_ehci_writel(0x00000000, EHCICONFIG);
	/* step1. tune out and in buffer size */

	rtl8117_ehci_writel(0x0002001B, EHCICONFIG);
	/* step2. */
	//(1)prepare control/bulk out/bulk in descriptors
	// tomadd 2010.11.18
	//  [1]  for control out descriptor, CTLOUTdescNumber=4
	for(i=0; i< CTLOUTdescNumber; i++)
	{
		ctloutdesc->length = 0x40;
		ctloutdesc->bufaddr = ehci_core.ctl_outbuf_phy + i*0x40;

		if(i==CTLOUTdescNumber-1)
			ctloutdesc ->eor = 1;
		else
			ctloutdesc ->eor = 0;

		ctloutdesc->own = 1;
		ctloutdesc++;
	}

	// tomadd 2010.12.14
	//  [2]  for control in descriptor, CTLINdescNumber=1
	for(i=0; i< CTLINdescNumber; i++)
	{
		//only set the eor
		if(i==CTLINdescNumber-1)
			ctlindesc ->eor = 1;
		else
			ctlindesc ->eor = 0;
			ctlindesc++;
	}
	//  [3] for bulk out descriptors, OUTdescNumber=4
	for(i=0; i< OUTdescNumber; i++)
	{
		outdesc->length = MAX_BULK_LEN;
		//default assign CBW as  bulk out buffer
		outdesc->bufaddr = ehci_core.bulk_outbuf_phy;

		if(i==OUTdescNumber-1)
			outdesc ->eor = 1;//last descriptor
		else
			outdesc ->eor = 0;
		if(i == 0)
			outdesc->own = 1;
			outdesc++;
	}

	//  [4] for bulk IN descriptors, INdescNumber=4
	for (i = 0; i < INdescNumber; i++)
	{
		//only set the eor
		if (i == INdescNumber-1)
			indesc->eor  = 1;
		else
			indesc->eor  = 0;
			indesc++;
	}

	//(2)assign out descriptor address to register
	rtl8117_ehci_writel(ehci_core.bulk_outdesc_phy, OUTDesc_Addr);

	//(3)assign in descriptor address to register
	rtl8117_ehci_writel(ehci_core.bulk_indesc_phy, INDesc_Addr);

	//(4)assign control out descriptor address to register
	rtl8117_ehci_writel(ehci_core.ctl_outdesc_phy, CTLOUTDesc_Addr);

	//(5)assign control in descriptor address to register
	rtl8117_ehci_writel(ehci_core.ctl_indesc_phy, CTLINDesc_Addr);
	rtl8117_ehci_writel(0x600003FF, EHCI_IMR);

	/* step4. */
	// enable OUT transaction state mechine
	rtl8117_ehci_writel(ehci_readl(EHCICONFIG) | 0x00010000, EHCICONFIG);
}

int rlt8117_ehci_core_init(void *platform_pdev)
{
	struct platform_device *pdev = (struct platform_device *)platform_pdev;
	struct device *d = &pdev->dev;

	printk(KERN_INFO "[EHCI] enter ehci_usb_enabled\n");

	ehci_core.pdev = pdev;
	ehci_core.ctl_outdesc_addr = dma_alloc_coherent(d, 0x200, &ehci_core.ctl_outdesc_phy, GFP_ATOMIC);
	if (!ehci_core.ctl_outdesc_addr) {
		printk(KERN_CRIT "[EHCI] ctl_outdesc_addr is null\n");
		return -1;
	}

	ehci_core.ctl_indesc_addr = (ehci_core.ctl_outdesc_addr + 0x040);

	ehci_core.bulk_outdesc_addr = (ehci_core.ctl_outdesc_addr + 0x080);
	ehci_core.bulk_intdesc_addr = (ehci_core.ctl_outdesc_addr + 0x0C0);

	ehci_core.ctl_indesc_phy = (ehci_core.ctl_outdesc_phy + 0x040);
	ehci_core.bulk_outdesc_phy = (ehci_core.ctl_outdesc_phy + 0x080);
	ehci_core.bulk_indesc_phy = (ehci_core.ctl_outdesc_phy + 0x0C0);

	ehci_core.ctl_outbuf_addr = kmalloc(0x40*CTLOUTdescNumber, GFP_ATOMIC);
	if (!ehci_core.ctl_outbuf_addr) {
		printk(KERN_CRIT "[EHCI] ctl_outbuf_addr is null\n");
		return -1;
	}

	ehci_core.ctl_outbuf_phy = dma_map_single(d, ehci_core.ctl_outbuf_addr, 0x40*CTLOUTdescNumber, DMA_FROM_DEVICE);
	if (!ehci_core.ctl_outbuf_phy) {
		printk(KERN_CRIT "[EHCI] ctl_outbuf_phy is null\n");
		return -1;
	}

	ehci_core.bulk_outbuf_addr = dma_alloc_coherent(d, MAX_BULK_LEN*OUTdescNumber, &ehci_core.bulk_outbuf_phy, GFP_ATOMIC);
	if (!ehci_core.bulk_outbuf_addr) {
		printk(KERN_CRIT "[EHCI] bulk_outbuf_addr is null\n");
		return -1;
	}

	rtl8117_ehci_init();

	return 0;
}

// ehci engine reset function
int EHCI_RST(void)
{
	u32 ehcirst,count;

	count=0;
	ehcirst = 0x00800000;

	rtl8117_ehci_writel(ehcirst, EHCICONFIG);

	do
	{
		rtl8117_ehci_writel(ehcirst, EHCICONFIG);
		ehcirst = ehci_readl(EHCICONFIG);

		if (++count > 1000)
		{
			return 1;
		}
	}
	while ( !(ehcirst & 0x00800000) );	/* ehci firmware reset */
	return 0;
}

void ehci_usb_disabled(void)
{
	Wt_IBIO(PORTSC, 0x1000);

	//reset ehci to avoid cmdsts always 0x80 @usb disconnect
	EHCI_RST();
	mdelay(1000);
}

u32 Rd_IBIO(u8 phyaddr)
{
	u32  temp_count;
	u8  offset;
	//default error code
	u32  Rd_Data = 0xDEADBEAF;
	offset = phyaddr;

	rtl8117_ehci_writel(0x0000F000|offset, OOBACTADDR);
	temp_count=0;

	while((ehci_readl(OOBACTADDR)>>31)==0 && temp_count<1000)
	{
		Rd_Data = ehci_readl(OOBACTDATA);
		temp_count++;
	};

	return ehci_readl(OOBACTDATA);
}

//EHCI Bulk IN transfer
void rtl8117_ehci_ep_start_transfer(u32 len, u8 *addr, u8 is_in, bool stall)
{
	struct device *d = &ehci_core.pdev->dev;

	outindesc_r *indesc = (outindesc_r *)(ehci_core.bulk_intdesc_addr)+ehci_core.bulk_indesc_index;
	outindesc_r *outdesc= (outindesc_r *) (ehci_core.bulk_outdesc_addr)+ehci_core.bulk_outdesc_index;

	if (is_in)
	{
		//if already stalled, just return
		while((ehci_readl(CMDSTS) & 0x00000080))
			;

		indesc->length = len; // buffer size = 4kbytes
		indesc->ls  = 1;
		indesc->fs  = 0;//first segement

		if (stall) {
			indesc->stall = 1;
		}
		else {
			indesc->bufaddr = virt_to_phys(addr);
			dma_sync_single_for_device(d, indesc->bufaddr, len, DMA_TO_DEVICE);
			indesc->stall = 0;
		}

		indesc->own = 1;

		rtl8117_ehci_writel(ehci_readl(CMDSTS) | 0x00000080, CMDSTS);
		ehci_core.bulk_indesc_index = ( ehci_core.bulk_indesc_index + 1 ) % INdescNumber;
	}
	else
	{
		outdesc->length = len;

		outdesc->own = 1;
		rtl8117_ehci_writel(0x00000008, EHCI_ISR);
	}
}

//EHCI control IN transfer
void rtl8117_ehci_ep0_start_transfer(u16 len, u8 *addr, u8 is_in, bool stall)
{
	struct device *d = &ehci_core.pdev->dev;

	//u8 *ctloutaddr,*ctlinaddr;
	outindesc_r *ctlindesc = (outindesc_r *)(ehci_core.ctl_indesc_addr)+ehci_core.ctl_indesc_index;
	ehci_core.ctl_indesc_index = ( ehci_core.ctl_indesc_index + 1 ) % CTLINdescNumber;

	///u32 tcounter=0;
	ctlindesc->bufaddr = virt_to_phys((void*)addr);
	dma_sync_single_for_device(d, ctlindesc->bufaddr, 0x40, DMA_TO_DEVICE);
	if (is_in)
	{
		while(((ehci_readl(CMDSTS)&0x00000001)!=0))
			;

		if(stall == 1)
			ctlindesc->stall = 1;//set stall
		else
			ctlindesc->stall = 0;//clear stall

		ctlindesc->ls = 1;//last segment
		ctlindesc->length = len;
		ctlindesc->own= 1;

		//command status register
		//bit0: control in descriptor polling queue
		rtl8117_ehci_writel(ehci_readl(CMDSTS) | 0x00000001, CMDSTS);
		while(((ehci_readl(CMDSTS)&0x00000001)!=0))
			;
	}
}

//recycle control out descriptor
void recycle_ctloutdesc(outindesc_r *ctloutdesc)
{
	u8 i = 0;
	if (!ctloutdesc->own)
	{
		while (i++ < CTLOUTdescNumber)
		{
			//recycle control out descriptor
			ctloutdesc->length = 0x40;
			ctloutdesc->own = 1;
			ehci_core.ctl_outdesc_index= ( ehci_core.ctl_outdesc_index+ 1 )% (CTLOUTdescNumber) ;
			ctloutdesc = (outindesc_r *) ehci_core.ctl_outdesc_addr + ehci_core.ctl_outdesc_index;
		}

		ehci_core.ctl_outdesc_index= ( ehci_core.ctl_outdesc_index+ 1 )% (CTLOUTdescNumber) ;
		ctloutdesc = (outindesc_r *) ehci_core.ctl_outdesc_addr + ehci_core.ctl_outdesc_index;
	}
}

bool rtl8117_ehci_intr_handler(void)
{
	u32 OUTSTS;
	u32 packetlen;
	u32 D_tmp;

	struct device *d = &ehci_core.pdev->dev;

	//bulk out descriptor address
	outindesc_r *outdesc = (outindesc_r *) (ehci_core.bulk_outdesc_addr)+ehci_core.bulk_outdesc_index;

	//control out descriptor address
	outindesc_r *ctloutdesc =  (outindesc_r *)(ehci_core.ctl_outdesc_addr)+ehci_core.ctl_outdesc_index;

	//disable IMR
	rtl8117_ehci_writel(0x00000000, EHCI_IMR);
	OUTSTS = ehci_readl(EHCI_ISR);

	//clear ISR
	rtl8117_ehci_writel(OUTSTS & 0xFFFFFFF7, EHCI_ISR);

	//host controller reset done interrupt
	if(OUTSTS & 0x00000080)
	{
		printk(KERN_INFO "[EHCI] reset ehci done\n");

		/* to fix UEFI issue */
		Wt_IBIO(PORTSC, 0x1000);
		Wt_IBIO(PORTSC+0x04, 0x1000);

		rtl8117_ehci_init();
#if 0
		rtl8117_set_ehci_enable(1);
		rtl8117_ehci_set_otg_power(0);
		rtl8117_ehci_poweron_request();
#endif
	}

	//host port reset interrupt
	if(OUTSTS & 0x00000200)
	{
	}

	//host port reset done interrupt
	if(OUTSTS & 0x00000100)
	{
		D_tmp = ehci_readl(EHCICONFIG);
	}

	//Bit0 =1, setup token
	if(OUTSTS & 0x00000001)
	{
		// Firmware needs to check out descriptor own bit = 0 or 1
		if(!(ctloutdesc->own))
		{
			dma_rmb();

			dma_sync_single_for_cpu(d, ctloutdesc->bufaddr, 0x40*CTLOUTdescNumber, DMA_FROM_DEVICE);
			rtl8117_echi_control_request((struct usb_ctrlrequest *)(phys_to_virt(ctloutdesc->bufaddr)));
			dma_sync_single_for_device(d, ctloutdesc->bufaddr, 0x40*CTLOUTdescNumber, DMA_FROM_DEVICE);

			recycle_ctloutdesc(ctloutdesc);
		}
	}

	//Bit1 =1,  control out
#if 0
	if(OUTSTS & 0x00000002)
	{
		//REG32(EHCI_ISR)=REG32(EHCI_ISR)&0x00000002;
		// Firmware needs to check out descriptor own bit = 0 or 1
		if(!(ctloutdesc->own))
		{
			//packetlen = 0x40-ctloutdesc->stoi.length;
			// or call IN transaction function.
			//if(packetlen == 0x00)
			//	setup_phase();//parsing usb2.0 chap 9 pattern...

			// recycle outdescriptor
			//ctloutdesc->stoi.length = 0x40; // write back to total bytes to transfer
			//ctloutdesc->stoi.own = 1;             // wrtie back to ownbit
			handle_ep0();
			recycle_ctloutdesc(ctloutdesc);
		}
	}
#endif

	//Bit4 =1, bulk out transaction
	if(OUTSTS & 0x00000010)
	{
		if(!(outdesc->own)) {
			ehci_core.bulk_outdesc_index= ( ehci_core.bulk_outdesc_index+ 1 )% (OUTdescNumber) ;

			dma_rmb();

			dma_sync_single_for_cpu(d, outdesc->bufaddr, MAX_BULK_LEN*OUTdescNumber, DMA_FROM_DEVICE);
			packetlen = MAX_BULK_LEN - outdesc->length;
			rtl8117_ehci_bulkout_request(phys_to_virt(outdesc->bufaddr), packetlen);
			dma_sync_single_for_device(d, outdesc->bufaddr, MAX_BULK_LEN*OUTdescNumber, DMA_FROM_DEVICE);

			rtl8117_ehci_writel(0x00000008, EHCI_ISR);
		}
	}

	//Bit2: control out descriptor unavailable
	//Bit3: bulk out descriptor unavailable
	//Bit5: bulk in descriptor unavailable
	//Bit6: control in descriptor unavailable
	if(OUTSTS & 0x00000004)
	{
		//process control out descriptor unavailable
	}

#if 0
	if(OUTSTS & 0x00000008)
	{
		//REG32(EHCI_ISR)=REG32(EHCI_ISR)&0x00000008;
		//process bulk out descriptor unavailable
		recycle_outdesc(outdesc);
	}
#endif

	if(OUTSTS & 0x00000020)
	{
		//REG32(EHCI_ISR)=REG32(EHCI_ISR)&0x00000020;
		//process bulk in descriptor unavailable
	}

	if(OUTSTS & 0x00000040)
	{
		//REG32(EHCI_ISR)=REG32(EHCI_ISR)&0x00000040;
		//process control in descriptor unavailable
	}

	rtl8117_ehci_writel(0xE00003F7, EHCI_IMR);
	return 1;
}

void rtl8117_ehci_intep_enabled(u8 portnum)
{
	rtl8117_ehci_init();

	if(portnum == 1) {
		Wt_IBIO(PORTSC+0x04, 0x1001);//port1
	}

	if(portnum == 2) {
		Wt_IBIO(PORTSC+0x08, 0x1001);//port2
	}

	rtl8117_ehci_writeb(0, (DEVICE_ADDRESS + 1));
	rtl8117_ehci_writeb(0, (DEVICE_ADDRESS + 2));
}

void rtl8117_ehci_intep_disabled(u8 portnum)
{
	if(portnum == 1)
		Wt_IBIO(PORTSC+0x04, 0x1000);//port1
	if(portnum == 2)
		Wt_IBIO(PORTSC+0x08, 0x1000);//port2

	mdelay(1000);
}
