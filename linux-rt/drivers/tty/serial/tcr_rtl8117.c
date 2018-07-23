#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/timekeeping.h>
#include <linux/kthread.h>

#define TCR_DLL                 0x02
#define TCR_DLM                 0x03
#define TCR_IER                 0x04
#define TCR_FCR                 0x06
#define TCR_MSR                 0x0A
#define TCR_CONF0               0x10
#define TCR_THR_DESC_START      0x11
#define TCR_THR_PKT_RD          0x12
#define TCR_RBR_DESC_START      0x13
#define TCR_TIMT                0x14
#define TCR_TIMPC               0x18
#define TCR_DESCADR             0x1C
#define TCR_THR_DATA_START      0x20
#define TCR_RBR_DATA_START      0x40
#define TCR_TPT                 0x60
#define TCR_RBR_IFG             0x68
#define TCR_IMR                 0x6C
#define TCR_ISR                 0x6E
#define SERIAL_RBR_TIMER        0x74

#define DescOWN 0x80000000
#define DescEOR 0x40000000
#define TCRTHRdescNumber 32
#define NETLINK_USER 31

struct TCRTxDesc {
    __le32      opts;
    __le32     Bufferaddr;
} ;
struct boot_options {
    u8 ori_code[6];
    u8 new_code[5];
    u8 len;
};
#if 0
struct boot_options boot_sel[15] =
{
    {"\x1B\x5B\x31\x31\x7E", "\x1B\x31", 2}, {"\x1B\x5B\x31\x32\x7E", "\x1B\x32", 2}, {"\x1B\x5B\x31\x33\x7E", "\x1B\x33", 2},
    {"\x1B\x5B\x31\x34\x7E", "\x1B\x34", 2}, {"\x1B\x5B\x31\x35\x7E", "\x1B\x35", 2}, {"\x1B\x5B\x31\x36\x7E", "\x1B\x36", 2},
    {"\x1B\x5B\x31\x37\x7E", "\x1B\x37", 2}, {"\x1B\x5B\x31\x38\x7E", "\x1B\x38", 2}, {"\x1B\x5B\x31\x39\x7E", "\x1B\x39", 2},
    {"\x1B\x5B\x32\x30\x7E", "\x1B\x30", 2}, {"\x1B\x5B\x32\x31\x7E", "\x1B\x21", 2}, {"\x1B\x5B\x32\x32\x7E", "\x1B\x40", 2},
    {"\x1B\x5B\x33\x7E", "\x1B-", 2}, {"\x1B\x5B\x32\x33\x7E", "\x1B\x21", 2}, {"\x1B\x5B\x32\x34\x7E", "\x1B\x40", 2},
};
#endif
struct boot_options boot_sel[] =
{
    {"\x1B\x5B\x31\x31\x7E", "\x1B\x31", 2}, {"\x1B\x5B\x31\x32\x7E", "\x1B\x32", 2}, {"\x1B\x5B\x31\x33\x7E", "\x1B\x33", 2},
    {"\x1B\x5B\x31\x34\x7E", "\x1B\x34", 2}, {"\x1B\x5B\x31\x35\x7E", "\x1B\x35", 2}, {"\x1B\x5B\x31\x37\x7E", "\x1B\x36", 2},
    {"\x1B\x5B\x31\x38\x7E", "\x1B\x37", 2}, {"\x1B\x5B\x31\x39\x7E", "\x1B\x38", 2}, {"\x1B\x5B\x32\x30\x7E", "\x1B\x39", 2},
    {"\x1B\x5B\x32\x31\x7E", "\x1B\x30", 2}, {"\x1B\x5B\x32\x33\x7E", "\x1B\x21", 2}, {"\x1B\x5B\x32\x34\x7E", "\x1B\x40", 2},
    {"\x1B\x5B\x33\x7E", "\x1B-", 2}
};

unsigned char * TCR_BASE_ADDR;
static struct TCRTxDesc  *tcr_txdesc=NULL;
dma_addr_t txdesc_mapping;
struct sock *nl_sk = NULL;
struct nlmsghdr *nlh=NULL;
int pid;
unsigned char *snd_buf_ori =NULL;
static unsigned int TCRRxIndex =  0;
struct task_struct *queue_th;
struct timer_list timer;
struct queue_ring
{
    unsigned int len;
    unsigned char *buf;
};
#define BUFFER_RING 128
struct queue_ring buffer_ring[BUFFER_RING];
static unsigned int enqueue_index=0;
static unsigned int dequeue_index = 0;
unsigned char * tx_buf[BUFFER_RING];
static irqreturn_t rtk_tcr_interrupt(int irq, void *dev_instance);

