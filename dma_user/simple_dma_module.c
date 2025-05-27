#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>    // For kmalloc/kfree or dma_alloc_coherent
#include <linux/uaccess.h> // For copy_to_user/copy_from_user
#include <linux/fs.h>      // For file_operations
#include <linux/err.h>
#include <linux/mm.h>      // For mmap
#include <linux/version.h> // For kernel version checks

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/uapi/asm-generic/ioctl.h> // For _IO, _IOW, _IOR
#else
#include <asm/ioctl.h>
#endif

#include <linux/dma-mapping.h> // For DMA API

#define DEVICE_NAME "simple_dma"
#define DMA_BUFFER_SIZE (4 * PAGE_SIZE) // Allocate 4 pages for DMA buffer
#define SIMPLE_DMA_MAGIC 's'
#define SIMPLE_DMA_START_TRANSFER _IO(SIMPLE_DMA_MAGIC, 1) // ioctl command

static dev_t simple_dma_dev_t;
static struct cdev simple_dma_cdev;
static struct class *simple_dma_class;

// DMA buffer related variables
static void *dma_buffer_virt = NULL;
static dma_addr_t dma_buffer_phys;       // This is the bus address for the device
static struct device *dma_device = NULL; // Placeholder for device struct

/************************************************************************
 * File Operations
 ************************************************************************/

// Open operation
static int simple_dma_open(struct inode *inode, struct file *file)
{
    pr_info("simple_dma: Device opened\n");
    return 0;
}

// Release operation
static int simple_dma_release(struct inode *inode, struct file *file)
{
    pr_info("simple_dma: Device closed\n");
    return 0;
}

// mmap operation to map the DMA buffer to user space
static int simple_dma_mmap(struct file *file, struct vm_area_struct *vma)
{
    int ret;
    unsigned long size = vma->vm_end - vma->vm_start;

    // Ensure the requested size does not exceed the allocated buffer size
    if (size > DMA_BUFFER_SIZE)
    {
        pr_err("simple_dma: mmap size exceeds buffer size\n");
        return -EINVAL;
    }

    // Ensure the offset is 0 for simplicity in this example although this is easy to do
    if (vma->vm_pgoff != 0)
    {
        pr_err("simple_dma: mmap offset must be 0\n");
        return -EINVAL;
    }

    // Use dma_mmap_coherent to map the DMA buffer to user space
    // This handles cache synchronization and IOMMU translation if needed.
    // The dma_buffer_phys is the bus address that the device sees.
    ret = dma_mmap_coherent(dma_device, vma, dma_buffer_virt, dma_buffer_phys, size); // this is also pinned
    if (ret < 0)
    {
        pr_err("simple_dma: dma_mmap_coherent failed: %d\n", ret);
        return ret;
    }

    pr_info("simple_dma: DMA buffer mapped to user space\n");
    return 0;
}

// ioctl operation to trigger a simulated DMA transfer
static long simple_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
    case SIMPLE_DMA_START_TRANSFER:
        pr_info("simple_dma: Received START_TRANSFER ioctl from user space\n");
        // In a real driver, you would program your hardware here
        // to start a DMA transfer using dma_buffer_phys as the
        // source or destination address.

        // Simulate a DMA transfer: copy data within the kernel buffer
        // This is just to show the kernel can work with the buffer.
        // A real DMA would move data to/from hardware.
        if (dma_buffer_virt)
        {
            pr_info("simple_dma: Simulating DMA transfer (memcpy within kernel)\n");
            // Example: Reverse the data in the buffer
            char *buf = (char *)dma_buffer_virt;
            int i, j;
            char temp;
            for (i = 0, j = DMA_BUFFER_SIZE - 1; i < j; ++i, --j)
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
            pr_info("simple_dma: Simulated DMA (reverse) complete\n");

            // In a real scenario, you might need to use dma_sync_single_for_cpu
            // or dma_sync_single_for_device here depending on the direction
            // and cache coherence requirements if not using dma_alloc_coherent
            // for cache-incoherent memory. With dma_alloc_coherent, the memory
            // is typically kept coherent by the hardware or the DMA API.
        }
        else
        {
            pr_err("simple_dma: DMA buffer not allocated!\n");
            return -EFAULT;
        }

        // In a real driver, you would likely wait for a DMA completion interrupt
        // or poll for completion before returning from the ioctl if it's meant
        // to be a blocking transfer.

        return 0; // Success

    default:
        pr_info("simple_dma: Unknown ioctl command: 0x%x\n", cmd);
        return -ENOTTY; // Inappropriate ioctl for device
    }
}

