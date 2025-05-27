// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/napi.h>
#include <linux/dma-mapping.h>

#define MYNIC_RX_RING_SIZE 128
#define MYNIC_TX_RING_SIZE 128

#define MYNIC_MMIO_BAR 0

// PCI Device Table
static const struct pci_device_id mynic_pci_tbl[] = {
    { PCI_DEVICE(0x1234, 0x5678) }, // Example vendor/device ID
    { 0, }
};
MODULE_DEVICE_TABLE(pci, mynic_pci_tbl);

// Per-NIC private data
struct mynic_priv {
    void __iomem *mmio;              // MMIO region base
    struct napi_struct napi;         // NAPI context
    struct net_device *netdev;       // Pointer back to net_device
    struct pci_dev *pdev;            // PCI device
    dma_addr_t rx_dma, tx_dma;       // DMA base addresses
    struct sk_buff *rx_skbuff[MYNIC_RX_RING_SIZE];
    struct sk_buff *tx_skbuff[MYNIC_TX_RING_SIZE];
    int rx_head, rx_tail, tx_head, tx_tail;
    spinlock_t lock;
    // ... hardware-specific descriptors, stats, etc.
};

// Forward declarations
static int mynic_open(struct net_device *dev);
static int mynic_stop(struct net_device *dev);
static netdev_tx_t mynic_start_xmit(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t mynic_interrupt(int irq, void *dev_id);
static int mynic_poll(struct napi_struct *napi, int budget);

// Netdevice operations
static const struct net_device_ops mynic_netdev_ops = {
    .ndo_open       = mynic_open,
    .ndo_stop       = mynic_stop,
    .ndo_start_xmit = mynic_start_xmit,
    // .ndo_set_rx_mode, .ndo_do_ioctl, etc. can be added here
    // first for multicast and second for custom commands and ifconfig
};

// PCI Probe Function
static int mynic_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct net_device *netdev;
    struct mynic_priv *priv;
    int err;
    u8 __iomem *mmio;

    // Enable PCI device
    err = pci_enable_device(pdev);
    if (err)
        return err;

    // Request MMIO region
    err = pci_request_region(pdev, MYNIC_MMIO_BAR, "mynic-mmio");
    if (err)
        goto err_disable_pci;

    // Map MMIO region
    mmio = pci_iomap(pdev, MYNIC_MMIO_BAR, 0);
    if (!mmio) {
        err = -ENOMEM;
        goto err_release_region;
    }

    // Allocate net_device with private data
    netdev = alloc_etherdev(sizeof(struct mynic_priv));
    if (!netdev) {
        err = -ENOMEM;
        goto err_iounmap;
    }

    // Setup private data
    priv = netdev_priv(netdev);
    priv->mmio = mmio;
    priv->netdev = netdev;
    priv->pdev = pdev;
    spin_lock_init(&priv->lock);

    pci_set_drvdata(pdev, netdev); // sets the netdev as private data in the PCI dev
    SET_NETDEV_DEV(netdev, &pdev->dev); //sets the netdev->dev.parent pointer to pdev->dev, 
    // so the network interface appears as a child of the PCI device in the kernel’s device model and sysfs.

    // Set netdevice ops
    netdev->netdev_ops = &mynic_netdev_ops;
    netdev->watchdog_timeo = msecs_to_jiffies(5000); // 5s TX timeout

    // Register net_device
    err = register_netdev(netdev);
    if (err)
        goto err_free_netdev;

    // Enable MSI/MSI-X interrupt (not shown: use pci_alloc_irq_vectors)
    err = request_irq(pdev->irq, mynic_interrupt, 0, "mynic", netdev);
    if (err)
        goto err_unregister_netdev;

    // A kernel structure representing the polling context/state for NAPI (new API),
    // used to manage interrupt mitigation and packet batching for efficient RX/TX processing in high-speed network drivers.
    // Registers a NAPI polling context (priv->napi) with the network device, associating it with the 
    // poll function (mynic_poll) and a poll budget of 64 packets per pass
    netif_napi_add(netdev, &priv->napi, mynic_poll, 64);

    dev_info(&pdev->dev, "mynic NIC registered\n");
    return 0;

err_unregister_netdev:
    unregister_netdev(netdev);
err_free_netdev:
    free_netdev(netdev);
err_iounmap:
    pci_iounmap(pdev, mmio);
err_release_region:
    pci_release_region(pdev, MYNIC_MMIO_BAR);
err_disable_pci:
    pci_disable_device(pdev);
    return err;
}

// PCI Remove Function
static void mynic_remove(struct pci_dev *pdev)
{
    struct net_device *netdev = pci_get_drvdata(pdev);
    struct mynic_priv *priv = netdev_priv(netdev);

    free_irq(pdev->irq, netdev);
    unregister_netdev(netdev);
    netif_napi_del(&priv->napi);

    pci_iounmap(pdev, priv->mmio);
    free_netdev(netdev);
    pci_release_region(pdev, MYNIC_MMIO_BAR);
    pci_disable_device(pdev);
    dev_info(&pdev->dev, "mynic NIC removed\n");
}

