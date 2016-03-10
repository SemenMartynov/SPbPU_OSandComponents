#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/pci.h>

#define TX_BUF_SIZE  1536  /* should be at least MTU + 14 + 4 */
#define TOTAL_TX_BUF_SIZE  (TX_BUF_SIZE * NUM_TX_SIZE)

/* Size of the in-memory receive ring. */
#define RX_BUF_LEN_IDX 2         /* 0==8K, 1==16K, 2==32K, 3==64K */
#define RX_BUF_LEN     (8192 << RX_BUF_LEN_IDX)
#define RX_BUF_PAD     16           /* see 11th and 12th bit of RCR: 0x44 */
#define RX_BUF_WRAP_PAD 2048   /* spare padding to handle pkt wrap */
#define RX_BUF_TOT_LEN  (RX_BUF_LEN + RX_BUF_PAD + RX_BUF_WRAP_PAD)

/* 8169 register offsets */
#define TSD0          0x10
#define TSAD0         0x20
#define RBSTART       0x30
#define CR            0x37
#define CAPR          0x38
#define IMR           0x3c
#define ISR           0x3e
#define TCR           0x40
#define RCR           0x44
#define MPC           0x4c
#define MULINT        0x5c

/* TSD register commands */
#define TxHostOwns    0x2000
#define TxUnderrun    0x4000
#define TxStatOK      0x8000
#define TxOutOfWindow 0x20000000
#define TxAborted     0x40000000
#define TxCarrierLost 0x80000000

/* CR register commands */
#define RxBufEmpty 0x01
#define CmdTxEnb   0x04
#define CmdRxEnb   0x08
#define CmdReset   0x10

/* ISR Bits */
#define RxOK       0x01
#define RxErr      0x02
#define TxOK       0x04
#define TxErr      0x08
#define RxOverFlow 0x10
#define RxUnderrun 0x20
#define RxFIFOOver 0x40
#define CableLen   0x2000
#define TimeOut    0x4000
#define SysErr     0x8000

#define INT_MASK (RxOK | RxErr | TxOK | TxErr | \
               RxOverFlow | RxUnderrun | RxFIFOOver | \
               CableLen | TimeOut | SysErr)

static struct pci_dev* probe_for_realtek8169(void)
{
  struct pci_dev *pdev = NULL;
  /* Ensure we are not working on a non-PCI system */
  if (!pci_present())
  {
    LOG_MSG("<1>pci not present\n");
    return pdev;
  }

#define REALTEK_VENDER_ID  0x10EC
#define REALTEK_DEVICE_ID  0X8169

  /* Look for RealTek 8169 NIC */
  pdev = pci_find_device(REALTEK_VENDER_ID, REALTEK_DEVICE_ID, NULL);
  if (pdev)
  {
    /* device found, enable it */
    if (pci_enable_device(pdev))
    {
      LOG_MSG("Could not enable the device\n");
      return NULL;
    }
    else
      LOG_MSG("Device enabled\n");
  }
  else
  {
    LOG_MSG("device not found\n");
    return pdev;
  }
  return pdev;
}

#define NUM_TX_DESC 4
struct rtl8169_private
{
  struct pci_dev *pci_dev; /* PCI device */
  void *mmio_addr; /* memory mapped I/O addr */
  unsigned long regs_len; /* length of I/O or MMI/O region */
  unsigned int tx_flag;
  unsigned int cur_tx;
  unsigned int dirty_tx;
  unsigned char *tx_buf[NUM_TX_DESC]; /* Tx bounce buffers */
  unsigned char *tx_bufs; /* Tx bounce buffer region. */
  dma_addr_t tx_bufs_dma;

  struct net_device_stats stats;
  unsigned char *rx_ring;
  dma_addr_t rx_ring_dma;
  unsigned int cur_rx;
};

#define DRIVER "rtl8169"
static struct net_device *rtl8169_dev;

