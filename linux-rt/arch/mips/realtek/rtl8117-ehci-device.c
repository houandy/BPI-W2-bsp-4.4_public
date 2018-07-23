/*
 *  Copyright (c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <rtl8117_platform.h>

#include <linux/blkdev.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <asm/unaligned.h>
#include <linux/kthread.h>
#include <linux/usb/storage.h>

#include "rtl8117-ehci.h"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

enum {
	FSG_STRING_INTERFACE
};

#define BULK_BUF_SIZE 0x10000
#define CBW_SIZE 31
#define CSW_SIZE 13

struct rtl8117_ehci_device {
	struct usb_device *udev;

	unsigned int send_bulk_pipe;  /* cached pipe values */
	unsigned int recv_bulk_pipe;
	unsigned int send_ctrl_pipe;
	unsigned int recv_ctrl_pipe;
	unsigned int recv_intr_pipe;

	struct delayed_work schedule;

	struct usb_ctrlrequest request;
	struct command_block_wrapper data_read_cbw;
	struct command_status_wrapper data_read_csw;

	u8 data_read_buf[BULK_BUF_SIZE];
	struct command_block_wrapper data_write_cbw;
	struct command_status_wrapper data_write_csw;
	u8 data_write_buf[BULK_BUF_SIZE];

	struct delayed_work data_write_schedule;

	u32 bulk_out_rev_length;

	struct delayed_work data_read_schedule;

	u8 control_buf[256];
	u8 bulk_buf[256];

	struct file *filp;
	struct inode *inode;

	loff_t		file_length;
	loff_t		num_sectors;

	unsigned int	blkbits; /* Bits of logical block size
				    of bound block device */
	unsigned int	blksize; /* logical block size of bound block device */
};

const struct usb_device_descriptor msg_device_desc = {
    .bLength =		sizeof(msg_device_desc),
    .bDescriptorType =	USB_DT_DEVICE,
    .bcdUSB =		0x0200,
    .bDeviceClass =		USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass =      0,
    .bDeviceProtocol =      0,
    .bMaxPacketSize0 =      0x40,

    .idVendor =		0x0BDA,//Realtek Semiconductor Corp.
    .idProduct =	0x8168,
    .bcdDevice =            0x0200,
    .iManufacturer =	0x1,
    .iProduct =		0x2,
    .iSerialNumber =	0x3,
    .bNumConfigurations =	1,
};

static struct usb_config_descriptor msg_config_desc = {
	.bLength = USB_DT_CONFIG_SIZE,
	.bDescriptorType = USB_DT_CONFIG,
	.wTotalLength = 32,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = USB_CONFIG_ATT_ONE,
	.bMaxPower = 0x32,
};

struct usb_interface_descriptor fsg_intf_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,

	.bNumEndpoints =	2,		/* Adjusted during fsg_bind() */
	.bInterfaceClass =	USB_CLASS_MASS_STORAGE,
	.bInterfaceSubClass =	USB_SC_SCSI,	/* Adjusted during fsg_bind() */
	.bInterfaceProtocol =	USB_PR_BULK,	/* Adjusted during fsg_bind() */
	.iInterface =		FSG_STRING_INTERFACE,
};

struct usb_endpoint_descriptor fsg_fs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN | 0x1,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */

	.wMaxPacketSize = 0x200,
	.bInterval = 0x0,
};

struct usb_endpoint_descriptor fsg_fs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT | 0x2,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */

	.wMaxPacketSize = 0x200,
	.bInterval = 0x0,
};

struct usb_descriptor_header *fsg_fs_function[] = {
	(struct usb_descriptor_header *) &fsg_intf_desc,
	(struct usb_descriptor_header *) &fsg_fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fsg_fs_bulk_out_desc,
	NULL,
};

struct usb_endpoint_descriptor fsg_hs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

struct usb_endpoint_descriptor fsg_hs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_out_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
	.bInterval =		1,	/* NAK every 1 uframe */
};

