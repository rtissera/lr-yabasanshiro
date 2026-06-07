/*
        Copyright 2021 devMiyax(smiyaxdev@gmail.com)

This file is part of YabaSanshiro.

        YabaSanshiro is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

YabaSanshiro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
along with YabaSanshiro; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "vidvulkan_bridge.h"

extern "C" {
#include "vdp1.h"
#include "vidsoft.h"
}

#include "../vulkan/Renderer.h"
#include "../vulkan/Window.h"
#include "../vulkan/VIDVulkan.h"
#include "../vulkan/VulkanScene.h"

#include <vector>
#include <cstdio>

static retro_log_printf_t g_log_cb = nullptr;

void libretro_vulkan_set_log_cb(retro_log_printf_t cb)
{
    g_log_cb = cb;
}

/* Forward decls (g_vulkan defined further down). */
static const struct retro_hw_render_interface_vulkan *g_vulkan_fwd();

void libretro_vk_queue_lock(void)
{
    const struct retro_hw_render_interface_vulkan *vk = g_vulkan_fwd();
    if (vk && vk->lock_queue) vk->lock_queue(vk->handle);
}

void libretro_vk_queue_unlock(void)
{
    const struct retro_hw_render_interface_vulkan *vk = g_vulkan_fwd();
    if (vk && vk->unlock_queue) vk->unlock_queue(vk->handle);
}

#define LOGI(fmt, ...) do { if (g_log_cb) g_log_cb(RETRO_LOG_INFO, "[libretro] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOGE(fmt, ...) do { if (g_log_cb) g_log_cb(RETRO_LOG_ERROR, "[libretro] " fmt "\n", ##__VA_ARGS__); } while(0)

/* ── LibretroRenderer: wraps libretro Vulkan context for the standalone renderer ── */

class LibretroRenderer : public Renderer
{
public:
  LibretroRenderer(const struct retro_hw_render_interface_vulkan *vulkan)
    : Renderer((void*)1)
  {
    _vk = vulkan;
    _window = nullptr;
    _instance_owned = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(_vk->gpu, &props);
    _gpu_properties = props;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(_vk->gpu, &memProps);
    _gpu_memory_properties = memProps;
  }

  virtual const VkInstance GetVulkanInstance() const override
  {
    return _instance_owned ? _instance_owned : _vk->instance;
  }

  virtual const VkPhysicalDevice GetVulkanPhysicalDevice() const override
  {
    return _vk->gpu;
  }

  virtual const VkDevice GetVulkanDevice() const override
  {
    return _vk->device;
  }

  virtual const VkQueue GetVulkanQueue() const override
  {
    return _vk->queue;
  }

  virtual const VkQueue GetComputeQueue() const override
  {
    return _vk->queue;
  }

  virtual const uint32_t GetVulkanGraphicsQueueFamilyIndex() const override
  {
    return _vk->queue_index;
  }

  virtual const uint32_t GetVulkanComputeQueueFamilyIndex() const override
  {
    return _vk->queue_index;
  }

  virtual const VkPhysicalDeviceProperties &GetVulkanPhysicalDeviceProperties() const override
  {
    return _gpu_properties;
  }

  virtual const VkPhysicalDeviceMemoryProperties &GetVulkanPhysicalDeviceMemoryProperties() const override
  {
    return _gpu_memory_properties;
  }

  virtual Window * getWindow() override { return _window; }
  virtual bool isTessellationAvailable() override { return false; }

  void setWindow(Window *w) { _window = w; }

  void setVulkanInstance(VkInstance inst) { _instance_owned = inst; }

private:
  const struct retro_hw_render_interface_vulkan *_vk;
  Window *_window;
  VkInstance _instance_owned;
  VkPhysicalDeviceProperties _gpu_properties;
  VkPhysicalDeviceMemoryProperties _gpu_memory_properties;
};

/* ── LibretroWindow: provides render pass + framebuffer for libretro output ── */