static int rtl8169_init(struct pci_dev *pdev, struct net_device **dev_out)
{
  struct net_device *dev;
  struct rtl8169_private *tp;

  /* 
   * alloc_etherdev allocates memory for dev and dev->priv.
   * dev->priv shall have sizeof(struct rtl8169_private) memory
   * allocated.
   */
  dev = alloc_etherdev(sizeof(struct rtl8169_private));
  if (!dev)
  {
    LOG_MSG("Could not allocate etherdev\n");
    return -1;
  }

  tp = dev->priv;
  tp->pci_dev = pdev;
  *dev_out = dev;

  return 0;
}

int init_module(void)
{
  struct pci_dev *pdev;
  unsigned long mmio_start, mmio_end, mmio_len, mmio_flags;
  void *ioaddr;
  struct rtl8169_private *tp;
  int i;

  pdev = probe_for_realtek8169();
  if (!pdev)
    return 0;

  if (rtl8169_init(pdev, &rtl8169_dev))
  {
    LOG_MSG("Could not initialize device\n");
    return 0;
  }

  tp = rtl8169_dev->priv; /* rtl8169 private information */

  /* get PCI memory mapped I/O space base address from BAR1 */
  mmio_start = pci_resource_start(pdev, 1);
  mmio_end = pci_resource_end(pdev, 1);
  mmio_len = pci_resource_len(pdev, 1);
  mmio_flags = pci_resource_flags(pdev, 1);

  /* make sure above region is MMI/O */
  if (!(mmio_flags & I / ORESOURCE_MEM))
  {
    LOG_MSG("region not MMI/O region\n");
    goto cleanup1;
  }

  /* get PCI memory space */
  if (pci_request_regions(pdev, DRIVER))
  {
    LOG_MSG("Could not get PCI region\n");
    goto cleanup1;
  }

  pci_set_master(pdev);

  /* ioremap MMI/O region */
  ioaddr = ioremap(mmio_start, mmio_len);
  if (!ioaddr)
  {
    LOG_MSG("Could not ioremap\n");
    goto cleanup2;
  }

  rtl8169_dev->base_addr = (long) ioaddr;
  tp->mmio_addr = ioaddr;
  tp->regs_len = mmio_len;

  /* UPDATE NET_DEVICE */

  for (i = 0; i < 6; i++)
  { /* Hardware Address */
    rtl8169_dev->dev_addr[i] = readb(rtl8169_dev->base_addr + i);
    rtl8169_dev->broadcast[i] = 0xff;
  }
  rtl8169_dev->hard_header_len = 14;

  memcpy(rtl8169_dev->name, DRIVER, sizeof(DRIVER)); /* Device Name */
  rtl8169_dev->irq = pdev->irq; /* Interrupt Number */
  rtl8169_dev->open = rtl8169_open;
  rtl8169_dev->stop = rtl8169_stop;
  rtl8169_dev->hard_start_xmit = rtl8169_start_xmit;
  rtl8169_dev->get_stats = rtl8169_get_stats;

  /* register the device */
  if (register_netdev(rtl8169_dev))
  {
    LOG_MSG("Could not register netdevice\n");
    goto cleanup0;
  }

  return 0;
}

void cleanup_module(void)
{
  struct rtl8169_private *tp;
  tp = rtl8169_dev->priv;

  iounmap(tp->mmio_addr);
  pci_release_regions(tp->pci_dev);

  unregister_netdev(rtl8169_dev);
  pci_disable_device(tp->pci_dev);
  return;
}