void rtk_tcr_show(void)
{
    int i=0;
    printk("******************tcr registers**********************\n");
    for (i=0; i<0xFF; i=i+4)
        printk("%4x: 0x%4x\n", i, readl(TCR_BASE_ADDR + i));
}

static void rtl8117_tcr_init(int irq, struct platform_device *pdev)
{
    u8 temp, i;
    unsigned int counter=0;
    dma_addr_t buffer_paddr;
    static unsigned int initdone =0;
    struct TCRTxDesc  * tcrp;

    tcrp=tcr_txdesc;
    writeb(0x00, TCR_BASE_ADDR + TCR_CONF0);
    do {
        udelay(10);
        temp = readb(TCR_BASE_ADDR + TCR_CONF0);
        counter++;
    } while((temp&0x01)!=0x00 && (counter<1000));

    writeb(0x01, TCR_BASE_ADDR + TCR_FCR);
    writel(0x00000100, TCR_BASE_ADDR + TCR_TPT);
    writel(0x00002500, TCR_BASE_ADDR + TCR_TIMT);
    writeb(0x10, TCR_BASE_ADDR + TCR_TIMPC);
    writel(0x0000000C, TCR_BASE_ADDR + TCR_RBR_IFG);
    writew(0xffff, TCR_BASE_ADDR + TCR_ISR);

    if (initdone ==0)
    {
        writel(txdesc_mapping, TCR_BASE_ADDR + TCR_DESCADR);
        for (i=0; i<TCRTHRdescNumber; i++)
        {
            dma_alloc_coherent(&pdev->dev, 0x70,
                               &buffer_paddr, GFP_KERNEL);

            tcrp->Bufferaddr= cpu_to_le32(buffer_paddr);
            if (i== TCRTHRdescNumber-1)
                tcrp->opts = cpu_to_le32(DescOWN | DescEOR | 0x70);
            else
                tcrp->opts = cpu_to_le32(DescOWN | 0x70);
            tcrp += 1;
        }
        request_irq(irq, rtk_tcr_interrupt, 0, "rtk-tcr", &(pdev->dev));
        initdone = 1;
    }
    else
    {
        for (i=0; i<TCRTHRdescNumber; i++)
        {
            if (i== TCRTHRdescNumber-1)
                tcrp->opts = cpu_to_le32(DescOWN | DescEOR | 0x70);
            else
                tcrp->opts = cpu_to_le32(DescOWN | 0x70);
            tcrp += 1;
        }
    }
    TCRRxIndex = 0;
    enqueue_index = dequeue_index = 0;
    writew(0x0006, TCR_BASE_ADDR + TCR_IMR);
    writeb(0xE8, TCR_BASE_ADDR + TCR_CONF0);
}
struct sock *netlink_getsockbyportid(struct sock *ssk, u32 portid);
static int rtk_enqueue_handler(void *data)
{
    do {
        __set_current_state(TASK_INTERRUPTIBLE);
        if (nlh != NULL)
        {
            struct sk_buff *skb;
            struct nlmsghdr *nlh_new=NULL;
            int res;
            unsigned int len=0;
            unsigned char * buf;
            unsigned char *snd_buf=NULL;
            struct sock *sk=NULL;
            snd_buf = snd_buf_ori;
            memset(snd_buf, 0, 0x80);

            while (dequeue_index != enqueue_index)
            {
                len += buffer_ring[dequeue_index].len;
                buf = buffer_ring[dequeue_index].buf;
                memcpy(snd_buf, buf, len);
                snd_buf = snd_buf + buffer_ring[dequeue_index].len;
                if ((len + buffer_ring[(dequeue_index + 1)%BUFFER_RING].len > 0x80) || (((dequeue_index + 1)%BUFFER_RING) == enqueue_index))
                {
retry:
                    skb = nlmsg_new(len, 0);
                    if(!skb)
                    {
                        mdelay(10);
                        printk(KERN_ERR "net_link: allocate failed, len is %d.\n", len);
                        goto retry;
                    }
                    nlh_new = nlmsg_put(skb,0,0,NLMSG_DONE,len,0);
                    if(nlh_new==NULL)
                    {
                        printk(KERN_ERR "nlh is NULL!\n\n");
                        //return IRQ_RETVAL(handled);;
                    }
                    NETLINK_CB(skb).portid = 0; /* from kernel */
                    NETLINK_CB(skb).dst_group = 0; /* not in mcast group */

                    memcpy(NLMSG_DATA(nlh_new), snd_buf_ori, len);
                    nlh_new->nlmsg_len = len;

                    sk = netlink_getsockbyportid(nl_sk, pid);
                    if (IS_ERR(sk)) {
                        enqueue_index = dequeue_index = 0;
                        break;
                    }
                    if (atomic_read(&sk->sk_rmem_alloc) > (sk->sk_rcvbuf -0x200))
                        atomic_set(&sk->sk_rmem_alloc, 0);
                    res = netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);
                    if (res < 0)
                    {
                        if (res == -ECONNREFUSED)
                        {
                            enqueue_index = dequeue_index = 0;
                            break;
                        }
                        printk(KERN_ERR "Error(%d) while sending back to user.\n", res);
                    }
                    len = 0;
                    snd_buf = snd_buf_ori;
                    memset(snd_buf_ori, 0, 0x80);
                }
                dequeue_index = (dequeue_index + 1)%BUFFER_RING;
            }
        }
        schedule();
    } while (!kthread_should_stop());

    return 0;
}

