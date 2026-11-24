#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "jla.h"

#define RING_SIZE 64

struct jla_dev {
  struct pci_dev *pdev;
  void __iomem *regs;

  struct jla_job *sq; // submission ring
  dma_addr_t sq_dma;
  u32 ring_size;
  u32 sq_tail;
};

// devices this driver binds to
static const struct pci_device_id jla_ids[] = {
    {PCI_DEVICE(JLA_VENDOR_ID, JLA_DEVICE_ID)}, {}};
MODULE_DEVICE_TABLE(pci, jla_ids);

// write a job into ring + ring doorbell
static void jla_submit(struct jla_dev *dev, u32 job_id, u64 duration,
                       u64 estimate_duration) {
  struct jla_job *j = &dev->sq[dev->sq_tail];

  memset(j, 0, sizeof(*j));
  j->job_id = cpu_to_le32(job_id);
  j->kernel_id = cpu_to_le16(0); // set to 0 for now
  j->flags = cpu_to_le16(0);
  j->duration = cpu_to_le64(duration);
  j->estimate_duration = cpu_to_le64(estimate_duration);

  dev->sq_tail = (dev->sq_tail + 1) & (dev->ring_size - 1);

  writel(dev->sq_tail, dev->regs + JLA_REG_SQ_TAIL);
}

static void jla_test_jobs(struct jla_dev *dev) {
  jla_submit(dev, 42, 1000, 1100);
  jla_submit(dev, 43, 2000, 1900);
  jla_submit(dev, 44, 3000, 3000);

  msleep(20);

  dev_info(&dev->pdev->dev, "SQ_HEAD=%u, status=0x%08x\n",
           readl(dev->regs + JLA_REG_SQ_HEAD),
           readl(dev->regs + JLA_REG_STATUS));
}

// tell device where ring is
static void jla_setup_ring(struct jla_dev *dev) {
  writel(JLA_CONTROL_ENABLE, dev->regs + JLA_REG_CONTROL);
  writel(lower_32_bits(dev->sq_dma), dev->regs + JLA_REG_SQ_BASE_LO);
  writel(upper_32_bits(dev->sq_dma), dev->regs + JLA_REG_SQ_BASE_HI);
  writel(dev->ring_size, dev->regs + JLA_REG_RING_SIZE);
  dev->sq_tail = 0;
}

// called when a matching device is found
static int jla_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
  struct jla_dev *dev;
  u32 dev_id;
  int ret;

  ret = pcim_enable_device(pdev);
  if (ret)
    return ret;

  // allocate zero-initialised memory (freed on unbind)
  dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
  if (!dev)
    return -ENOMEM;
  dev->pdev = pdev;

  // map BAR0
  dev->regs = pcim_iomap_region(pdev, 0, "jla");
  if (IS_ERR(dev->regs))
    return PTR_ERR(dev->regs);

  // verify ID
  dev_id = readl(dev->regs + JLA_REG_ID);
  if (dev_id != JLA_ID_VALUE) {
    dev_err(&pdev->dev, "bad ID 0x%08x (expected 0x%08x)\n", dev_id,
            JLA_ID_VALUE);
    return -ENODEV;
  }

  dev_info(&pdev->dev, "device probed, version 0x%08x\n",
           readl(dev->regs + JLA_REG_VERSION));

  // allow DMA
  pci_set_master(pdev);
  ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
  if (ret)
    return ret;

  // allocate submission ring (freed on unbind)
  dev->ring_size = RING_SIZE;
  dev->sq = dmam_alloc_coherent(&pdev->dev, dev->ring_size * sizeof(*dev->sq),
                                &dev->sq_dma, GFP_KERNEL);

  if (!dev->sq)
    return -ENOMEM;

  pci_set_drvdata(pdev, dev);
  jla_setup_ring(dev);

  jla_test_jobs(dev);

  return 0;
}

// called on unbind/rmmod
static void jla_remove(struct pci_dev *pdev) {
  struct jla_dev *dev = pci_get_drvdata(pdev);

  // stop device before memory freed
  writel(JLA_CONTROL_RESET, dev->regs + JLA_REG_CONTROL);
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
