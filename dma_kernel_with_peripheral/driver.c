#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>          // PCI bus support
#include <linux/slab.h>         // kmalloc/kfree
#include <linux/io.h>           // ioremap/iounmap
#include <linux/dma-mapping.h>  // DMA API
#include <linux/delay.h>        // mdelay (for simulation delay)
#include <linux/version.h>      // For kernel version checks
#include <linux/interrupt.h>    // Interrupt handling
#include <linux/sched.h>        // Tasklets

// Define a dummy PCI device ID for our simulated device
// In a real driver, this would match your hardware's Vendor and Device ID.
#define MY_DEVICE_VENDOR_ID   0x1234 // Replace with actual Vendor ID
#define MY_DEVICE_DEVICE_ID   0x5678 // Replace with actual Device ID

// Simulated Device MMIO Registers Structure
// This represents how the device's control registers might look in memory.
// The driver writes to these addresses via the ioremapped pointer.
struct my_device_regs {
    u32 control;        // Control register (e.g., bit 0: Start DMA, bit 1: Direction)
    u32 status;         // Status register (e.g., bit 0: DMA Busy, bit 1: Completion, bit 2: Interrupt Enable)
    u64 dma_addr;       // DMA buffer bus address (64-bit)
    u32 dma_len;        // DMA transfer length in bytes
    u32 reserved;       // Padding to align 64-bit dma_addr
    // Add other simulated registers as needed for your device
};

// Control register bits (simulated)
#define MY_DEVICE_DMA_START_BIT  (1 << 0)
#define MY_DEVICE_DMA_DIR_BIT    (1 << 1) // 0: To Device, 1: From Device

// Status register bits (simulated)
#define MY_DEVICE_DMA_BUSY_BIT   (1 << 0)
#define MY_DEVICE_DMA_DONE_BIT   (1 << 1)
#define MY_DEVICE_IRQ_ENABLE_BIT (1 << 2) // Simulated interrupt enable bit

// Size of the DMA buffer we will allocate
#define DMA_BUFFER_SIZE (4 * PAGE_SIZE) // Allocate 4 pages

// Structure to hold our device-specific data
struct my_device_priv {
    struct pci_dev *pdev;           // Pointer to the PCI device structure
    void __iomem *regs_base;        // Base address of mapped MMIO registers
    struct my_device_regs *regs;    // Pointer to the simulated registers structure

    void *dma_buffer_virt;          // Kernel virtual address of DMA buffer
    dma_addr_t dma_buffer_phys;     // DMA (bus) address of DMA buffer

    int irq;                        // Interrupt line number
    struct tasklet_struct tasklet;  // Tasklet for bottom-half processing

    // Add other private data as needed
};

// Tasklet handler function (bottom half)
static void my_device_tasklet_handler(unsigned long data)
{
    struct my_device_priv *priv = (struct my_device_priv *)data;

    pr_info("my_device: Tasklet handler executed (Bottom Half)\n");

    // --- DMA Completion Logic ---
    // This is where you would handle the completion of the DMA transfer.
    // This runs in a non-atomic context, so you can do more work here
    // than in the ISR.

    // Example: Check the simulated status register (already cleared in ISR)
    // u32 status = ioread32(&priv->regs->status);
    // if (status & MY_DEVICE_DMA_DONE_BIT) {
        pr_info("my_device: DMA transfer completion processed in tasklet.\n");
        // Signal completion to user space (e.g., wake up a waiting process)
        // ...
    // }

    // In a real driver, you might process received data, update state, etc.
}


