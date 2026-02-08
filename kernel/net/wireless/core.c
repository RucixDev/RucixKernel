#include "net/wireless.h"
#include "heap.h"
#include "string.h"
#include "console.h"

struct wireless_dev *alloc_wireless_dev(int sizeof_priv, const char *name) {
    struct net_device *netdev = alloc_netdev(sizeof_priv, name, NULL);
    if (!netdev) return NULL;
    
    struct wireless_dev *wdev = (struct wireless_dev*)kmalloc(sizeof(struct wireless_dev));
    if (!wdev) {
         
        return NULL;
    }
    
    memset(wdev, 0, sizeof(struct wireless_dev));
    wdev->netdev = netdev;
    netdev->priv = (void*)((uint64_t)netdev + sizeof(struct net_device));  

    return wdev;
}

int register_wireless_dev(struct wireless_dev *wdev) {
    if (!wdev || !wdev->netdev) return -1;
    
    kprint_str("Wireless: Registering device ");
    kprint_str(wdev->netdev->name);
    kprint_newline();
    
    register_netdev(wdev->netdev);
    return 0;
}

int unregister_wireless_dev(struct wireless_dev *wdev) {
     
    return 0;
}

void wifi_scan(struct wireless_dev *wdev) {
    if (wdev && wdev->scan) {
        wdev->scan(wdev);
    }
}
