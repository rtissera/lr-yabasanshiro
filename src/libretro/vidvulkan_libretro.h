#ifndef VIDVULKAN_LIBRETRO_H
#define VIDVULKAN_LIBRETRO_H

#include "vdp1.h"
#include <libretro.h>

#define VIDCORE_VULKAN_LIBRETRO 5

extern VideoInterface_struct VIDVulkan;

void VIDVulkanSwapBuffers(void);
void VIDVulkanSetInterface(const struct retro_hw_render_interface *iface);
int  VIDVulkanIsActive(void);

#endif