static int rtl8169_open(struct net_device *dev)
{
  int retval;
  struct rtl8169_private *tp = dev->priv;

  /* get the IRQ
   * second arg is interrupt handler
   * third is flags, 0 means no IRQ sharing
   */
  retval = request_irq(dev->irq, rtl8169_interrupt, 0, dev->name, dev);
  if (retval)
    return retval;

  /* get memory for Tx buffers
   * memory must be DMAable
   */
  tp->tx_bufs = pci_alloc_consistent(tp->pci_dev, TOTAL_TX_BUF_SIZE,
      &tp->tx_bufs_dma);
  tp->rx_ring = pci_alloc_consistent(tp->pci_dev, RX_BUF_TOT_LEN,
      &tp->rx_ring_dma);

  if ((!tp->tx_bufs) || (!tp->rx_ring))
  {
    free_irq(dev->irq, dev);

    if (tp->tx_bufs)
    {
      pci_free_consistent(tp->pci_dev, TOTAL_TX_BUF_SIZE, tp->tx_bufs,
          tp->tx_bufs_dma);
      tp->tx_bufs = NULL;
    }
    if (tp->rx_ring)
    {
      pci_free_consistent(tp->pci_dev, RX_BUF_TOT_LEN, tp->rx_ring,
          tp->rx_ring_dma);
      tp->rx_ring = NULL;
    }
    return -ENOMEM;
  }

  tp->tx_flag = 0;
  rtl8169_init_ring(dev);
  rtl8169_hw_start(dev);

  return 0;
}

static void rtl8169_init_ring(struct net_device *dev)
{
  struct rtl8169_private *tp = dev->priv;
  int i;

  tp->cur_tx = 0;
  tp->dirty_tx = 0;

  for (i = 0; i < NUM_TX_DESC; i++)
    tp->tx_buf[i] = &tp->tx_bufs[i * TX_BUF_SIZE];

  return;
}

static void rtl8169_hw_start(struct net_device *dev)
{
  struct rtl8169_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  u32 i;

  rtl8169_chip_reset(ioaddr);

  /* Must enable Tx/Rx before setting transfer thresholds! */
  writeb(CmdTxEnb | CmdRxEnb, ioaddr + CR);

  /* tx config */
  writel(0x00000600, ioaddr + TCR); /* DMA burst size 1024 */

  /* rx config */
  writel(((1 << 12) | (7 << 8) | (1 << 7) | (1 << 3) | (1 << 2) | (1 << 1)),
      ioaddr + RCR);

  /* init Tx buffer DMA addresses */
  for (i = 0; i < NUM_TX_DESC; i++)
  {
    writel(tp->tx_bufs_dma + (tp->tx_buf[i] - tp->tx_bufs),
        ioaddr + TSAD0 + (i * 4));
  }

  /* init RBSTART */
  writel(tp->rx_ring_dma, ioaddr + RBSTART);

  /* initialize missed packet counter */
  writel(0, ioaddr + MPC);

  /* no early-rx interrupts */
  writew((readw(ioaddr + MULINT) & 0xF000), ioaddr + MULINT);

  /* Enable all known interrupts by setting the interrupt mask. */
  writew(INT_MASK, ioaddr + IMR);

  netif_start_queue(dev);
  return;
}

static void rtl8169_chip_reset(void *ioaddr)
{
  int i;

  /* Soft reset the chip. */
  writeb(CmdReset, ioaddr + CR);

  /* Check that the chip has finished the reset. */
  for (i = 1000; i > 0; i--)
  {
    barrier();
    if ((readb(ioaddr + CR) & CmdReset) == 0)
      break;
    udelay(10);
  }
  return;
}

static int rtl8169_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
  struct rtl8169_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  unsigned int entry = tp->cur_tx;
  unsigned int len = skb->len;
#define ETH_MIN_LEN 60  /* minimum Ethernet frame size */
  if (len < TX_BUF_SIZE)
  {
    if (len < ETH_MIN_LEN)
      memset(tp->tx_buf[entry], 0, ETH_MIN_LEN);
    skb_copy_and_csum_dev(skb, tp->tx_buf[entry]);
    dev_kfree_skb(skb);
  }
  else
  {
    dev_kfree_skb(skb);
    return 0;
  }
  writel(tp->tx_flag | max(len, (unsigned int) ETH_MIN_LEN),
      ioaddr + TSD0 + (entry * sizeof(u32)));
  entry++;
  tp->cur_tx = entry % NUM_TX_DESC;

  if (tp->cur_tx == tp->dirty_tx)
  {
    netif_stop_queue(dev);
  }
  return 0;
}