static irqreturn_t rtk_tcr_interrupt(int irq, void *dev_instance)
{
    int handled = 0;
    u16 status;
    struct TCRTxDesc *tcrthr;

    writew(0x0000, TCR_BASE_ADDR + TCR_IMR);
    status = readw(TCR_BASE_ADDR + TCR_ISR);
    writew(status, TCR_BASE_ADDR + TCR_ISR);

    //reset
    if ((status&0x4) != 0)
        rtl8117_tcr_init(0, NULL);

    //tok
    if ((status&0x2) != 0)
    {
        tcrthr = (struct TCRTxDesc  *) (tcr_txdesc) + TCRRxIndex;
        while (!(tcrthr->opts&DescOWN) && (tcrthr->opts&0x7FF))
        {
            memcpy(tx_buf[enqueue_index], (unsigned char *)tcrthr->Bufferaddr+0x80000000, tcrthr->opts&0x7FF);
            buffer_ring[enqueue_index].len = tcrthr->opts&0x7FF;
            buffer_ring[enqueue_index].buf = tx_buf[enqueue_index];
            enqueue_index = (enqueue_index + 1) % BUFFER_RING;
            tcrthr->opts = cpu_to_le32(DescOWN | (tcrthr->opts&DescEOR) | 0x70);
            TCRRxIndex = (TCRRxIndex + 1) % TCRTHRdescNumber;
            tcrthr = (struct TCRTxDesc *)(tcr_txdesc) + TCRRxIndex;
        }
    }
    handled = 1;
    writew(0x0006, TCR_BASE_ADDR + TCR_IMR);
    return IRQ_RETVAL(handled);
}

static void rtk_tcr_shutdown(struct platform_device *pdev)
{
    int i = 0;
    struct TCRTxDesc *tcrp;

    del_timer(&timer);
    kthread_stop(queue_th);
    tcrp=tcr_txdesc;
    for (i=0; i<TCRTHRdescNumber; i++)
    {
        dma_free_coherent(&pdev->dev, sizeof(struct TCRTxDesc )*TCRTHRdescNumber, tcrp->Bufferaddr|0x80000000, tcrp->Bufferaddr);
        tcrp += 1;
    }
    dma_free_coherent(&pdev->dev, sizeof(struct TCRTxDesc )*TCRTHRdescNumber, tcr_txdesc, txdesc_mapping);
    for (i=0; i<BUFFER_RING; i++)
    {
        kfree(tx_buf[i]);
    }

    kfree(snd_buf_ori);
}

static void tcr_queue_handler(unsigned long __opaque)
{
    wake_up_process(queue_th);
    mod_timer(&timer, jiffies + 1*HZ/100);
}