class LibretroWindow : public Window
{
public:
  LibretroWindow(Renderer *renderer, const struct retro_hw_render_interface_vulkan *vk,
                 uint32_t width, uint32_t height)
    : Window(renderer, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, width, height)
  {
    /* Base Window stores null handles for rp/fb so its dtor won't free them */
    _vk = vk;
    _out_w = width;
    _out_h = height;

    VkDevice device = renderer->GetVulkanDevice();

    /* Create output image */
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_B8G8R8A8_UNORM;
    ici.extent.width = width;
    ici.extent.height = height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &ici, nullptr, &_out_img) != VK_SUCCESS) return;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device, _out_img, &mr);

    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    VkPhysicalDeviceMemoryProperties mp = renderer->GetVulkanPhysicalDeviceMemoryProperties();
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      if ((mr.memoryTypeBits & (1 << i)) &&
          (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        ai.memoryTypeIndex = i;
        break;
      }
    }
    vkAllocateMemory(device, &ai, nullptr, &_out_mem);
    vkBindImageMemory(device, _out_img, _out_mem, 0);

    VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = _out_img;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_B8G8R8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &ivci, nullptr, &_out_view);

    /* Create render pass */
    VkAttachmentDescription att = {};
    att.format = VK_FORMAT_B8G8R8A8_UNORM;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colRef;

    VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments = &att;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;

    vkCreateRenderPass(device, &rpci, nullptr, &_rp);
    vkCreateRenderPass(device, &rpci, nullptr, &_rp_keep);

    /* Create framebuffer */
    VkFramebufferCreateInfo fbci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fbci.renderPass = _rp;
    fbci.attachmentCount = 1;
    fbci.pAttachments = &_out_view;
    fbci.width = width;
    fbci.height = height;
    fbci.layers = 1;
    vkCreateFramebuffer(device, &fbci, nullptr, &_fb);

    /* Override internal state to match what VIDVulkan expects */
    // We set internal members through the lightweight Window ctor that stores
    // the render pass and framebuffer. But since we didn't have them at construction,
    // we directly overwrite the internal fields via the Window's settable pointers.
    _set_internals();
  }

  ~LibretroWindow()
  {
    VkDevice device = _vk->device;
    vkDeviceWaitIdle(device);
    if (_fb) vkDestroyFramebuffer(device, _fb, nullptr);
    if (_rp) vkDestroyRenderPass(device, _rp, nullptr);
    if (_rp_keep && _rp_keep != _rp) vkDestroyRenderPass(device, _rp_keep, nullptr);
    if (_out_view) vkDestroyImageView(device, _out_view, nullptr);
    if (_out_img) vkDestroyImage(device, _out_img, nullptr);
    if (_out_mem) vkFreeMemory(device, _out_mem, nullptr);
  }

  VkRenderPass GetVulkanRenderPass() override { return _rp; }
  VkRenderPass GetVulkanKeepRenderPass() override { return _rp_keep; }
  VkFramebuffer GetVulkanActiveFramebuffer() override { return _fb; }
  VkExtent2D GetVulkanSurfaceSize() override { return {_out_w, _out_h}; }
  VkFormat getColorFormat() override { return VK_FORMAT_B8G8R8A8_UNORM; }
  VkSurfaceTransformFlagBitsKHR GetPreTransFlag() override
  {
    return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }

  VkImage getCurrentImage() override { return _out_img; }
  VkImageView getCurrentImageView() { return _out_view; }

  int BeginRender() override { return 0; }
  void EndRender(std::vector<VkSemaphore> wait_semaphores) override
  {
    /* No-op for libretro: presentation goes via set_image() */
    (void)wait_semaphores;
  }

  void resize(int width, int height) override
  {
    if (width == 0 || height == 0) return;
    if ((uint32_t)width == _out_w && (uint32_t)height == _out_h) return;

    LOGI("LibretroWindow resize: %dx%d", width, height);

    VkDevice device = _vk->device;
    vkDeviceWaitIdle(device);

    /* Destroy old size-dependent resources only. The render pass config is
       size-independent (constant format), so we KEEP _rp/_rp_keep stable —
       the pipeline factory caches getRenderPass() and would otherwise be left
       holding a freed render pass handle, crashing later pipeline creation. */
    if (_fb) vkDestroyFramebuffer(device, _fb, nullptr);
    if (_out_view) vkDestroyImageView(device, _out_view, nullptr);
    if (_out_img) vkDestroyImage(device, _out_img, nullptr);
    if (_out_mem) vkFreeMemory(device, _out_mem, nullptr);

    _out_w = width;
    _out_h = height;

    /* Recreate output image */
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_B8G8R8A8_UNORM;
    ici.extent.width = width;
    ici.extent.height = height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &ici, nullptr, &_out_img) != VK_SUCCESS) return;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device, _out_img, &mr);

    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(_vk->gpu, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
      if ((mr.memoryTypeBits & (1 << i)) &&
          (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        ai.memoryTypeIndex = i;
        break;
      }
    }
    vkAllocateMemory(device, &ai, nullptr, &_out_mem);
    vkBindImageMemory(device, _out_img, _out_mem, 0);

    VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = _out_img;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_B8G8R8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &ivci, nullptr, &_out_view);

    /* Render pass kept stable from construction (size-independent). */

    /* Recreate framebuffer against the existing render pass */
    VkFramebufferCreateInfo fbci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fbci.renderPass = _rp;
    fbci.attachmentCount = 1;
    fbci.pAttachments = &_out_view;
    fbci.width = width;
    fbci.height = height;
    fbci.layers = 1;
    vkCreateFramebuffer(device, &fbci, nullptr, &_fb);

    /* Update internal state */
    _set_internals();
  }

