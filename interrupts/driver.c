// In your driver's .c file

// ... (other includes and definitions) ...

// Define a device with multiple interrupt vectors (simulated)
#define MY_MULTI_VECTOR_DEVICE_ID 0x9999

// Structure to hold private data for this device
struct multi_vector_priv {
    struct pci_dev *pdev;
    int irq_vecs[4]; // Array to hold up to 4 IRQ vectors
    // ... other fields ...
};

// In the probe function for this device type:
static int multi_vector_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct multi_vector_priv *priv;
    int i, num_vectors;
    int ret;

    // ... (enable device, allocate priv struct, map MMIO, etc.) ...

    priv->pdev = pdev; // Store pdev

    // Request multiple IRQ vectors (e.g., for MSI-X)
    // The device's config space indicates how many vectors it supports.
    // pci_enable_msix() or pci_enable_msi() would typically be called first.
    // For this example, we'll assume MSI-X is enabled and the device
    // supports at least 4 vectors.

    num_vectors = pci_msix_vec_count(pdev); // Get actual supported count
    if (num_vectors < 4) {
        pr_err("Device does not support enough MSI-X vectors (%d < 4)\n", num_vectors);
        // Handle error, maybe fallback to MSI or legacy interrupt
        ret = -ENODEV;
        goto cleanup_previous_steps;
    }

    // Get the IRQ numbers (vectors)
    for (i = 0; i < 4; i++) {
        priv->irq_vecs[i] = pci_irq_vector(pdev, i); // Get the i-th vector
        if (priv->irq_vecs[i] < 0) {
            pr_err("Failed to get IRQ vector %d: %d\n", i, priv->irq_vecs[i]);
            // Handle error, free previously requested vectors
            ret = priv->irq_vecs[i];
            goto free_vectors;
        }
        pr_info("Assigned IRQ vector %d: %d\n", i, priv->irq_vecs[i]);

        // In a real driver, you would then call request_irq() for each vector
        // with different handler functions or a single handler that checks the vector.
        // request_irq(priv->irq_vecs[i], my_vector_handler, 0, "my_device_vec", priv);
    }

    // ... (rest of probe function) ...

    return 0; // Success

free_vectors:
    // Free any vectors that were successfully requested before failure
    for (int j = 0; j < i; j++) {
        // free_irq(priv->irq_vecs[j], priv); // Need to free if request_irq was called
    }
cleanup_previous_steps:
    // ... (cleanup MMIO, disable device, etc.) ...
    return ret;
}

// In the remove function:
static void multi_vector_remove(struct pci_dev *pdev)
{
    struct multi_vector_priv *priv = pci_get_drvdata(pdev);
    int i;

    // ... (disable interrupts on device, etc.) ...

    // Free the IRQ vectors
    for (i = 0; i < 4; i++) {
         if (priv->irq_vecs[i] > 0) { // Check if vector was valid
             // free_irq(priv->irq_vecs[i], priv); // Need to free if request_irq was called
         }
    }

    // ... (free DMA buffer, unmap MMIO, etc.) ...
}


// In the PCI device ID table:
static const struct pci_device_id my_device_id_table[] = {
    { PCI_DEVICE(MY_DEVICE_VENDOR_ID, MY_DEVICE_DEVICE_ID) },
    { PCI_DEVICE(MY_DEVICE_VENDOR_ID, MY_MULTI_VECTOR_DEVICE_ID) }, // Add entry for multi-vector device
    { 0, } // Sentinel entry
};
MODULE_DEVICE_TABLE(pci, my_device_id_table);

// ... (rest of module init/exit) ...