static int rtk_tcr_probe(struct platform_device *pdev)
{
    const u32 *prop;
    int size;
    int irq;
    int i = 0;

    prop = of_get_property(pdev->dev.of_node, "reg", &size);
    if ((prop) && (size))
        TCR_BASE_ADDR = of_iomap(pdev->dev.of_node, 0);

    irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
    tcr_txdesc = dma_alloc_coherent(&pdev->dev, sizeof(struct TCRTxDesc )*TCRTHRdescNumber,
                                    &txdesc_mapping, GFP_KERNEL);
    rtl8117_tcr_init(irq, pdev);

    enqueue_index = dequeue_index = 0;
    queue_th = kthread_run(rtk_enqueue_handler, pdev, "rtk_tcr");
    init_timer(&timer);
    timer.expires = jiffies + 1*HZ/100;
    timer.data = 0;
    timer.function = tcr_queue_handler;
    add_timer(&timer);
    snd_buf_ori = kmalloc(0x80, 0);
    for (i=0; i<BUFFER_RING; i++)
    {
        tx_buf[i] =kzalloc(0x70, 0);
    }
    return 0;
}

static struct of_device_id rtk_tcr_ids[] = {
    { .compatible = "realtek,rtl8117-tcr" },
    { /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, rtk_tcr_ids);

static struct platform_driver realtek_tcr_platdrv = {
    .driver = {
        .name   = "rtl8117-tcr",
        .owner  = THIS_MODULE,
        .of_match_table = rtk_tcr_ids,
    },
    .probe      = rtk_tcr_probe,
    .shutdown   = rtk_tcr_shutdown,
};

void tcr_received(unsigned char *buf, unsigned int len)
{
    volatile unsigned char *rxdesc = (unsigned char *) (TCR_BASE_ADDR + TCR_RBR_DESC_START);
    unsigned char *ptr = (unsigned char *) (TCR_BASE_ADDR + TCR_RBR_DATA_START);
    unsigned char sendlen;
    unsigned int count =0;
    int ind = 0;
    int boot_sel_len = sizeof(boot_sel)/sizeof(struct boot_options);

    if ((len == 5) || (len == 4) || (len == 13) || (len == 12))
    {
        /*for SSH protocol*/
        if (((len == 13) || (len == 12)) )
        {
            if (!memcmp(buf, "\x00\x00\x00\x00\x00\x00\x00\x05\x1B", 9) || !memcmp(buf, "\x00\x00\x00\x00\x00\x00\x00\x04\x1B", 9))
            {
                buf = (u8 *)buf + 8;
                len = len - 8;
            }
            else if (!memcmp(buf, "\x1B\x5B", 2))
                len = len - 8;
        }

        for (ind=0; ind< boot_sel_len; ind++)
        {
            if (!memcmp(buf, boot_sel[ind].ori_code, len))
            {
                len = boot_sel[ind].len;
                memcpy(buf, boot_sel[ind].new_code, len);
                break;
            }
        }
    }

    while (len)
    {
        sendlen = (len > 16) ? (16) :  (len);
        while ((*rxdesc & 0x80) == 0x80 && count < 1000)
        {
            count++;
            udelay(10);
        }

        writeb(0x01, TCR_BASE_ADDR + TCR_FCR);
        {
            memcpy(ptr, buf, sendlen);
            *rxdesc = (0x80 | sendlen);
        }
        buf += sendlen;
        len -= sendlen;
    }
}

static void rtk_nl_recv_msg(struct sk_buff *skb)
{
    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid;
    tcr_received((unsigned char *)nlmsg_data(nlh),  nlmsg_len(nlh));
}

static int __init rtk_tcr_init(void)
{
    int rc;
    //printk(KERN_INFO "Serial: rtk driver\n");

    struct netlink_kernel_cfg cfg = {
        .input = rtk_nl_recv_msg,
    };

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT "Error creating socket.\n");
        return -10;
    }

    rc = platform_driver_register(&realtek_tcr_platdrv);
    return rc;
}
module_init(rtk_tcr_init);

static void __exit rtk_tcr_exit(void)
{
    platform_driver_unregister(&realtek_tcr_platdrv);
    netlink_kernel_release(nl_sk);
}
module_exit(rtk_tcr_exit);
