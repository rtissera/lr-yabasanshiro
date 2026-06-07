#ifndef VIDVULKAN_BRIDGE_H
#define VIDVULKAN_BRIDGE_H

#include <libretro.h>
#include <libretro_vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

int libretro_vulkan_init(const struct retro_hw_render_interface *iface);
void libretro_vulkan_deinit(void);
void libretro_vulkan_swap_buffers(void);
void libretro_vulkan_set_log_cb(retro_log_printf_t cb);

/* VkQueue access must be externally synchronized. The frontend may submit to
   the same queue from its own (WSI/present) thread, so every vkQueueSubmit /
   vkQueue/DeviceWaitIdle the renderer performs must be bracketed by these. */
void libretro_vk_queue_lock(void);
void libretro_vk_queue_unlock(void);

#ifdef __cplusplus
}
#endif

#endif
