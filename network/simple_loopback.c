#include <linux/netdevice.h>
#include <linux/etherdevice.h>

static int myloop_open(struct net_device *dev) { netif_start_queue(dev); return 0; }
static int myloop_stop(struct net_device *dev) { netif_stop_queue(dev); return 0; }
static netdev_tx_t myloop_xmit(struct sk_buff *skb, struct net_device *dev)
{
    skb_orphan(skb); // remove owner
    skb->protocol = eth_type_trans(skb, dev);
    netif_rx(skb); // deliver back to the stack
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;
    dev->stats.rx_packets++;
    dev->stats.rx_bytes += skb->len;
    return NETDEV_TX_OK;
}

static const struct net_device_ops myloop_ops = {
    .ndo_open = myloop_open,
    .ndo_stop = myloop_stop,
    .ndo_start_xmit = myloop_xmit,
};

static void myloop_setup(struct net_device *dev)
{
    ether_setup(dev);
    dev->netdev_ops = &myloop_ops;
    dev->flags |= IFF_LOOPBACK;
}

static struct net_device *myloop_dev;

static int __init myloop_init(void)
{
    myloop_dev = alloc_netdev(0, "myloop%d", NET_NAME_UNKNOWN, myloop_setup);
    return register_netdev(myloop_dev);
}

static void __exit myloop_exit(void)
{
    unregister_netdev(myloop_dev);
    free_netdev(myloop_dev);
}

module_init(myloop_init);
module_exit(myloop_exit);
MODULE_LICENSE("GPL");