static const struct file_operations simple_dma_fops = {
    .owner = THIS_MODULE,
    .open = simple_dma_open,
    .release = simple_dma_release,
    .mmap = simple_dma_mmap,
    .unlocked_ioctl = simple_dma_ioctl,
};

/************************************************************************
 * Module Initialization and Exit
 ************************************************************************/

static int __init simple_dma_init(void)
{
    int ret;

    pr_info("simple_dma: Initializing module\n");

    // 1. Allocate a character device region
    ret = alloc_chrdev_region(&simple_dma_dev_t, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("simple_dma: Failed to allocate character device region: %d\n", ret);
        return ret;
    }
    pr_info("simple_dma: Allocated device with major %d, minor %d\n", MAJOR(simple_dma_dev_t), MINOR(simple_dma_dev_t));

    // 2. Create a device class (for automatic device node creation)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    simple_dma_class = class_create(DEVICE_NAME);
#else
    simple_dma_class = class_create(THIS_MODULE, DEVICE_NAME);
#endif
    if (IS_ERR(simple_dma_class))
    {
        pr_err("simple_dma: Failed to create device class\n");
        ret = PTR_ERR(simple_dma_class);
        goto unregister_chrdev;
    }

    // 3. Create a device
    // In a real driver, you would typically get the struct device*
    // from the platform bus, PCI subsystem, etc. For this example,
    // we'll use a simple approach to get a device pointer for dma_alloc_coherent.
    // A more robust approach might involve using a dummy device or
    // associating with a platform device.
    // WARNING: Using a generic device pointer like this might not work
    // correctly on all architectures or with all DMA controllers.
    // A proper driver MUST get the device pointer from the bus subsystem.
    dma_device = class_find_device(simple_dma_class, NULL, NULL, NULL);
    if (!dma_device)
    {
        // If class_find_device doesn't work immediately (e.g., no devices yet),
        // try a simpler approach for this example.
        // THIS IS HIGHLY SIMPLIFIED AND MAY NOT BE SUITABLE FOR REAL DRIVERS.
        // Find a suitable device for DMA mapping. This is a tricky part
        // for a generic example without a specific bus/device.
        // On some systems, a platform device or the root device might work.
        // For a real driver, use the `struct device` provided by the bus.
#if defined(CONFIG_ARCH_HAS_DMA_MAP_COHERENT) && defined(CONFIG_GENERIC_ALLOCATOR)
        pr_warn("simple_dma: Could not find a device via class_find_device. Attempting alternative.\n");
        // This part is architecture/configuration dependent.
        // A reliable way needs a proper device structure.
        // We'll skip dma_alloc_coherent without a valid device pointer for safety.
        dma_device = NULL; // Explicitly set to NULL if we can't find one
#else
        pr_err("simple_dma: No suitable device found for DMA allocation. DMA features disabled.\n");
        dma_device = NULL;
#endif
    }

    if (dma_device)
    {
        // 4. Allocate DMA-coherent buffer
        // The dma_handle (dma_buffer_phys) is the address the device uses.
        // The virtual address (dma_buffer_virt) is what the CPU uses.
        dma_buffer_virt = dma_alloc_coherent(dma_device, DMA_BUFFER_SIZE, &dma_buffer_phys, GFP_KERNEL);
        if (!dma_buffer_virt)
        {
            pr_err("simple_dma: Failed to allocate DMA coherent buffer\n");
            // Continue without DMA buffer, mmap will fail later.
        }
        else
        {
            pr_info("simple_dma: Allocated DMA buffer: virt=0x%px, phys=0x%llx\n",
                    dma_buffer_virt, (unsigned long long)dma_buffer_phys);
            // Initialize the buffer
            memset(dma_buffer_virt, 0, DMA_BUFFER_SIZE);
        }
    }
    else
    {
        pr_err("simple_dma: Could not obtain a valid device pointer for DMA allocation.\n");
        pr_err("simple_dma: DMA buffer allocation and mmap will not be available.\n");
    }

    // 5. Initialize and add the character device
    cdev_init(&simple_dma_cdev, &simple_dma_fops);
    simple_dma_cdev.owner = THIS_MODULE;
    ret = cdev_add(&simple_dma_cdev, simple_dma_dev_t, 1);
    if (ret < 0)
    {
        pr_err("simple_dma: Failed to add character device: %d\n", ret);
        goto destroy_class;
    }

    // 6. Create the device node in /dev
    // This makes the device accessible from user space.
    if (IS_ERR(device_create(simple_dma_class, NULL, simple_dma_dev_t, NULL, DEVICE_NAME)))
    {
        pr_err("simple_dma: Failed to create device node\n");
        ret = PTR_ERR(simple_dma_class);
        goto del_cdev;
    }

    pr_info("simple_dma: Module loaded and device /dev/%s created\n", DEVICE_NAME);
    return 0;

del_cdev:
    cdev_del(&simple_dma_cdev);
destroy_class:
    class_destroy(simple_dma_class);
unregister_chrdev:
    unregister_chrdev_region(simple_dma_dev_t, 1);
    // dma_free_coherent will be handled in exit if allocation succeeded
    return ret;
}

