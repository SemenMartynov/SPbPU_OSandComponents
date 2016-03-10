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

int init_module(void)
{
  struct pci_dev *pdev;
  pdev = probe_for_realtek8169();
  if (!pdev)
    return 0;

  return 0;
}