struct usb_descriptor_header *fsg_hs_function[] = {
	(struct usb_descriptor_header *) &fsg_intf_desc,
	(struct usb_descriptor_header *) &fsg_hs_bulk_in_desc,
	(struct usb_descriptor_header *) &fsg_hs_bulk_out_desc,
	NULL,
};

struct usb_endpoint_descriptor fsg_ss_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

struct usb_ss_ep_comp_descriptor fsg_ss_bulk_in_comp_desc = {
	.bLength =		sizeof(fsg_ss_bulk_in_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/*.bMaxBurst =		DYNAMIC, */
};

struct usb_endpoint_descriptor fsg_ss_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_out_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

struct usb_ss_ep_comp_descriptor fsg_ss_bulk_out_comp_desc = {
	.bLength =		sizeof(fsg_ss_bulk_in_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/*.bMaxBurst =		DYNAMIC, */
};

struct usb_descriptor_header *fsg_ss_function[] = {
	(struct usb_descriptor_header *) &fsg_intf_desc,
	(struct usb_descriptor_header *) &fsg_ss_bulk_in_desc,
	(struct usb_descriptor_header *) &fsg_ss_bulk_in_comp_desc,
	(struct usb_descriptor_header *) &fsg_ss_bulk_out_desc,
	(struct usb_descriptor_header *) &fsg_ss_bulk_out_comp_desc,
	NULL,
};

const u8 str0[] = { 0x04, 0x03, 0x09, 0x04 };
const u8 str1[] = { 0x10 , 0x03, 'R', 0x00, 'e', 0x00, 'a', 0x00, 'l', 0x00, 't', 0x00, 'e', 0x00, 'k', 0x00 };
const u8 str2[] = { 0x20 , 0x03, 'U', 0x00, 'S', 0x00, 'B', 0x00, ' ', 0x00, 'R', 0x00, 'E', 0x00, 'D', 0x00, 'I', 0x00, 'R', 0x00, 'E', 0x00, 'C', 0x00, 'T', 0x00, 'I', 0x00, 'O', 0x00, 'N', 0x00 }    ;
const u8 str3[] = { 0x1C , 0x03, '0', 0x00, '3', 0x00, '5', 0x00, '7', 0x00, '8', 0x00, '0', 0x00, '2', 0x00, '1', 0x00, '1', 0x00, '5', 0x00, '9', 0x00, '4', 0x00, '6', 0x00 };

const u8 inquiry_data[] = { 0x00, 0x80, 0x04, 0x02, 0x20, 0x00, 0x00, 0x00, 'R', 'e', 'a', 'l', 't', 'e', 'k', ' ', 'P', 'h', 'o', 't', 'o', '2', 'h', 'o', 'm', 'e', ' ', 'D', 'I', 'S', 'K', ' ', '2', '.', '0', '0'};

static struct rtl8117_ehci_device rtl8117_ehci_dev;

int rtl8117_ehci_close_file(void)
{
	if (rtl8117_ehci_dev.filp) {
		vfs_fsync(rtl8117_ehci_dev.filp, 1);
		filp_close(rtl8117_ehci_dev.filp, NULL);

		rtl8117_ehci_dev.blksize = 0;
		rtl8117_ehci_dev.blkbits = 0;
		rtl8117_ehci_dev.filp = NULL;
		rtl8117_ehci_dev.file_length = 0;
		rtl8117_ehci_dev.num_sectors = 0;
	}

	return 0;
}

int rtl8117_ehci_open_file(char *path)
{
	struct file			*filp = NULL;
	int				rc = -EINVAL;
	struct inode			*inode = NULL;
	loff_t				size;
	loff_t				num_sectors;
	loff_t				min_sectors;
	unsigned int			blkbits;
	unsigned int			blksize;

	filp = filp_open(path, O_RDWR | O_LARGEFILE, 0);
	if (IS_ERR(filp)) {
		printk(KERN_CRIT "[EHCI] unable to open backing file: %s, filp = %d\n", path, (int)PTR_ERR(filp));
		dump_stack();
		return PTR_ERR(filp);
	}

	inode = file_inode(filp);
	if ((!S_ISREG(inode->i_mode) && !S_ISBLK(inode->i_mode))) {
		printk(KERN_CRIT "[EHCI] invalid file type: %s\n", path);
		dump_stack();
		goto out;
	}

	size = i_size_read(inode->i_mapping->host);
	if (size < 0) {
		printk(KERN_CRIT "[EHCI] unable to find file size: %s\n", path);
		rc = (int) size;
		goto out;
	}

	if (inode->i_bdev) {
		blksize = bdev_logical_block_size(inode->i_bdev);
		blkbits = blksize_bits(blksize);
	} else {
		blksize = 512;
		blkbits = 9;
	}

	num_sectors = size >> blkbits; /* File size in logic-block-size blocks */
	min_sectors = 1;

	if (num_sectors < min_sectors) {
		printk(KERN_CRIT "[EHCI] file too small: %s\n", path);
		rc = -ETOOSMALL;
		goto out;
	}

	rtl8117_ehci_dev.blksize = blksize;
	rtl8117_ehci_dev.blkbits = blkbits;
	rtl8117_ehci_dev.filp = filp;
	rtl8117_ehci_dev.file_length = size;
	rtl8117_ehci_dev.num_sectors = num_sectors;

	return 0;

out:
	fput(filp);
	return rc;
}

static void rtl8117_control_work_func_t(struct work_struct *work)
{
	struct usb_ctrlrequest *request = &rtl8117_ehci_dev.request;

	u8 desctype, descindex;
	u8 length = request->wLength;

	u8 desc_length = 0;

	switch ( request->bRequest ) {
	case USB_REQ_SET_ADDRESS:
		writeb(request->wValue, (volatile void __iomem *)DEVICE_ADDRESS);
		memset(rtl8117_ehci_dev.control_buf, 0, 64);
		rtl8117_ehci_ep0_start_transfer(0 , rtl8117_ehci_dev.control_buf, true, false);
		return;

	case USB_REQ_SET_CONFIGURATION:
		memset(rtl8117_ehci_dev.control_buf, 0, 64);
		rtl8117_ehci_ep0_start_transfer(0 , rtl8117_ehci_dev.control_buf, true, false);
		return;

	case USB_REQ_GET_DESCRIPTOR:
		desctype = request->wValue >> 8;
		descindex = (request->wValue & 0xFF);

		if (desctype == USB_DT_DEVICE) {
				memcpy(rtl8117_ehci_dev.control_buf, &msg_device_desc, 18);
				desc_length = 18;
		}

		if (desctype == USB_DT_CONFIG) {
			memcpy(rtl8117_ehci_dev.control_buf, &msg_config_desc, USB_DT_CONFIG_SIZE);
			desc_length += USB_DT_CONFIG_SIZE;

			memcpy(rtl8117_ehci_dev.control_buf + desc_length, &fsg_intf_desc, USB_DT_INTERFACE_SIZE);
			desc_length += USB_DT_INTERFACE_SIZE;

			memcpy(rtl8117_ehci_dev.control_buf + desc_length, &fsg_fs_bulk_in_desc, USB_DT_ENDPOINT_SIZE);
			desc_length += USB_DT_ENDPOINT_SIZE;

			memcpy(rtl8117_ehci_dev.control_buf + desc_length, &fsg_fs_bulk_out_desc, USB_DT_ENDPOINT_SIZE);
			desc_length += USB_DT_ENDPOINT_SIZE;
		}

		if (desctype == USB_DT_STRING) {
			if (descindex == 0) {
				memcpy(rtl8117_ehci_dev.control_buf, str0, sizeof(str0));
				desc_length = sizeof(str0);
			}
			else if (descindex == 1) {
				memcpy(rtl8117_ehci_dev.control_buf, str1, sizeof(str1));
				desc_length = sizeof(str1);
			}
			else if (descindex == 2) {
				memcpy(rtl8117_ehci_dev.control_buf, str2, sizeof(str2));
				desc_length = sizeof(str2);
			}
			else if (descindex == 3) {
				memcpy(rtl8117_ehci_dev.control_buf, str3, sizeof(str3));
				desc_length = sizeof(str3);
			}
		}

		if (length < desc_length)
			rtl8117_ehci_ep0_start_transfer(length , rtl8117_ehci_dev.control_buf, true, false);
		else
			rtl8117_ehci_ep0_start_transfer(desc_length , rtl8117_ehci_dev.control_buf, true, false);

		break;

	case USB_REQ_CLEAR_FEATURE:
		memset(rtl8117_ehci_dev.control_buf, 0, 64);
		rtl8117_ehci_ep0_start_transfer(0 , rtl8117_ehci_dev.control_buf, true, false);
		break;

	/* LUNS */
	//case IdeGetMaxLun:
	case 0xfe:
		memset(rtl8117_ehci_dev.control_buf, 0, 64);
		rtl8117_ehci_ep0_start_transfer(0 , rtl8117_ehci_dev.control_buf, true, true);
		break;

	default:
		break;
	}

	return;
}

int do_inquary(u8* buf,u8 *status)
{
	*status = US_BULK_STAT_OK;
	memcpy(buf, inquiry_data, sizeof(inquiry_data));
	return sizeof(inquiry_data);
}

int do_read_format_capacities(u8* destbuf, u8 *status)
{
	loff_t num_sectors = rtl8117_ehci_dev.num_sectors;
	unsigned int	blksize = rtl8117_ehci_dev.blksize;

	destbuf[0] = destbuf[1] = destbuf[2] = 0;
	destbuf[3] = 8;	/* Only the Current/Maximum Capacity Descriptor */
	destbuf += 4;

	put_unaligned_be32(num_sectors, &destbuf[0]);
						/* Number of blocks */
	put_unaligned_be32(blksize, &destbuf[4]);/* Block length */
	destbuf[4] = 0x02;				/* Current capacity */

	*status = US_BULK_STAT_OK;
	return 12;
}

int do_read_capacity(u8* cmnd, u8* destbuf, u8 *status)
{
	loff_t num_sectors = rtl8117_ehci_dev.num_sectors;
	unsigned int	blksize = rtl8117_ehci_dev.blksize;

	u32		lba = get_unaligned_be32(&cmnd[2]);
	int		pmi = cmnd[8];

	u8		*buf = destbuf;

	/* Check the PMI and LBA fields */
	if (pmi > 1 || (pmi == 0 && lba != 0)) {
		//curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		*status = US_BULK_STAT_PHASE;
		return -EINVAL;
	}

	put_unaligned_be32(num_sectors - 1, &buf[0]);
						/* Max logical block */
	put_unaligned_be32(blksize, &buf[4]);/* Block length */

	*status = US_BULK_STAT_OK;
	return 8;
}

static inline u32 get_unaligned_be24(u8 *buf)
{
	return 0xffffff & (u32) get_unaligned_be32(buf - 1);
}

static int do_read(u8* cmnd, u8* destbuf, int data_size_from_cmnd, u8 *status)
{
	loff_t num_sectors = rtl8117_ehci_dev.num_sectors;
	unsigned int	blkbits = rtl8117_ehci_dev.blkbits;

	u32 lba;
	u32 amount_left;
	loff_t file_offset;
	loff_t file_offset_tmp;

	int nread = 0;
	mm_segment_t old_fs;

	if (!rtl8117_ehci_dev.filp) {
		*status = US_BULK_STAT_PHASE;
		return -EINVAL;
	}

	if (cmnd[0] == READ6)
		lba = get_unaligned_be24(&cmnd[1]);
	else {
		lba = get_unaligned_be32(&cmnd[2]);

		/*
		 * We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = don't read from the
		 * cache), but we don't implement them.
		 */
		if ((cmnd[1] & ~0x18) != 0) {
			*status = US_BULK_STAT_FAIL;
			return -EINVAL;
		}
	}
	if (lba >= num_sectors) {
		*status = US_BULK_STAT_FAIL;
		return -EINVAL;
	}
	file_offset = ((loff_t) lba) << blkbits;

	/* Carry out the file reads */
	amount_left = data_size_from_cmnd;
	if (unlikely(amount_left == 0))
		return -EIO;		/* No default reply */

	file_offset_tmp = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	nread = vfs_read(rtl8117_ehci_dev.filp,
			 (char __user *)destbuf,
			 data_size_from_cmnd, &file_offset);

	set_fs(old_fs);

	*status = US_BULK_STAT_OK;
	return nread;
}

static int do_write(u8* cmnd, u8* destbuf, int data_size_from_cmnd ,u8 *status)
{
	unsigned int	blkbits = rtl8117_ehci_dev.blkbits;
	u32			lba;
	int			get_some_more;
	u32			amount_left_to_req, amount_left_to_write;
	loff_t			usb_offset, file_offset; //file_offset_tmp;

	int nwrite = 0;
	mm_segment_t old_fs;

#if 0
	if (curlun->ro) {
		curlun->sense_data = SS_WRITE_PROTECTED;
		return -EINVAL;
	}
#endif

	if (!rtl8117_ehci_dev.filp) {
		*status = US_BULK_STAT_PHASE;
		return -EINVAL;
	}

	spin_lock(&rtl8117_ehci_dev.filp->f_lock);
	rtl8117_ehci_dev.filp->f_flags &= ~O_SYNC;	/* Default is not to wait */
	spin_unlock(&rtl8117_ehci_dev.filp->f_lock);

	/*
	 * Get the starting Logical Block Address and check that it's
	 * not too big
	 */
	if (cmnd[0] == WRITE6)
		lba = get_unaligned_be24(&cmnd[1]);
	else {
		lba = get_unaligned_be32(&cmnd[2]);

		/*
		 * We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = write directly to the
		 * medium).  We don't implement DPO; we implement FUA by
		 * performing synchronous output.
		 */
		if (cmnd[1] & ~0x18) {
			//curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			*status = US_BULK_STAT_FAIL;
			return -EINVAL;
		}

	}

	/* Carry out the file writes */
	get_some_more = 1;
	file_offset = usb_offset = ((loff_t) lba) << blkbits;
	amount_left_to_req = data_size_from_cmnd;
	amount_left_to_write = data_size_from_cmnd;

	//printk(KERN_INFO "do_write file_offset = %d, count = %d\n", (u32)file_offset, amount_left_to_req);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	nwrite = vfs_write(rtl8117_ehci_dev.filp,
			   (char __user *)destbuf,
			   data_size_from_cmnd, &file_offset);
	set_fs(old_fs);