static void __exit simple_dma_exit(void)
{
    pr_info("simple_dma: Exiting module\n");

    // Destroy the device node
    device_destroy(simple_dma_class, simple_dma_dev_t);

    // Delete the character device
    cdev_del(&simple_dma_cdev);

    // Destroy the device class
    class_destroy(simple_dma_class);

    // Unregister the character device region
    unregister_chrdev_region(simple_dma_dev_t, 1);

    // Free the DMA coherent buffer if it was allocated
    if (dma_buffer_virt && dma_device)
    {
        dma_free_coherent(dma_device, DMA_BUFFER_SIZE, dma_buffer_virt, dma_buffer_phys);
        pr_info("simple_dma: Freed DMA buffer\n");
    }
    else if (dma_buffer_virt)
    {
        // This case indicates dma_alloc_coherent succeeded without a proper device pointer,
        // which is unexpected but we should still try to free.
        pr_warn("simple_dma: Freeing DMA buffer without a valid device pointer. Potential issue.\n");
        // Depending on the kernel version and architecture, freeing might still work
        // with a NULL device pointer if the DMA mapping is simple.
        // In a real driver, this should not happen as dma_alloc_coherent requires a device.
        dma_free_coherent(NULL, DMA_BUFFER_SIZE, dma_buffer_virt, dma_buffer_phys);
    }

    pr_info("simple_dma: Module exited\n");
}