// Interrupt Service Routine (ISR) - Top Half
static irqreturn_t my_device_isr(int irq, void *dev_id)
{
    struct my_device_priv *priv = (struct my_device_priv *)dev_id;
    u32 status;

    // Read the device's status register to check for interrupts
    status = ioread32(&priv->regs->status);

    // Check if this interrupt is for our device and if DMA is done (simulated)
    // In a real device, you'd check a specific interrupt status bit.
    if (status & MY_DEVICE_DMA_DONE_BIT) {
        pr_info("my_device: Interrupt received (Top Half)\n");

        // Acknowledge the interrupt on the device (clear the status bit)
        // This prevents the interrupt from firing again immediately.
        // In a real device, this is a specific register write.
        iowrite32(status & ~MY_DEVICE_DMA_DONE_BIT, &priv->regs->status);
        pr_info("my_device: Simulated interrupt acknowledged.\n");

        // Schedule the tasklet for bottom-half processing
        tasklet_schedule(&priv->tasklet);

        return IRQ_HANDLED; // Indicate that we handled the interrupt
    }

    // If the interrupt was not for our device, return IRQ_NONE
    return IRQ_NONE;
}


// Simulated DMA transfer function
// This function simulates the hardware performing the DMA transfer.
// It now triggers a simulated interrupt upon completion.
static void my_device_simulate_dma(struct my_device_priv *priv)
{
    u32 control = ioread32(&priv->regs->control);
    u64 dma_addr = ioread64(&priv->regs->dma_addr);
    u32 dma_len = ioread32(&priv->regs->dma_len);
    int direction = (control & MY_DEVICE_DMA_DIR_BIT) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

    pr_info("my_device: Simulating DMA transfer:\n");
    pr_info("  Direction: %s\n", (direction == DMA_FROM_DEVICE) ? "From Device" : "To Device");
    pr_info("  DMA Address: 0x%llx\n", dma_addr);
    pr_info("  Length: %u bytes\n", dma_len);

    // --- Simulation of data transfer ---
    if (priv->dma_buffer_virt && dma_len <= DMA_BUFFER_SIZE) {
        if (direction == DMA_TO_DEVICE) {
            pr_info("  (Simulating device reading from buffer at virt 0x%px)\n", priv->dma_buffer_virt);
            // ... simulate read ...
        } else { // DMA_FROM_DEVICE
            pr_info("  (Simulating device writing to buffer at virt 0x%px)\n", priv->dma_buffer_virt);
            memset(priv->dma_buffer_virt, 0xAA, dma_len); // Simulate data written by device
            pr_info("  (Simulated buffer fill complete)\n");
        }

        // In a real driver, you would wait for the hardware to generate an interrupt here.
        // For this simulation, we'll add a small delay and then manually trigger the ISR.
        mdelay(10); // Simulate transfer time

        // --- Simulate Hardware Generating Interrupt ---
        // Set the done bit in the status register (simulating hardware)
        iowrite32(MY_DEVICE_DMA_DONE_BIT | MY_DEVICE_IRQ_ENABLE_BIT, &priv->regs->status);

        pr_info("my_device: Simulating hardware interrupt generation...\n");
        // Manually call the ISR for simulation purposes.
        // In a real system, the kernel calls the ISR when the interrupt occurs.
        my_device_isr(priv->irq, priv);

    } else {
        pr_err("my_device: Simulated DMA failed: invalid buffer or length.\n");
        iowrite32(0, &priv->regs->status); // Clear status or set error bit
    }

    // The start bit is typically cleared by the hardware upon completion or by the driver
    // after processing the completion. We'll clear it in the tasklet handler in a real driver.
    // For this simulation, we'll clear it here for simplicity after the simulated ISR call.
    iowrite32(control & ~MY_DEVICE_DMA_START_BIT, &priv->regs->control);
}