static void rtl8169_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
{
  struct net_device *dev = (struct net_device*) dev_instance;
  struct rtl8169_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  unsigned short isr = readw(ioaddr + ISR);

  /* clear all interrupt.
   * Specs says reading ISR clears all interrupts and writing
   * has no effect. But this does not seem to be case. I keep on
   * getting interrupt unless I forcibly clears all interrupt :-(
   */
  writew(0xffff, ioaddr + ISR);

  if ((isr & TxOK) || (isr & TxErr))
  {
    while ((tp->dirty_tx != tp->cur_tx) || netif_queue_stopped(dev))
    {
      unsigned int txstatus = readl(
          ioaddr + TSD0 + tp->dirty_tx * sizeof(int));

      if (!(txstatus & (TxStatOK | TxAborted | TxUnderrun)))
        break; /* yet not transmitted */

      if (txstatus & TxStatOK)
      {
        LOG_MSG("Transmit OK interrupt\n");
        tp->stats.tx_bytes += (txstatus & 0x1fff);
        tp->stats.tx_packets++;
      }
      else
      {
        LOG_MSG("Transmit Error interrupt\n");
        tp->stats.tx_errors++;
      }

      tp->dirty_tx++;
      tp->dirty_tx = tp->dirty_tx % NUM_TX_DESC;

      if ((tp->dirty_tx == tp->cur_tx) & netif_queue_stopped(dev))
      {
        LOG_MSG("waking up queue\n");
        netif_wake_queue(dev);
      }
    }
  }

  if (isr & RxErr)
  {
    /* TODO: Need detailed analysis of error status */
    LOG_MSG("receive err interrupt\n");
    tp->stats.rx_errors++;
  }

  if (isr & RxOK)
  {
    LOG_MSG("receive interrupt received\n");
    while ((readb(ioaddr + CR) & RxBufEmpty) == 0)
    {
      unsigned int rx_status;
      unsigned short rx_size;
      unsigned short pkt_size;
      struct sk_buff *skb;

      if (tp->cur_rx > RX_BUF_LEN)
        tp->cur_rx = tp->cur_rx % RX_BUF_LEN;

      /* TODO: need to convert rx_status from little to host endian
       * XXX: My CPU is little endian only :-)
       */
      rx_status = *(unsigned int*) (tp->rx_ring + tp->cur_rx);
      rx_size = rx_status >> 16;

      /* first two bytes are receive status register 
       * and next two bytes are frame length
       */
      pkt_size = rx_size - 4;

      /* hand over packet to system */
      skb = dev_alloc_skb(pkt_size + 2);
      if (skb)
      {
        skb->dev = dev;
        skb_reserve(skb, 2); /* 16 byte align the IP fields */

        eth_copy_and_sum(skb, tp->rx_ring + tp->cur_rx + 4, pkt_size,
            0);

        skb_put(skb, pkt_size);
        skb->protocol = eth_type_trans(skb, dev);
        netif_rx(skb);

        dev->last_rx = jiffies;
        tp->stats.rx_bytes += pkt_size;
        tp->stats.rx_packets++;
      }
      else
      {
        LOG_MSG("Memory squeeze, dropping packet.\n");
        tp->stats.rx_dropped++;
      }

      /* update tp->cur_rx to next writing location  */
       tp->cur_rx = (tp->cur_rx + rx_size + 4 + 3) & ~3;

       /* update CAPR */
      writew(tp->cur_rx, ioaddr + CAPR);
    }
  }

  if (isr & CableLen)
    LOG_MSG("cable length change interrupt\n");
  if (isr & TimeOut)
    LOG_MSG("time interrupt\n");
  if (isr & SysErr)
    LOG_MSG("system err interrupt\n");
  return;
}

static struct net_device_stats* rtl8169_get_stats(struct net_device *dev)
{
  struct rtl8169_private *tp = dev->priv;
  return &(tp->stats);
}
