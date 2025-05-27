/* dummy_uio_full.c - Full-featured UIO driver with MMIO, MSI-X and interrupt support */
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uio_driver.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/msi.h>

#define PCI_VENDOR_ID_DUMMY 0x1234
#define PCI_DEVICE_ID_DUMMY 0x11E8

#define DRIVER_NAME "dummy_uio_full"

static struct uio_info dummy_uio_info;
static void __iomem *mmio_base;
static int dummy_irq;

/* Optional custom IRQ control (from userspace write to /dev/uioX) */
static int dummy_irqcontrol(struct uio_info *info, s32 irq_on)
{
    if (irq_on)
        enable_irq(dummy_irq);
    else
        disable_irq(dummy_irq);
    return 0;
}

/* IRQ handler */
static irqreturn_t dummy_handler(int irq, void *dev_id)
{
    struct uio_info *info = dev_id;
    return IRQ_HANDLED;
}

static int dummy_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int ret;
    resource_size_t mmio_start, mmio_len;

    ret = pci_enable_device(pdev);
    if (ret)
        return ret;

    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret)
        goto err_disable_device;

    mmio_start = pci_resource_start(pdev, 0);
    mmio_len = pci_resource_len(pdev, 0);

    mmio_base = pci_ioremap_bar(pdev, 0);
    if (!mmio_base)
    {
        ret = -ENOMEM;
        goto err_release_regions;
    }

    ret = pci_enable_msix_exact(pdev, (struct msix_entry[]){{.entry = 0}}, 1);
    if (ret)
    {
        pr_info("MSI-X not available, falling back to legacy IRQ\n");
        dummy_irq = pdev->irq;
    }
    else
    {
        dummy_irq = pdev->msix_entries[0].vector;
    }

    ret = request_irq(dummy_irq, dummy_handler, 0, DRIVER_NAME, &dummy_uio_info);
    if (ret)
        goto err_iounmap;

    dummy_uio_info.name = DRIVER_NAME;
    dummy_uio_info.version = "1.0";
    dummy_uio_info.mem[0].addr = mmio_start;
    dummy_uio_info.mem[0].size = mmio_len;
    dummy_uio_info.mem[0].memtype = UIO_MEM_PHYS;
    dummy_uio_info.irq = dummy_irq;
    dummy_uio_info.irq_flags = 0;
    dummy_uio_info.irqcontrol = dummy_irqcontrol;

    ret = uio_register_device(&pdev->dev, &dummy_uio_info);
    if (ret)
        goto err_free_irq;

    pci_set_drvdata(pdev, &dummy_uio_info);
    return 0;

err_free_irq:
    free_irq(dummy_irq, &dummy_uio_info);
err_iounmap:
    iounmap(mmio_base);
err_release_regions:
    pci_release_regions(pdev);
err_disable_device:
    pci_disable_device(pdev);
    return ret;
}

static void dummy_remove(struct pci_dev *pdev)
{
    struct uio_info *info = pci_get_drvdata(pdev);
    uio_unregister_device(info);
    free_irq(dummy_irq, info);
    iounmap(mmio_base);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    pci_disable_msix(pdev);
}

static const struct pci_device_id dummy_ids[] = {
    {
        PCI_DEVICE(PCI_VENDOR_ID_DUMMY, PCI_DEVICE_ID_DUMMY),
    },
    {
        0,
    }};
MODULE_DEVICE_TABLE(pci, dummy_ids);

static struct pci_driver dummy_uio_driver = {
    .name = DRIVER_NAME,
    .id_table = dummy_ids,
    .probe = dummy_probe,
    .remove = dummy_remove,
};

module_pci_driver(dummy_uio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Complex UIO PCI driver with MMIO, MSI-X and IRQ support");

/*
==========================
💡 UIO DRIVER LOAD STEPS
==========================

1. Build the module:
   make -C /lib/modules/$(uname -r)/build M=$(pwd) modules

2. Load the driver:
   sudo insmod dummy_uio_full.ko

3. Unbind current driver (if any):
   echo -n 0000:00:19.0 > /sys/bus/pci/devices/0000:00:19.0/driver/unbind

4. Add your device ID:
   echo "1234 11e8" > /sys/bus/pci/drivers/dummy_uio_full/new_id

5. Bind the device (this triggers probe):
   echo -n 0000:00:19.0 > /sys/bus/pci/drivers/dummy_uio_full/bind

6. Verify:
   ls /dev/uio*
   cat /sys/class/uio/uio0/maps/map0/addr

7. From userspace:
   - `mmap()` /dev/uioX to access MMIO
   - `read()` /dev/uioX to wait for interrupts
   - `write(1)` to enable, `write(0)` to disable interrupts
*/