// PCI Driver Structure
static struct pci_driver mynic_driver = {
    .name       = "mynic",
    .id_table   = mynic_pci_tbl,
    .probe      = mynic_probe,
    .remove     = mynic_remove,
};

// Open (ifconfig up / ip link set up)
static int mynic_open(struct net_device *dev)
{
    struct mynic_priv *priv = netdev_priv(dev);

    napi_enable(&priv->napi);
    netif_start_queue(dev);

    // Enable interrupts and RX in hardware (MMIO writes)
    writel(ENABLE_IRQ | ENABLE_RX, priv->mmio + CONTROL_REG);

    return 0;
}

// Stop (ifconfig down / ip link set down)
static int mynic_stop(struct net_device *dev)
{
    struct mynic_priv *priv = netdev_priv(dev);

    // Disable RX/interrupts
    writel(0, priv->mmio + CONTROL_REG);

    netif_stop_queue(dev);
    napi_disable(&priv->napi);
    return 0;
}

// Transmit function
static netdev_tx_t mynic_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct mynic_priv *priv = netdev_priv(dev);
    dma_addr_t dma_addr;
    unsigned int len = skb_headlen(skb);

    // Map skb data for DMA
    dma_addr = dma_map_single(&priv->pdev->dev, skb->data, len, DMA_TO_DEVICE);

    spin_lock(&priv->lock);

    // Place DMA address and length into TX descriptor
    // Hardware-specific: mynic_tx_desc_write(priv, priv->tx_head, dma_addr, len);
    // now the dma engine in the nic is the master and asks for data
    // at dma_addr (the dma handle) for that amount of data

    priv->tx_skbuff[priv->tx_head] = skb;
    priv->tx_head = (priv->tx_head + 1) % MYNIC_TX_RING_SIZE;

    // Kick hardware to start transmit (MMIO)
    writel(KICK_TX, priv->mmio + TX_REG);

    spin_unlock(&priv->lock);

    // If TX ring is full, call netif_stop_queue(dev)

    return NETDEV_TX_OK;
}

// Interrupt handler (MSI-X/legacy)
static irqreturn_t mynic_interrupt(int irq, void *dev_id)
{
    struct net_device *dev = dev_id;
    struct mynic_priv *priv = netdev_priv(dev);
    u32 status;

    // Read and acknowledge interrupt status (MMIO)
    status = readl(priv->mmio + INTR_STATUS_REG);
    writel(status, priv->mmio + INTR_STATUS_REG);

    if (status & RX_INTR)
        napi_schedule(&priv->napi);

    if (status & TX_INTR) {
        // Complete transmit, free sent sk_buffs
        spin_lock(&priv->lock);
        while (/* hardware reports packets sent */) {
            int idx = priv->tx_tail;
            struct sk_buff *skb = priv->tx_skbuff[idx];
            if (skb) {
                dma_unmap_single(&priv->pdev->dev, /* get DMA addr */, skb->len, DMA_TO_DEVICE);
                dev_kfree_skb_irq(skb);
                priv->tx_skbuff[idx] = NULL;
            }
            priv->tx_tail = (priv->tx_tail + 1) % MYNIC_TX_RING_SIZE;
        }
        spin_unlock(&priv->lock);
        netif_wake_queue(dev);
    }

    return IRQ_HANDLED;
}

// NAPI Poll (RX)
static int mynic_poll(struct napi_struct *napi, int budget)
{
    struct mynic_priv *priv = container_of(napi, struct mynic_priv, napi);
    int rx_done = 0;

    while (rx_done < budget && /* packets available */) {
        struct sk_buff *skb;
        unsigned int pkt_len;
        dma_addr_t dma_addr;
        void *buf;

        // Get pointer to received data, DMA address, and packet length from RX descriptor
        // Hardware-specific...

        skb = netdev_alloc_skb_ip_align(priv->netdev, pkt_len);
        if (!skb)
            break;

        // Copy data from device buffer to skb (or map for zero-copy if possible)
        memcpy(skb_put(skb, pkt_len), buf, pkt_len);

        skb->protocol = eth_type_trans(skb, priv->netdev);
        skb->ip_summed = CHECKSUM_NONE; // or CHECKSUM_UNNECESSARY if HW did it

        netif_receive_skb(skb);
        rx_done++;
    }

    if (rx_done < budget) {
        napi_complete_done(napi, rx_done);
        // Re-enable RX interrupts
        writel(ENABLE_RX_IRQ, priv->mmio + CONTROL_REG);
    }

    return rx_done;
}

// Module init/exit
static int __init mynic_init(void)
{
    return pci_register_driver(&mynic_driver);
}
static void __exit mynic_exit(void)
{
    pci_unregister_driver(&mynic_driver);
}

module_init(mynic_init);
module_exit(mynic_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("Example Modern PCIe NIC Driver Skeleton");