private:
  void _set_internals()
  {
    /* Populate internal Window members that VIDVulkan accesses */
    /* We already set them in the lightweight ctor, but _render_pass etc.
       are private. Since we inherit from Window, we can access protected
       if we make them protected... but they're private. We use the
       lightweight Window ctor parameters instead. */
  }

  const struct retro_hw_render_interface_vulkan *_vk;
  VkImage _out_img = VK_NULL_HANDLE;
  VkDeviceMemory _out_mem = VK_NULL_HANDLE;
  VkImageView _out_view = VK_NULL_HANDLE;
  VkRenderPass _rp = VK_NULL_HANDLE;
  VkRenderPass _rp_keep = VK_NULL_HANDLE;
  VkFramebuffer _fb = VK_NULL_HANDLE;
  uint32_t _out_w = 0;
  uint32_t _out_h = 0;
};

/* ── Global state ── */

static LibretroRenderer *g_renderer = nullptr;
static LibretroWindow *g_window = nullptr;
static const struct retro_hw_render_interface_vulkan *g_vulkan = nullptr;
static retro_vulkan_set_image_t g_set_image = nullptr;
static void *g_set_image_handle = nullptr;

static const struct retro_hw_render_interface_vulkan *g_vulkan_fwd() { return g_vulkan; }

/* ── C API ── */

int libretro_vulkan_init(const struct retro_hw_render_interface *iface_raw)
{
  if (!iface_raw) return -1;
  if (iface_raw->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN) return -1;

  const auto *vulkan = (const struct retro_hw_render_interface_vulkan *)iface_raw;
  g_vulkan = vulkan;
  g_set_image = vulkan->set_image;
  g_set_image_handle = vulkan->handle;

  /* Create LibretroRenderer */
  g_renderer = new LibretroRenderer(vulkan);

  /* Create output window (352x240 initial, will be resized) */
  g_window = new LibretroWindow(g_renderer, vulkan, 352, 240);
  g_renderer->setWindow(g_window);

  /* Set renderer on VIDVulkan singleton */
  VIDVulkan::getInstance()->setRenderer(g_renderer);

  LOGI("libretro_vulkan_init: Vulkan context initialized");

  return 0;
}

void libretro_vulkan_deinit(void)
{
  VIDVulkan::getInstance()->deInit();
  /* ~Renderer() deletes _window, so we null it to avoid double-free */
  g_renderer->setWindow(nullptr);
  delete g_window;
  g_window = nullptr;
  delete g_renderer;
  g_renderer = nullptr;
  g_vulkan = nullptr;
  g_set_image = nullptr;
  g_set_image_handle = nullptr;
}