// PCI device probe function
static int my_device_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int ret;
    struct my_device_priv *priv;
    unsigned long mmio_start, mmio_len;

    pr_info("my_device: Probe function called for Vendor ID 0x%x, Device ID 0x%x\n",
            pdev->vendor, pdev->device);

    // 1. Enable the PCI device
    ret = pci_enable_device(pdev);
    if (ret) {
        pr_err("my_device: Failed to enable PCI device: %d\n", ret);
        return ret;
    }

    // Allocate private data structure
    priv = devm_kzalloc(&pdev->dev, sizeof(struct my_device_priv), GFP_KERNEL);
    if (!priv) {
        pr_err("my_device: Failed to allocate device private data\n");
        ret = -ENOMEM;
        goto disable_pci;
    }
    pci_set_drvdata(pdev, priv); // Store private data in pci_dev
    priv->pdev = pdev;

    // 2. Request and map the device's MMIO region (BAR)
    ret = pci_request_region(pdev, 0, "my_device_mmio");
    if (ret) {
        pr_err("my_device: Failed to request MMIO region (BAR 0): %d\n", ret);
        goto free_priv;
    }

    mmio_start = pci_resource_start(pdev, 0);
    mmio_len = pci_resource_len(pdev, 0);

    pr_info("my_device: MMIO region BAR 0: start=0x%lx, len=0x%lx\n",
            mmio_start, mmio_len);

    priv->regs_base = ioremap(mmio_start, mmio_len);
    if (!priv->regs_base) {
        pr_err("my_device: Failed to ioremap MMIO region\n");
        ret = -EIO;
        goto release_region;
    }
    priv->regs = (struct my_device_regs *)priv->regs_base;

    pr_info("my_device: MMIO region mapped to kernel virt address: 0x%px\n", priv->regs_base);

    // 3. Allocate DMA-coherent buffer
    priv->dma_buffer_virt = dma_alloc_coherent(&pdev->dev, DMA_BUFFER_SIZE,
                                               &priv->dma_buffer_phys, GFP_KERNEL);
    if (!priv->dma_buffer_virt) {
        pr_err("my_device: Failed to allocate DMA coherent buffer\n");
        ret = -ENOMEM;
        goto iounmap_regs;
    }

    pr_info("my_device: Allocated DMA buffer: virt=0x%px, phys=0x%llx\n",
            priv->dma_buffer_virt, (unsigned long long)priv->dma_buffer_phys);

    memset(priv->dma_buffer_virt, 0, DMA_BUFFER_SIZE);
    pr_info("my_device: DMA buffer initialized.\n");

    // 4. Request Interrupt
    // Get the IRQ number for the device
    priv->irq = pci_irq_vector(pdev, 0); // Get the first IRQ vector
    if (priv->irq < 0) {
        pr_err("my_device: Failed to get IRQ vector: %d\n", priv->irq);
        ret = priv->irq;
        goto free_dma_buffer;
    }

    // Initialize the tasklet
    tasklet_init(&priv->tasklet, my_device_tasklet_handler, (unsigned long)priv);
    pr_info("my_device: Tasklet initialized.\n");

    // Request the interrupt line
    // IRQF_SHARED allows sharing the IRQ with other devices (if applicable)
    // "my_device" is the name that will appear in /proc/interrupts
    // priv is the dev_id that will be passed to the ISR
    ret = request_irq(priv->irq, my_device_isr, IRQF_SHARED, "my_device", priv);
    if (ret) {
        pr_err("my_device: Failed to request IRQ %d: %d\n", priv->irq, ret);
        goto free_dma_buffer; // No need to kill tasklet if request_irq failed
    }
    pr_info("my_device: Requested IRQ %d\n", priv->irq);

    // --- Simulate Programming Device Registers and Starting DMA ---
    pr_info("my_device: Simulating programming device registers...\n");

    // 5. Program Device's DMA Registers (Simulated via mapped MMIO)
    iowrite64(priv->dma_buffer_phys, &priv->regs->dma_addr);
    iowrite32(DMA_BUFFER_SIZE, &priv->regs->dma_len);

    // Clear control register first
    iowrite32(0, &priv->regs->control);
    // Set direction bit (simulated) and enable interrupts on the device (simulated)
    iowrite32(MY_DEVICE_DMA_DIR_BIT | MY_DEVICE_IRQ_ENABLE_BIT, &priv->regs->control);
    // Ensure writes are flushed (important for MMIO)
    wmb(); // Write memory barrier

    // 6. Trigger DMA Start (Simulated via mapped MMIO)
    // Set the start bit in the control register (simulated)
    iowrite32(MY_DEVICE_DMA_START_BIT | MY_DEVICE_DMA_DIR_BIT | MY_DEVICE_IRQ_ENABLE_BIT, &priv->regs->control);
    wmb(); // Write memory barrier

    pr_info("my_device: Simulated device registers programmed. Triggering DMA...\n");

    // 7. Simulate DMA Transfer and Interrupt Generation
    // In a real driver, you would typically return here and wait for the interrupt.
    // For this simple example, we'll call our simulation function directly
    // after a short delay to trigger the interrupt path.
    mdelay(5); // Small delay before simulating completion
    my_device_simulate_dma(priv); // This function will now trigger the ISR

    pr_info("my_device: Probe finished successfully.\n");
    return 0; // Success

