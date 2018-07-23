#ifndef _RTL8117_EHCI_H_
#define _RTL8117_EHCI_H_

#define CTLOUTdescNumber 4
#define CTLINdescNumber  1
#define OUTdescNumber 4
#define INdescNumber 4

#define MAX_BULK_LEN 16384

#define READ6 0x08
#define WRITE6 0x0a

#define INQUIRY                                 0x12
#define READ_FORMAT_CAPACITIES                  0x23
#define READ10                                  0x28
#define WRITE10                                 0x2A
#define MODE_SENSE6                             0x1A
#define REQUEST_SENSE                           0x03
#define MODE_SELECT6                            0x15
#define MODE_SELECT10                           0x55
#define MODE_SENSE10                            0x5A
#define READ_CAPACITY                           0x25
#define START_STOP_UNIT                         0x1B
#define TEST_UNIT_READY                         0x00
#define READ_TOC                                0x43
#define READ_IO_BLOCK                           0x66
#define PREVENT_ALLOW_MEDIUM_REMOVAL            0x1E
#define GET_EVENT_STATUS_NOTIFICATION           0x4A
#define READ_DISC_INFORMATION                   0x51
#define GET_CONFIGURATION                       0x46
#define ERASE12                                 0xAC
#define READ_DVD_STRUCTURE                      0xAD
#define REPORT_KEY                              0xA4
#define SYNC_CACHE                              0x35
#define READ_TRACK_INFORMATION                  0x52

// MSC Test
#define READ12                                  0xA8
#define WRITE12                                 0xAA
#define WRITEANDVERIFY                          0x2E
#define VERIFY                                  0x2F

//Sense key
#define SK_NOT_READY                            0x02
#define SK_MEDIUM_ERROR                         0x03
#define SK_HARDWARE_ERROR                       0x04
#define SK_ILLEGAL_REQUEST                      0x05
#define SK_UNIT_ATTENTION                       0x06
#define SK_DATA_PROTECT                         0x07
#define ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE  0x21
#define ASCQ_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE 0x00
#define ASC_MEDIUM_NOT_PRESENT                  0x3a
#define ASCQ_MEDIUM_NOT_PRESENT                 0x00
#define ASC_PERIPHERAL_DEVICE_WRITE_FAULT       0x03
#define ASCQ_PERIPHERAL_DEVICE_WRITE_FAULT      0x00
#define ASC_UNRECOVERED_READ_ERROR              0x11
#define ASCQ_UNRECOVERED_READ_ERROR             0x00
#define ASC_WRITE_PROTECTED                     0x27
#define ASCQ_WRITE_PROTECTED                    0x00
#define ep0_mps                                 64

#define USB_PR_CBI	0x00		/* Control/Bulk/Interrupt */
#define USB_PR_CB	0x01		/* Control/Bulk w/o interrupt */
#define USB_PR_BULK	0x50		/* bulk only */
#define USB_PR_UAS	0x62		/* USB Attached SCSI */

#define USB_PR_USBAT	0x80		/* SCM-ATAPI bridge */
#define USB_PR_EUSB_SDDR09	0x81	/* SCM-SCSI bridge for SDDR-09 */
#define USB_PR_SDDR55	0x82		/* SDDR-55 (made up) */
#define USB_PR_DPCM_USB	0xf0		/* Combination CB/SDDR09 */
#define USB_PR_FREECOM	0xf1		/* Freecom */
#define USB_PR_DATAFAB	0xf2		/* Datafab chipsets */
#define USB_PR_JUMPSHOT	0xf3		/* Lexar Jumpshot */
#define USB_PR_ALAUDA	0xf4		/* Alauda chipsets */
#define USB_PR_KARMA	0xf5		/* Rio Karma */
#define USB_PR_DEVICE	0xff		/* Use device's value */

#define USB_SC_RBC	0x01		/* Typically, flash devices */
#define USB_SC_8020	0x02		/* CD-ROM */
#define USB_SC_QIC	0x03		/* QIC-157 Tapes */
#define USB_SC_UFI	0x04		/* Floppy */
#define USB_SC_8070	0x05		/* Removable media */
#define USB_SC_SCSI	0x06		/* Transparent */
#define USB_SC_LOCKABLE	0x07		/* Password-protected */

struct command_block_wrapper {
	u32   dCBWSignature;
	u32   dCBWTag;
	u32   dCBWDataTransferLength;
	u8    bmCBWFlags;
	u8    bCBWLUN : 4, Reserved :4 ;
	u8    bCBWCBLength : 5, Reserved2: 3;
	u8    rbc[16];
};

/* CSW data structure */
struct command_status_wrapper {
	u32 dCSWSignature;
	u32 dCSWTag;
	u32 dCSWDataResidue;
	u8  bCSWStatus;
};

typedef struct
{
	u32 length: 16, rsvd:11, stall:1, ls:1, fs:1, eor:1, own:1;
	dma_addr_t bufaddr;
}volatile outindesc_r;

static inline void rtl8117_ehci_writeb(u8 val, u32 addr)
{
	writeb(val, (volatile void __iomem *)addr);
}

static inline void rtl8117_ehci_writew(u16 val, u32 addr)
{
	writew(val, (volatile void __iomem *)addr);
}

static inline void rtl8117_ehci_writel(u32 val, u32 addr)
{
	writel(val, (volatile void __iomem *)addr);
}

static inline u32 ehci_readl(u32 addr)
{
	return readl((volatile void __iomem *)addr);
}

/* rtl8117-ehci*/
bool rtl8117_ehci_intr_handler(void);

void rtl8117_ehci_set_otg_power(bool on);
void rtl8117_set_ehci_enable(bool on);

void rtl8117_ehci_poweron_request(void);

/* rtl8117-ehci-core */
int rlt8117_ehci_core_init(void *platform_pdev);

void rtl8117_ehci_intep_enabled(u8 portnum);
void rtl8117_ehci_intep_disabled(u8 portnum);

void rtl8117_ehci_ep0_start_transfer(u16 len, u8 *addr, u8 is_in, bool stall);
void rtl8117_ehci_ep_start_transfer(u32 len, u8 *addr, u8 is_in, bool stall);

/* rtl8117-ehci-device */
int rtl8117_ehci_device_init(void);

void rtl8117_echi_control_request(struct usb_ctrlrequest *request);
void rtl8117_ehci_bulkout_request(void* ptr, u32 length);

int rtl8117_ehci_open_file(char *path);
int rtl8117_ehci_close_file(void);

#endif