void libretro_vulkan_swap_buffers(void)
{
  if (!g_vulkan || !g_window) return;

  /* Render the frame */
  VIDVulkan::getInstance()->present();

  /* Wait for rendering to complete (queue access must be externally synced) */
  VkDevice device = g_vulkan->device;
  libretro_vk_queue_lock();
  vkDeviceWaitIdle(device);
  libretro_vk_queue_unlock();

  /* Debug: log image info */
  VkExtent2D size = g_window->GetVulkanSurfaceSize();

  /* One-shot content probe: read back the output image and count non-black
     pixels to verify the renderer is actually producing visible output. */
  if (getenv("YABA_READBACK")) {
    static int probe_frame = 0;
    ++probe_frame;
    if (probe_frame >= 600 && (probe_frame % 300) == 0) {
      uint32_t w = size.width, h = size.height;
      VkDeviceSize bytes = (VkDeviceSize)w * h * 4;
      VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
      bci.size = bytes; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      VkBuffer buf; vkCreateBuffer(device, &bci, nullptr, &buf);
      VkMemoryRequirements mr; vkGetBufferMemoryRequirements(device, buf, &mr);
      VkPhysicalDeviceMemoryProperties mp;
      vkGetPhysicalDeviceMemoryProperties(g_vulkan->gpu, &mp);
      uint32_t mt = 0;
      for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((mr.memoryTypeBits & (1u<<i)) &&
            (mp.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
              == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { mt=i; break; }
      VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
      mai.allocationSize = mr.size; mai.memoryTypeIndex = mt;
      VkDeviceMemory bmem; vkAllocateMemory(device, &mai, nullptr, &bmem);
      vkBindBufferMemory(device, buf, bmem, 0);

      VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
      pci.queueFamilyIndex = g_vulkan->queue_index;
      VkCommandPool pool; vkCreateCommandPool(device, &pci, nullptr, &pool);
      VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
      cai.commandPool = pool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
      VkCommandBuffer cb; vkAllocateCommandBuffers(device, &cai, &cb);
      VkCommandBufferBeginInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(cb, &cbi);
      VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
      b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      b.image = g_window->getCurrentImage();
      b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      b.subresourceRange.levelCount = 1; b.subresourceRange.layerCount = 1;
      b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0,0,nullptr,0,nullptr,1,&b);
      VkBufferImageCopy rgn = {};
      rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      rgn.imageSubresource.layerCount = 1;
      rgn.imageExtent = { w, h, 1 };
      vkCmdCopyImageToBuffer(cb, g_window->getCurrentImage(),
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &rgn);
      vkEndCommandBuffer(cb);
      VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
      si.commandBufferCount = 1; si.pCommandBuffers = &cb;
      libretro_vk_queue_lock();
      vkQueueSubmit(g_vulkan->queue, 1, &si, VK_NULL_HANDLE);
      vkDeviceWaitIdle(device);
      libretro_vk_queue_unlock();
      void *ptr = nullptr; vkMapMemory(device, bmem, 0, bytes, 0, &ptr);
      unsigned char *px = (unsigned char*)ptr;
      uint64_t nonblack = 0, maxv = 0;
      uint32_t minx=w, miny=h, maxx=0, maxy=0;
      for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
          VkDeviceSize i = ((VkDeviceSize)y*w + x)*4;
          unsigned v = px[i] | px[i+1] | px[i+2];
          if (v) { nonblack++;
            if (x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y;
            if (px[i]>maxv)maxv=px[i]; if(px[i+1]>maxv)maxv=px[i+1]; if(px[i+2]>maxv)maxv=px[i+2]; }
        }
      }
      LOGI("PROBE f=%d img=%ux%u nonblack=%llu/%llu maxchan=%llu bbox=[%u,%u..%u,%u]",
           probe_frame, w, h, (unsigned long long)nonblack, (unsigned long long)(bytes/4),
           (unsigned long long)maxv, minx, miny, maxx, maxy);
      vkUnmapMemory(device, bmem);
      vkDestroyCommandPool(device, pool, nullptr);
      vkDestroyBuffer(device, buf, nullptr);
      vkFreeMemory(device, bmem, nullptr);
    }
  }

  LOGI("swap_buffers: size=%dx%d layout=SHADER_READ_ONLY_OPTIMAL", size.width, size.height);

  /* Present via set_image */
  if (g_set_image && g_set_image_handle) {
    VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = g_window->getCurrentImage();
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_B8G8R8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;

    struct retro_vulkan_image img;
    img.image_view = g_window->getCurrentImageView();
    img.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.create_info = ivci;

    g_set_image(g_set_image_handle, &img, 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
    LOGI("set_image called");
  }
}