free_irq:
    free_irq(priv->irq, priv);
    // tasklet_kill is needed if request_irq succeeded but we are rolling back

free_dma_buffer:
    if (priv->dma_buffer_virt) {
        dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE,
                          priv->dma_buffer_virt, priv->dma_buffer_phys);
        pr_info("my_device: Freed DMA buffer during error rollback\n");
    }
iounmap_regs:
    if (priv->regs_base) {
        iounmap(priv->regs_base);
        pr_info("my_device: Unmapped MMIO region during error rollback\n");
    }
release_region:
    pci_release_region(pdev, 0);
free_priv:
    // devm_kzalloc is used, so no need to kfree here
disable_pci:
    pci_disable_device(pdev);

    return ret;
}

// PCI device remove function
static void my_device_remove(struct pci_dev *pdev)
{
    struct my_device_priv *priv = pci_get_drvdata(pdev);

    pr_info("my_device: Remove function called\n");

    // Disable interrupts on the device (simulated)
    if (priv->regs) {
        iowrite32(ioread32(&priv->regs->control) & ~MY_DEVICE_IRQ_ENABLE_BIT, &priv->regs->control);
        wmb(); // Ensure write is flushed
    }

    // 1. Free Interrupt
    if (priv->irq > 0) { // Check if IRQ was successfully requested
        free_irq(priv->irq, priv);
        pr_info("my_device: Freed IRQ %d\n", priv->irq);
    }

    // 2. Kill Tasklet
    // Ensure the tasklet is not scheduled or running
    tasklet_kill(&priv->tasklet);
    pr_info("my_device: Tasklet killed.\n");


    // 3. Free DMA coherent buffer
    if (priv->dma_buffer_virt) {
        dma_free_coherent(&pdev->dev, DMA_BUFFER_SIZE,
                          priv->dma_buffer_virt, priv->dma_buffer_phys);
        pr_info("my_device: Freed DMA buffer\n");
    }

    // 4. Unmap MMIO region
    if (priv->regs_base) {
        iounmap(priv->regs_base);
        pr_info("my_device: Unmapped MMIO region\n");
    }

    // 5. Release PCI region
    pci_release_region(pdev, 0);
    pr_info("my_device: Released PCI region\n");

    // 6. Disable the PCI device
    pci_disable_device(pdev);
    pr_info("my_device: PCI device disabled\n");

    // devm_kzalloc handles freeing the priv struct
    pr_info("my_device: Remove finished.\n");
}

// PCI device ID table
static const struct pci_device_id my_device_id_table[] = {
    { PCI_DEVICE(MY_DEVICE_VENDOR_ID, MY_DEVICE_DEVICE_ID) },
    { 0, } // Sentinel entry
};
MODULE_DEVICE_TABLE(pci, my_device_id_table);

// PCI driver structure
static struct pci_driver my_device_driver = {
    .name       = "my_device",
    .id_table   = my_device_id_table,
    .probe      = my_device_probe,
    .remove     = my_device_remove,
    // Add other callbacks like suspend/resume if needed
};

/************************************************************************
 * Module Entry and Exit
 ************************************************************************/

static int __init my_device_init(void)
{
    pr_info("my_device: Module initializing\n");
    // Register the PCI driver with the kernel
    return pci_register_driver(&my_device_driver);
}

static void __exit my_device_exit(void)
{
    pr_info("my_device: Module exiting\n");
    // Unregister the PCI driver
    pci_unregister_driver(&my_device_driver);
}

module_init(my_device_init);
module_exit(my_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple simulated PCI device driver with DMA and Interrupts");
MODULE_VERSION("0.2");

