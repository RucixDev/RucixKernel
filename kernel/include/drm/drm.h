#ifndef _DRM_DRM_H
#define _DRM_DRM_H

#include "types.h"
#include "list.h"
#include <stdbool.h>

 
#define DRM_MODE_TYPE_BUILTIN   (1<<0)
#define DRM_MODE_TYPE_CLOCK_C   (1<<1)
#define DRM_MODE_TYPE_CRTC_C    (1<<2)
#define DRM_MODE_TYPE_PREFERRED (1<<3)
#define DRM_MODE_TYPE_DEFAULT   (1<<4)
#define DRM_MODE_TYPE_USERDEF   (1<<5)
#define DRM_MODE_TYPE_DRIVER    (1<<6)

struct drm_device;
struct drm_connector;
struct drm_encoder;
struct drm_crtc;
struct drm_framebuffer;
struct drm_plane;
struct device;

 
struct drm_display_mode {
    int clock;  
    int hdisplay, hsync_start, hsync_end, htotal;
    int vdisplay, vsync_start, vsync_end, vtotal;
    int vrefresh;  
    uint32_t type;
    uint32_t flags;
    char name[32];
    struct list_head list;
};

 
enum drm_connector_status {
    connector_status_connected = 1,
    connector_status_disconnected = 2,
    connector_status_unknown = 3,
};

struct drm_connector_funcs {
    enum drm_connector_status (*detect)(struct drm_connector *connector, bool force);
    void (*destroy)(struct drm_connector *connector);
};

struct drm_connector {
    struct drm_device *dev;
    struct drm_encoder *encoder;
    enum drm_connector_status status;
    struct list_head modes;  
    const struct drm_connector_funcs *funcs;
    struct list_head list;
};

 
struct drm_encoder_funcs {
    void (*destroy)(struct drm_encoder *encoder);
};

struct drm_encoder {
    struct drm_device *dev;
    struct drm_crtc *crtc;
    const struct drm_encoder_funcs *funcs;
    struct list_head list;
};

 
struct drm_framebuffer {
    struct drm_device *dev;
    struct list_head list;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;  
    uint64_t paddr;   
    void *vaddr;      
};

 
struct drm_crtc_funcs {
    void (*dpms)(struct drm_crtc *crtc, int mode);
    int (*set_config)(struct drm_crtc *crtc, struct drm_framebuffer *fb);
    void (*destroy)(struct drm_crtc *crtc);
};

struct drm_crtc {
    struct drm_device *dev;
    struct drm_framebuffer *primary;  
    struct drm_display_mode mode;
    int x, y;
    const struct drm_crtc_funcs *funcs;
    struct list_head list;
};

 
struct drm_driver {
    const char *name;
    const char *desc;
    const char *date;
    int major;
    int minor;
    int patchlevel;
    
    int (*load)(struct drm_device *dev, unsigned long flags);
    void (*unload)(struct drm_device *dev);
};

struct drm_device {
    struct drm_driver *driver;
    struct list_head crtc_list;
    struct list_head connector_list;
    struct list_head encoder_list;
    struct list_head fb_list;
    
    void *dev_private;  
};

 
struct drm_device *drm_dev_alloc(struct drm_driver *driver, struct device *parent);
int drm_dev_register(struct drm_device *dev, unsigned long flags);
void drm_dev_unregister(struct drm_device *dev);

 
void drm_connector_init(struct drm_device *dev, struct drm_connector *connector, const struct drm_connector_funcs *funcs);
void drm_encoder_init(struct drm_device *dev, struct drm_encoder *encoder, const struct drm_encoder_funcs *funcs);
void drm_crtc_init(struct drm_device *dev, struct drm_crtc *crtc, const struct drm_crtc_funcs *funcs);

#endif