	*status = US_BULK_STAT_OK;
	return nwrite;		/* No default reply */
}


static int do_mode_sense(u8* cmnd, u8* destbuf, u8 *status)
{
	memset(destbuf, 0x0, 8);
	*destbuf = 0x3;

	return 8;
}

static int do_synchronize_cache(struct file *filp, u8 *status)
{
	vfs_fsync(filp, 1);
	return 0;
}

static int do_request_sense(u8* cmnd, u8* destbuf, u8 *status)
{
	memset(destbuf, 0x0, 18);
	destbuf[0] = 0x70;
	destbuf[7] = 0x0A;

	destbuf[2] = 0x5;
	destbuf[12] = 0x20;
	return 18;
}

static void rtl8117_data_read_work_func_t(struct work_struct *work)
{
	struct command_block_wrapper *cbw = &rtl8117_ehci_dev.data_read_cbw;
	u8 *databuf = rtl8117_ehci_dev.data_read_buf;

	u32 residual_length = cbw->dCBWDataTransferLength;

	int ret = 0;
	int data_size_from_cmnd = 0;

	u8 bCSWStatus = 0;
	int transfer_left = 0, transfer_size = 0, readbuf_offset = 0;

	switch (cbw->rbc[0]) {
	case INQUIRY:
		ret = do_inquary(databuf, &bCSWStatus);
		break;

	case READ_FORMAT_CAPACITIES:
		ret = do_read_format_capacities(databuf, &bCSWStatus);
		break;

	case READ_CAPACITY:
		ret = do_read_capacity(cbw->rbc, databuf, &bCSWStatus);
		break;

	case READ6:
		data_size_from_cmnd = residual_length;
		ret = do_read(cbw->rbc, databuf, data_size_from_cmnd, &bCSWStatus);
		break;

	case READ10:
		data_size_from_cmnd = residual_length;
		ret = do_read(cbw->rbc, databuf, data_size_from_cmnd, &bCSWStatus);
		break;

	case MODE_SENSE6:
		ret = do_mode_sense(cbw->rbc, databuf, &bCSWStatus);
		break;

	case TEST_UNIT_READY:
		ret = 0;
		break;

	case PREVENT_ALLOW_MEDIUM_REMOVAL:
		bCSWStatus = cbw->rbc[4];
		ret = 0;
		break;

	case REQUEST_SENSE:
		ret = do_request_sense(cbw->rbc, databuf, &bCSWStatus);
		break;

	case SYNC_CACHE:
		ret = do_synchronize_cache(rtl8117_ehci_dev.filp, &bCSWStatus);
		break;

	case START_STOP_UNIT:
		ret = 0;
		bCSWStatus = 0;
		break;

	default:
		printk(KERN_INFO "[EHCI] unknown read cbw->rbc[0] = 0x%x!!!\n",cbw->rbc[0]);
		dump_stack();
		ret = 0;
		break;
	}

	/* data */
	if (ret > 0) {
		readbuf_offset = 0;
		transfer_left = ret;

		while(transfer_left) {
			transfer_size = (transfer_left >= MAX_BULK_LEN) ? (MAX_BULK_LEN) : (transfer_left);
			rtl8117_ehci_ep_start_transfer( transfer_size, databuf + readbuf_offset, true, false );
			transfer_left -= transfer_size;
			readbuf_offset += transfer_size;
		}
	}

	/* csw */
	rtl8117_ehci_dev.data_read_csw.dCSWSignature = cpu_to_le32(0x53425355);
	rtl8117_ehci_dev.data_read_csw.dCSWTag = rtl8117_ehci_dev.data_read_cbw.dCBWTag;

#if 0
	rtl8117_ehci_dev.data_read_csw.dCSWDataResidue = ( residual_length - ret );
#else
	rtl8117_ehci_dev.data_read_csw.dCSWDataResidue = (ret > 0) ? (residual_length - ret) : (residual_length);
#endif

	rtl8117_ehci_dev.data_read_csw.bCSWStatus = bCSWStatus;

	rtl8117_ehci_ep_start_transfer( CSW_SIZE, (u8*)&rtl8117_ehci_dev.data_read_csw, true, false );
	rtl8117_ehci_ep_start_transfer( MAX_BULK_LEN, (u8*)rtl8117_ehci_dev.bulk_buf, false, false );

	return;
}

static void rtl8117_data_write_work_func_t(struct work_struct *work)
{
	struct command_block_wrapper *cbw = &rtl8117_ehci_dev.data_write_cbw;
	u8 *databuf = rtl8117_ehci_dev.data_write_buf;

	u32 residual_length = cbw->dCBWDataTransferLength;
	int ret;

	u8 bCSWStatus = 0;

	switch (cbw->rbc[0]) {
		case WRITE10:
		case WRITE12:
			ret = do_write(cbw->rbc, databuf, residual_length, &bCSWStatus);
			break;

		case MODE_SELECT6:
		case MODE_SELECT10:
		default:
			printk(KERN_INFO "[EHCI] unknown write cbw->rbc[0] = 0x%x!!!\n",cbw->rbc[0]);
			break;
	}

	/* csw */
	rtl8117_ehci_dev.data_write_csw.dCSWSignature = cpu_to_le32(0x53425355);
	rtl8117_ehci_dev.data_write_csw.dCSWTag = rtl8117_ehci_dev.data_write_cbw.dCBWTag;
#if 0
	rtl8117_ehci_dev.data_write_csw.dCSWDataResidue = ( residual_length - ret );
#else
	rtl8117_ehci_dev.data_write_csw.dCSWDataResidue = ( ret > 0 ) ? (residual_length - ret) : (residual_length);
#endif
	rtl8117_ehci_dev.data_write_csw.bCSWStatus = bCSWStatus;

	rtl8117_ehci_ep_start_transfer( CSW_SIZE, (u8*)&rtl8117_ehci_dev.data_write_csw, true, false );
	rtl8117_ehci_ep_start_transfer( MAX_BULK_LEN, (u8*)rtl8117_ehci_dev.bulk_buf, false, false );
	return;
}

int rtl8117_ehci_device_init(void)
{
	memset(&rtl8117_ehci_dev, 0, sizeof(struct rtl8117_ehci_device));

	INIT_DELAYED_WORK(&rtl8117_ehci_dev.schedule, rtl8117_control_work_func_t);
	INIT_DELAYED_WORK(&rtl8117_ehci_dev.data_write_schedule, rtl8117_data_write_work_func_t);
	INIT_DELAYED_WORK(&rtl8117_ehci_dev.data_read_schedule, rtl8117_data_read_work_func_t);

	return 0;
}

void rtl8117_echi_control_request(struct usb_ctrlrequest *request)
{
	u8 *dbg = (u8 *) request;
	printk(KERN_ALERT "[EHCI] Setup packet %02x %02x %02x %02x %02x %02x %02x %02x\n", *dbg, *(dbg+1), *(dbg+2), *(dbg+3), *(dbg+4), *(dbg+5), *(dbg+6), *(dbg+7));

	memcpy(&rtl8117_ehci_dev.request, request, sizeof(struct usb_ctrlrequest));
	schedule_delayed_work(&rtl8117_ehci_dev.schedule, 0);
}

void rtl8117_ehci_bulkout_request(void* ptr, u32 length)
{
	struct command_block_wrapper *cbw = ptr;
	bool is_cbw = false;
	bool write = false;
	bool schedule_to_usb = false;
	u32 start_copy_address;

	/* cbw */
	if ((length == CBW_SIZE) && (cbw->dCBWSignature == 0x43425355)) {
		switch (cbw->rbc[0]) {
			case WRITE10:
			case WRITE12:
			case MODE_SELECT6:
			case MODE_SELECT10:
				write = true;
				break;
			default:
				write = false;
				schedule_to_usb = true;
				break;
		}
		is_cbw = true;
	}

	if (is_cbw) {
		if (write) {
			memcpy(&rtl8117_ehci_dev.data_write_cbw, ptr, CBW_SIZE);
			rtl8117_ehci_ep_start_transfer( MAX_BULK_LEN, (u8*)rtl8117_ehci_dev.bulk_buf, false, false );
		}
		else {
			memcpy(&rtl8117_ehci_dev.data_read_cbw, ptr, CBW_SIZE);
			schedule_delayed_work(&rtl8117_ehci_dev.data_read_schedule, 0);
		}
	}
	else {
		start_copy_address = rtl8117_ehci_dev.bulk_out_rev_length;
		rtl8117_ehci_dev.bulk_out_rev_length += length;

		memcpy(rtl8117_ehci_dev.data_write_buf + start_copy_address, ptr, length);
		if (rtl8117_ehci_dev.data_write_cbw.dCBWDataTransferLength == rtl8117_ehci_dev.bulk_out_rev_length) {
			rtl8117_ehci_dev.bulk_out_rev_length = 0;
			schedule_delayed_work(&rtl8117_ehci_dev.data_write_schedule, 0);
		}
		else
			//memcpy(rtl8117_ehci_dev.data_write_buf + start_copy_address, ptr, length);
			rtl8117_ehci_ep_start_transfer( MAX_BULK_LEN, (u8*)rtl8117_ehci_dev.bulk_buf, false, false );
	}
}
