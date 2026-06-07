#ifndef LIBRETRO_QUEUE_LOCK_H
#define LIBRETRO_QUEUE_LOCK_H

/* VkQueue access must be externally synchronized. Under the libretro Vulkan HW
   context the frontend submits to the same queue from its own WSI/present
   thread, so every vkQueueSubmit / vkQueue/DeviceWaitIdle performed by the
   renderer must be bracketed by these. Outside libretro they compile away. */

#ifdef __LIBRETRO__
#ifdef __cplusplus
extern "C" {
#endif
void libretro_vk_queue_lock(void);
void libretro_vk_queue_unlock(void);
#ifdef __cplusplus
}
#endif
#define VK_Q_LOCK()   libretro_vk_queue_lock()
#define VK_Q_UNLOCK() libretro_vk_queue_unlock()
#else
#define VK_Q_LOCK()   ((void)0)
#define VK_Q_UNLOCK() ((void)0)
#endif

#endif
