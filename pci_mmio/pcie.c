#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#define MY_VENDOR_ID 0x1234
#define MY_DEVICE_ID 0x5678

static struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(MY_VENDOR_ID, MY_DEVICE_ID), },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids); // needed for hot plug

static void __iomem *mmio_base;
static int irq_line;

// TOP half
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    pr_info("pci_irq_driver: Interrupt received on IRQ %d\n", irq);
    // Acknowledge interrupt from device if needed
    return IRQ_HANDLED; // needed or multiple are probed and it won't stop
}

static int pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int err;
    resource_size_t mmio_start, mmio_len;

    err = pci_enable_device(pdev);
    if (err) return err;

    mmio_start = pci_resource_start(pdev, 0);
    mmio_len   = pci_resource_len(pdev, 0);

    err = pci_request_region(pdev, 0, "pci_irq_driver");
    if (err) {
        pci_disable_device(pdev);
        return err;
    }

    mmio_base = ioremap(mmio_start, mmio_len);
    if (!mmio_base) {
        pci_release_region(pdev, 0);
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    irq_line = pdev->irq;
    // or irq_line = pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq_num);
    err = request_irq(irq_line, irq_handler, IRQF_SHARED, "pci_irq_driver", pdev);
    if (err) {
        iounmap(mmio_base);
        pci_release_region(pdev, 0);
        pci_disable_device(pdev);
        return err;
    }

    pr_info("pci_irq_driver loaded: MMIO=%pa, IRQ=%d\n", &mmio_start, irq_line);
    return 0;
}

static void pci_remove(struct pci_dev *pdev)
{
    free_irq(irq_line, pdev);
    iounmap(mmio_base);
    pci_release_region(pdev, 0);
    pci_disable_device(pdev);
    pr_info("pci_irq_driver unloaded.\n");
}

static struct pci_driver pci_irq_driver = {
    .name     = "pci_irq_driver",
    .id_table = pci_ids,
    .probe    = pci_probe,
    .remove   = pci_remove,
};

// Manual init/exit instead of module_pci_driver
static int __init pci_irq_init(void)
{
    return pci_register_driver(&pci_irq_driver);
}

static void __exit pci_irq_exit(void)
{
    pci_unregister_driver(&pci_irq_driver);
}

module_init(pci_irq_init);
module_exit(pci_irq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("PCI Driver with IRQ (no DMA)");
