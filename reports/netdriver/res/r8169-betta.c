#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/pci.h>

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

struct rtl8169_private
{
  struct pci_dev *pci_dev; /* PCI device */
  void *mmio_addr; /* memory mapped I/O addr */
  unsigned long regs_len; /* length of I/O or MMI/O region */
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

static int rtl8169_open(struct net_device *dev)
{
  LOG_MSG("rtl8169_open is called\n");
  return 0;
}

static int rtl8169_stop(struct net_device *dev)
{
  LOG_MSG("rtl8169_open is called\n");
  return 0;
}

static int rtl8169_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
  LOG_MSG("rtl8169_start_xmit is called\n");
  return 0;
}

static struct net_device_stats* rtl8169_get_stats(struct net_device *dev)
{
  LOG_MSG("rtl8169_get_stats is called\n");
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