module_init(simple_dma_init);
module_exit(simple_dma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple example module for user-space DMA memory access");
MODULE_VERSION("0.1");

/*
The fd (file descriptor) passed by user space is used by the kernel to find the corresponding struct file object.
The kernel looks up the mmap function pointer in the struct file's f_op (file operations) field. This is why we set .mmap = simple_dma_mmap in the kernel module's file_operations.
The kernel's memory management subsystem then prepares a struct vm_area_struct (vma) structure to represent the region of virtual memory being requested by the user process.
This vma structure is populated with information derived from the user-space mmap arguments:
vma->vm_start and vma->vm_end: The virtual address range in the user process's address space where the mapping is requested (based on the addr and length arguments, adjusted by the kernel if addr is NULL). The size of the region is vma->vm_end - vma->vm_start, corresponding to DMA_BUFFER_SIZE in your user code.
vma->vm_flags: Contains the protection and sharing flags (like VM_READ, VM_WRITE derived from PROT_READ | PROT_WRITE, and VM_SHARED from MAP_SHARED).
vma->vm_pgoff: The offset into the file or device being mapped, in units of pages (4096 bytes on most systems). This corresponds to the offset argument (which is 0 in your user-space mmap call).
The kernel then calls the device driver's registered mmap handler (simple_dma_mmap), passing the struct file and the prepared struct vm_area_struct (vma) as arguments. Your driver code then uses the information in vma (like size and offset) to perform the actual mapping using functions like dma_mmap_coherent, setting up the user process's page tables to point to the correct physical memory.


What coherent really means for DMA: In the context of DMA, "coherent" means the memory region can be accessed concurrently by both the CPU and the DMA device, and the system (hardware or a combination of hardware and low-level kernel code) automatically ensures that both the CPU caches and the device see the same, up-to-date data without requiring explicit cache flush or invalidate operations from the driver for this specific memory. It ensures data consistency between the CPU and the device.

Are these pages also pinned? Yes, memory allocated with dma_alloc_coherent is pinned in physical RAM for the lifetime of the allocation. This is necessary because the DMA device operates on physical or bus addresses, which are stable, unlike user-space virtual addresses which can be paged out to swap.

Kernel virtual address (vaddr): This is the standard memory address that the Linux kernel uses to access the allocated buffer directly using the CPU's load/store instructions. It's within the kernel's virtual address space.

Device-accessible bus address (dma_handle): This is the address that the DMA device uses to access the allocated buffer over the system bus. This address might be the physical address or an address translated by an IOMMU. The driver provides this address to the device's DMA controller to initiate data transfers.


When are the IOMMU registers set and where are they?
IOMMU registers are typically set up by the kernel's IOMMU driver when a driver requests to map memory for DMA using the DMA API functions (like dma_alloc_coherent). The IOMMU driver programs the translation tables within the IOMMU hardware. The IOMMU hardware itself is a physical component, often part of the system's chipset or CPU, and its control registers are hardware registers accessible by the kernel, usually via memory-mapped I/O (MMIO).




dma_alloc_coherent:

This function works by first finding and allocating a block of physical memory that is suitable for DMA. "Suitable" often means physically contiguous (depending on the architecture and device needs) and aligned correctly.
It then establishes a kernel virtual address (dma_buffer_virt) that maps to this allocated physical memory. This mapping is part of the kernel's own page tables, allowing the CPU to access the memory directly using this pointer.
Simultaneously, it determines the device-accessible bus address (dma_buffer_phys). If there's no IOMMU, this is usually the physical address of the allocated buffer. If an IOMMU is present, the DMA API interacts with the IOMMU driver to create a translation entry, and dma_buffer_phys becomes the address within the IOMMU's address space that maps to the physical buffer. Neither address is assigned arbitrarily; they are determined by the physical location of the allocated memory and the system's address mapping hardware (MMU for CPU, IOMMU for device).
dma_mmap_coherent:

This function is called by your kernel driver's mmap handler (simple_dma_mmap) when a user-space process requests to map the device memory. It receives the vma, which describes the user process's requested virtual address range and memory access permissions (PROT_READ | PROT_WRITE).
Its core job is to set up the user process's page tables. It creates entries in the user process's page tables so that the virtual addresses within the vma range now point to the same physical pages that were allocated by dma_alloc_coherent.
Crucially, dma_mmap_coherent also ensures that the memory attributes for this user-space mapping are set correctly to maintain cache coherence when the CPU accesses this memory from user space. It essentially makes the DMA buffer accessible from both the kernel's virtual address space (dma_buffer_virt) and the user process's virtual address space (via the mapped vma), with appropriate handling for concurrent CPU and device access.




They are the standard way to handle DMA for buffers that are used for single, short-term data transfers, such as network packets or disk blocks. The Linux kernel's DMA API provides functions specifically for streaming mappings like dma_map_single, dma_unmap_single, dma_map_sg, and dma_unmap_sg (for scatter/gather lists).

Drivers use streaming mappings because they allow the system to perform performance optimizations and manage limited IOMMU resources more efficiently compared to keeping long-lived coherent mappings open when not actively in use. 1  They require the driver to explicitly handle cache synchronization using functions like dma_sync_single_for_cpu and dma_sync_single_for_device before and after the DMA transfer.













Okay, let's explain the modern flow of interacting with DMA controllers in Linux after you've prepared the memory.

The key shift from the old ISA model is that there isn't typically a single, central, programmable "DMA Controller" chip that all devices share and you program directly using generic `set_dma_*` functions. Instead, in modern systems:

  * **Devices often have their own integrated DMA engines.** This is standard for many modern peripherals, especially on buses like PCIe. The peripheral itself is a "bus master" and can initiate memory transfers.
  * **There can be system-level "DMA Engines" or "Copy Engines".** These might be part of the chipset and provide generic DMA capabilities that drivers can utilize if their hardware doesn't have a complex engine, or for memory-to-memory transfers. The Linux kernel has a "DMA Engine" subsystem to provide a standardized API for drivers to request DMA operations from these engines.

Here's a simplified flow and explanation:

**Modern DMA Flow (Conceptual)**

```
+-----------------+      +-------------------+      +---------------------+
| User Space App  |      |  Kernel Driver    |      |  Generic DMA API    |
| (Optional)      |      | (Device Specific) |      |  (Kernel Subsystem) |
+-----------------+      +-------------------+      +---------------------+
        |                          |                          |
        |  (e.g., mmap or ioctl)   |                          |
        +------------------------->|                          |
        |                          | 1. Allocate/Map Memory   |
        |                          |    (dma_alloc_coherent   |
        |                          |     or dma_map_single/sg)|<-----------------+
        |                          |                          |                  |
        |                          | 2. Get Bus Handle        |                  |
        |                          |    (dma_addr_t)          |                  |
        |                          |                          |                  |
        |                          +------------------------->|                  |
        |                                                     | 3. Perform Sync  |
        |                                                     |    (if streaming)|
        |                                                     |    (dma_sync_*)  |
        |                                                     |                  |
+-----------------+      +-------------------+      +---------------------+
|   System RAM    |      | Device's Hardware |      |   System Bus        |
| (Physical Memory)|      |  (PCIe Peripheral)|      | (e.g., PCIe)        |
| (Mapped/Pinned) |      | (with DMA Engine) |      |                     |
+-----------------+      +-------------------+      +---------------------+
        ^                          |                          |
        |                          | 4. Driver Programs       |
        |                          |    Device's Registers    |
        |                          |    - Pass dma_addr_t     |
        |                          |    - Set size, direction |
        |                          |    - Trigger DMA         |
        |                          +------------------------->|
        |                                                     | 5. DMA Transfer  |
        +-----------------------------------------------------|  (Device acts as|
        |(Device reads/writes to physically mapped memory)    |  Bus Master)    |
        |                                                     |                  |
        |                          |                          |                  |
        |                          | 6. DMA Completion        |                  |
        |                          |    (Interrupt/Polling)   |                  |
        +--------------------------<|                          |
                                   | 7. Handle Completion     |
                                   |    - Unmap/Free (stream)|
                                   |    - Perform Sync (CPU) |
                                   +------------------------->|
                                                              |

**Explanation of the Flow and Modern API:**

1.  **Allocate/Map Memory (`dma_alloc_coherent` or `dma_map_single`/`_sg`):**
    * The kernel driver first allocates or maps memory using the **generic DMA API**. This is where you use functions like `dma_alloc_coherent` (for coherent, long-lived buffers) or `dma_map_single`/`dma_map_sg` (for streaming, single-transfer buffers).
    * These functions handle the complexities of finding suitable physical memory, pinning it, and setting up necessary IOMMU translations if an IOMMU is present.

2.  **Get Bus Handle (`dma_addr_t`):**
    * The key output of the DMA allocation/mapping functions is the **`dma_addr_t`**. This is the *bus address* that the device understands. It's the address the device's DMA engine will use to access the memory over the bus. The kernel virtual address is for the CPU, the `dma_addr_t` is for the device.

3.  **Perform Synchronization (if streaming):**
    * If you are using **streaming mappings** (`dma_map_single`/`_sg`), the memory might be cached by the CPU. Before the device reads the data, the driver must call `dma_sync_single_for_device` (or `_sg`). After the device writes data and before the CPU reads it, the driver must call `dma_sync_single_for_cpu` (or `_sg`). These functions ensure cache coherence by flushing CPU caches (writing dirty data to RAM) or invalidating CPU caches (discarding stale data and forcing reads from RAM) as needed. Coherent mappings (`dma_alloc_coherent`) typically handle this automatically.

4.  **Driver Programs Device's Registers:**
    * This is where the **device-specific** part comes in. The driver knows the hardware details of the peripheral. It writes to the device's memory-mapped I/O (MMIO) registers to configure the device's integrated DMA engine.
    * The driver passes the **`dma_addr_t`** (the bus handle) to the device's DMA engine registers. It also tells the device:
        * The size of the transfer.
        * The direction of the transfer (to device or from device).
        * Potentially other parameters like scatter/gather list pointers if using `dma_map_sg`.
    * Finally, the driver writes to a specific control register to **trigger the DMA transfer** on the device's engine.

5.  **DMA Transfer (Device as Bus Master):**
    * Once triggered, the device's DMA engine becomes a **bus master**. It uses the `dma_addr_t` provided by the driver to directly read data from or write data to the physical memory (System RAM) over the system bus (e.g., PCIe), without significant CPU intervention in the data path. The IOMMU, if present, translates the bus addresses used by the device to physical RAM addresses.

6.  **DMA Completion (Interrupt/Polling):**
    * When the DMA transfer is finished, the device typically signals completion. This is usually done via an **interrupt** to the CPU, which the driver's interrupt handler processes. Some drivers might also **poll** a status register on the device to check for completion.

7.  **Handle Completion:**
    * The driver's completion handler performs necessary post-transfer steps:
        * If using **streaming mappings**, it calls `dma_unmap_single` or `dma_unmap_sg` to release the DMA mapping resources.
        * If data was transferred *to* the CPU for a streaming mapping, it calls `dma_sync_single_for_cpu` (if not already done) to ensure the CPU sees the latest data from RAM.
        * The driver then processes the received data or signals completion to the rest of the kernel or user space.

**Relevant Hardware Design (PCIe Peripherals):**

Yes, a crucial aspect of modern bus architectures like **PCIe is that peripherals are typically designed as "bus masters" with their own DMA engines**. This allows them to perform memory transfers independently of the CPU. This is fundamentally different from older architectures like ISA, where devices often relied on a central DMA controller.

So, when a kernel driver is written for a PCIe device, it's primarily interacting with the DMA engine *on that specific PCIe card* by programming its registers via MMIO. The generic DMA API provides the necessary address translations and coherence management, but the initiation and control of the transfer are often handled by the peripheral's own hardware DMA logic. Some systems may still have central DMA engines, but the per-peripheral bus mastering DMA is a defining characteristic of high-performance modern I/O.
A driver is shared for both the peripheral and the DMA engine.
```







The Linux kernel's IOMMU subsystem (drivers/iommu/) manages the IOMMU hardware. When a driver calls a generic DMA API function like dma_alloc_coherent or dma_map_single, the DMA API layer checks if an IOMMU is present and enabled for that device. If so, it calls into the IOMMU subsystem, which then dispatches the request to the specific hardware-dependent IOMMU driver (e.g., intel-iommu.c for Intel VT-d, amd_iommu.c for AMD-Vi, arm-smmu.c for ARM SMMU).

So, it's not one single driver name, but rather a framework that uses the appropriate driver for your system's IOMMU hardware under the hood.








Where is the table that monitors interrupts in hardware?
The hardware component responsible for receiving and dispatching interrupts is the Interrupt Controller. On modern systems, this is often integrated into the CPU or the chipset (like the Platform Controller Hub or PCH). This controller contains the internal logic and tables (like the Interrupt Descriptor Table or similar structures) that manage interrupt sources and map them to the appropriate handlers.

Shouldn't the table take in an interrupt number rather than a vector?
This is where terminology can be a bit confusing! In the context of MSI/MSI-X, the device doesn't assert a physical "interrupt line" with a fixed "interrupt number" in the old sense. Instead, the device performs a memory write operation to a specific address. The data written in this message contains the "vector" number. The interrupt controller is designed to detect these specific memory writes and use the vector value from the message to figure out which interrupt source it is. So, the hardware table does use the vector provided in the message as its key to look up what to do next.

Also is this irq number in the config space of the device at boot time? Does the kernel load this at boot time?
The device's PCI configuration space contains information about its capabilities, including whether it supports MSI or MSI-X and how many vectors it can support. However, the specific interrupt vector number that the device should use is not hardcoded in the config space by the vendor. Instead, the operating system kernel (during the driver's probe phase) reads the device's MSI/MSI-X capabilities and assigns one or more available interrupt vectors to that device. The kernel then programs the device's MSI/MSI-X capability registers to tell the device which vector(s) to use when generating interrupts. So, the kernel assigns and loads the vector information into the device at boot time (or when the device is hotplugged).

What if two devices decide to have the same irq number?
With MSI/MSI-X, the interrupt vectors are not shared between different devices. Each MSI/MSI-X vector is typically dedicated to a single interrupt source (which might be an entire device, or a specific function or queue within a complex device). The kernel ensures that when it assigns vectors, each device gets unique ones. This is a major advantage over older legacy interrupts where multiple devices might share a single physical IRQ line, requiring the kernel's interrupt handler to query each device on that line to see who interrupted.

Is this irq number determined by the firmware team at the vendor?
No, the firmware (BIOS/UEFI) enumerates the bus and identifies the devices and their capabilities (including MSI/MSI-X support). However, the assignment of the specific interrupt vectors is the responsibility of the operating system kernel. The vendor's firmware team defines the device's capabilities and how it can be programmed, but the OS dynamically allocates and assigns the actual interrupt resources (the vectors) to the device.






 Where is the config space located? The PCI/PCIe configuration space is a dedicated, standardized set of registers located within the actual peripheral device hardware itself. It's accessed by the CPU or firmware over the PCI/PCIe bus using special configuration cycles or memory-mapped I/O (MMIO).  

When the kernel reads it in during boot and assigns these things, is that the bootloader doing? No, it's primarily the operating system kernel's bus driver (like the Linux PCI subsystem) that reads the full configuration space and assigns resources (like interrupt vectors and memory regions) to devices. The bootloader (BIOS/UEFI) does an initial, basic enumeration and resource assignment to get the system running, but the OS kernel takes over for the detailed configuration required for device drivers.

And all this is stored in the kernel low memory (or pages pinned in ram)? The configuration information the kernel reads and the data structures it uses to manage devices (like struct pci_dev) are stored in kernel memory. This memory is part of the kernel's dynamic allocations and is managed by the kernel's memory management subsystem. It's not necessarily restricted to "low memory" and isn't typically "pages pinned in RAM" unless those specific kernel data structures themselves need to be pinned for some reason (which is uncommon for the device configuration data itself). The kernel ensures this memory is accessible to the kernel code.
Your understanding is close. The firmware (BIOS/UEFI) performs the initial PCI bus scan and assigns physical memory ranges (BARs) to each device's registers. It's the Linux kernel's PCI subsystem that then reads these physical addresses from the device's configuration space during boot. The device driver later uses the kernel's ioremap function to map that assigned physical address range into the kernel's virtual address space, allowing the driver to access the device's control registers using pointers.



Good wiki article: https://en.wikipedia.org/wiki/PCI_configuration_space
And in depth article: https://wiki.osdev.org/PCI#Configuration_Space_Access_Mechanism_.231
*/