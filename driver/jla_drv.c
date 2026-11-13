#include <linux/module.h>
#include <linux/pci.h>

#define JLA_VENDOR_ID 0x1234
#define JLA_DEVICE_ID 0x4a4c

// devices this driver binds to
static const struct pci_device_id jla_ids[] = {
    {PCI_DEVICE(JLA_VENDOR_ID, JLA_DEVICE_ID)}, {}};
MODULE_DEVICE_TABLE(pci, jla_ids);

// called when a matching device is found
static int jla_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
  int ret;

  ret = pcim_enable_device(pdev);
  if (ret)
    return ret;

  dev_info(&pdev->dev, "jla probed\n");
  return 0;
}

// called on unbind/rmmod
static void jla_remove(struct pci_dev *pdev) {
  dev_info(&pdev->dev, "jla removed\n");
}

static struct pci_driver jla_driver = {
    .name = "jla",
    .id_table = jla_ids,
    .probe = jla_probe,
    .remove = jla_remove,
};
module_pci_driver(jla_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("JLA job accelerator driver");
