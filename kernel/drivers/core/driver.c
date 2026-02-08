#include "driver.h"
#include "string.h"
#include "console.h"

static LIST_HEAD(bus_list_head);
static LIST_HEAD(global_device_list);
static LIST_HEAD(global_driver_list);

void driver_core_init(void) {
    kprint_str("[Driver Core] Initialized\n");
}

int bus_register(struct bus_type *bus) {
    if (!bus) return -1;
    INIT_LIST_HEAD(&bus->dev_list);
    INIT_LIST_HEAD(&bus->drv_list);
    list_add_tail(&bus->bus_list, &bus_list_head);
    kprint_str("[Bus] Registered: ");
    kprint_str(bus->name);
    kprint_newline();
    return 0;
}

void bus_unregister(struct bus_type *bus) {
    if (!bus) return;
    list_del(&bus->bus_list);
}

static int driver_probe_device(struct device_driver *drv, struct device *dev) {
    if (drv->bus != dev->bus) return -1;

    if (dev->bus->match) {
        if (!dev->bus->match(dev, drv)) {
            return -1;
        }
    }

    if (drv->probe) {
        int ret = drv->probe(dev);
        if (ret == 0) {
            dev->driver = drv;
            kprint_str("[Driver] Bound ");
            kprint_str(drv->name);
            kprint_str(" to ");
            kprint_str(dev->name);
            kprint_newline();
            return 0;
        }
    }
    return -1;
}

int driver_register(struct device_driver *drv) {
    if (!drv || !drv->bus) return -1;

    list_add_tail(&drv->bus_list, &drv->bus->drv_list);
    list_add_tail(&drv->global_list, &global_driver_list);

    struct list_head *pos;
    struct device *dev;
    list_for_each(pos, &drv->bus->dev_list) {
        dev = list_entry(pos, struct device, bus_list);
        if (!dev->driver) {
            driver_probe_device(drv, dev);
        }
    }

    return 0;
}

void driver_unregister(struct device_driver *drv) {
    if (!drv) return;
    list_del(&drv->bus_list);
    list_del(&drv->global_list);
}

int device_register(struct device *dev) {
    if (!dev || !dev->bus) return -1;

    list_add_tail(&dev->bus_list, &dev->bus->dev_list);
    list_add_tail(&dev->global_list, &global_device_list);

    struct list_head *pos;
    struct device_driver *drv;
    list_for_each(pos, &dev->bus->drv_list) {
        drv = list_entry(pos, struct device_driver, bus_list);
        if (driver_probe_device(drv, dev) == 0) {
            break;
        }
    }

    return 0;
}

void device_for_each(void (*fn)(struct device *)) {
    struct list_head *pos;
    struct device *dev;
    list_for_each(pos, &global_device_list) {
        dev = list_entry(pos, struct device, global_list);
        if(fn) fn(dev);
    }
}

void device_dump_all() {
    kprint_str("Device List:\n");
    struct list_head *pos;
    struct device *dev;
    list_for_each(pos, &global_device_list) {
        dev = list_entry(pos, struct device, global_list);
        kprint_str("- ");
        if (dev->name[0]) {
            kprint_str(dev->name);
        } else {
            kprint_str("Unnamed");
        }
        
        kprint_str(" (Bus: ");
        if (dev->bus && dev->bus->name) {
            kprint_str(dev->bus->name);
        } else {
            kprint_str("None");
        }
        kprint_str(")\n");
    }
}

void device_unregister(struct device *dev) {
    if (!dev) return;
    if (dev->driver && dev->driver->remove) {
        dev->driver->remove(dev);
    }
    list_del(&dev->bus_list);
    list_del(&dev->global_list);
}

int device_suspend(struct device *dev) {
    if (!dev || !dev->driver) return 0;
    if (dev->driver->suspend) {
        return dev->driver->suspend(dev);
    }
    return 0;
}

int device_resume(struct device *dev) {
    if (!dev || !dev->driver) return 0;
    if (dev->driver->resume) {
        return dev->driver->resume(dev);
    }
    return 0;
}
