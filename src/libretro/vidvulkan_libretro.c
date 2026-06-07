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

#include "vidvulkan_libretro.h"
#include "vidvulkan_bridge.h"
#include <string.h>

/* ── Thin bridge to the standalone CVIDVulkan renderer ── */

/* Include the CVIDVulkan C interface from the standalone renderer */
/* Note: VIDVulkanCInterface.h declares CVIDVulkan and all VIDVulkan* thunks */
/* We compile src/vulkan/VIDVulkan.cpp which defines all thunks and CVIDVulkan */

extern VideoInterface_struct CVIDVulkan;

/* ── Libretro-specific VideoInterface_struct ── */

/* Forward declarations of our wrapper callbacks */
static int  VIDVulkanInitWrapper(void);
static void VIDVulkanDeInitWrapper(void);
static void VIDVulkanResizeWrapper(int, int, unsigned int, unsigned int, int, int);
static int  VIDVulkanIsFullscreenWrapper(void);
static int  VIDVulkanVdp1ResetWrapper(void);
static void VIDVulkanVdp1DrawStartWrapper(void);
static void VIDVulkanVdp1DrawEndWrapper(void);
static void VIDVulkanVdp1NormalSpriteDrawWrapper(u8*, Vdp1*, u8*);
static void VIDVulkanVdp1ScaledSpriteDrawWrapper(u8*, Vdp1*, u8*);
static void VIDVulkanVdp1DistortedSpriteDrawWrapper(u8*, Vdp1*, u8*);
static void VIDVulkanVdp1PolygonDrawWrapper(u8*, Vdp1*, u8*);
static void VIDVulkanVdp1PolylineDrawWrapper(u8*, Vdp1*, u8*);
static void VIDVulkanVdp1LineDrawWrapper(u8*, Vdp1*, u8*);
static void VIDVulkanVdp1UserClippingWrapper(u8*, Vdp1*);
static void VIDVulkanVdp1SystemClippingWrapper(u8*, Vdp1*);
static void VIDVulkanVdp1LocalCoordinateWrapper(u8*, Vdp1*);
static void VIDVulkanVdp1ReadFrameBufferWrapper(u32, u32, void*);
static void VIDVulkanVdp1WriteFrameBufferWrapper(u32, u32, u32);
static void VIDVulkanVdp1EraseWriteWrapper(void);
static void VIDVulkanVdp1FrameChangeWrapper(void);
static int  VIDVulkanVdp2ResetWrapper(void);
static void VIDVulkanVdp2DrawStartWrapper(void);
static void VIDVulkanVdp2DrawEndWrapper(void);
static void VIDVulkanVdp2DrawScreensWrapper(void);
static void VIDVulkanGetGlSizeWrapper(int*, int*);
static void VIDVulkanSetSettingValueWrapper(int, int);
static void VIDVulkanSyncWrapper(void);
static void VIDVulkanGetNativeResolutionWrapper(int*, int*, int*);
static void VIDVulkanVdp2DispOffWrapper(void);
static void VIDVulkanOnUpdateColorRamWordWrapper(u32);

VideoInterface_struct VIDVulkan = {
  VIDCORE_VULKAN_LIBRETRO,
  "Vulkan Video Interface (libretro)",
  VIDVulkanInitWrapper,
  VIDVulkanDeInitWrapper,
  VIDVulkanResizeWrapper,
  VIDVulkanIsFullscreenWrapper,
  VIDVulkanVdp1ResetWrapper,
  VIDVulkanVdp1DrawStartWrapper,
  VIDVulkanVdp1DrawEndWrapper,
  VIDVulkanVdp1NormalSpriteDrawWrapper,
  VIDVulkanVdp1ScaledSpriteDrawWrapper,
  VIDVulkanVdp1DistortedSpriteDrawWrapper,
  VIDVulkanVdp1PolygonDrawWrapper,
  VIDVulkanVdp1PolylineDrawWrapper,
  VIDVulkanVdp1LineDrawWrapper,
  VIDVulkanVdp1UserClippingWrapper,
  VIDVulkanVdp1SystemClippingWrapper,
  VIDVulkanVdp1LocalCoordinateWrapper,
  VIDVulkanVdp1ReadFrameBufferWrapper,
  VIDVulkanVdp1WriteFrameBufferWrapper,
  VIDVulkanVdp1EraseWriteWrapper,
  VIDVulkanVdp1FrameChangeWrapper,
  VIDVulkanVdp2ResetWrapper,
  VIDVulkanVdp2DrawStartWrapper,
  VIDVulkanVdp2DrawEndWrapper,
  VIDVulkanVdp2DrawScreensWrapper,
  VIDVulkanGetGlSizeWrapper,
  VIDVulkanSetSettingValueWrapper,
  VIDVulkanSyncWrapper,
  VIDVulkanGetNativeResolutionWrapper,
  VIDVulkanVdp2DispOffWrapper,
  VIDVulkanOnUpdateColorRamWordWrapper,
  NULL /* GetScreenshot */
};

/* ── Libretro-specific state ── */

static int vulkan_active = 0;

/* ── Wrapper callbacks: delegate to CVIDVulkan ── */

static int VIDVulkanInitWrapper(void)
{
  vulkan_active = 1;
  return CVIDVulkan.Init ? CVIDVulkan.Init() : 0;
}

static void VIDVulkanDeInitWrapper(void)
{
  vulkan_active = 0;
  libretro_vulkan_deinit();
}

static void VIDVulkanResizeWrapper(int a, int b, unsigned int c, unsigned int d, int e, int f)
{
  if (CVIDVulkan.Resize) CVIDVulkan.Resize(a, b, c, d, e, f);
}

static int VIDVulkanIsFullscreenWrapper(void) { return 0; }
static int VIDVulkanVdp1ResetWrapper(void) { return CVIDVulkan.Vdp1Reset ? CVIDVulkan.Vdp1Reset() : 0; }
static void VIDVulkanVdp1DrawStartWrapper(void) { if (CVIDVulkan.Vdp1DrawStart) CVIDVulkan.Vdp1DrawStart(); }
static void VIDVulkanVdp1DrawEndWrapper(void) { if (CVIDVulkan.Vdp1DrawEnd) CVIDVulkan.Vdp1DrawEnd(); }
static void VIDVulkanVdp1NormalSpriteDrawWrapper(u8 *r, Vdp1 *re, u8 *b) {
  if (CVIDVulkan.Vdp1NormalSpriteDraw) CVIDVulkan.Vdp1NormalSpriteDraw(r, re, b);
}
static void VIDVulkanVdp1ScaledSpriteDrawWrapper(u8 *r, Vdp1 *re, u8 *b) {
  if (CVIDVulkan.Vdp1ScaledSpriteDraw) CVIDVulkan.Vdp1ScaledSpriteDraw(r, re, b);
}
static void VIDVulkanVdp1DistortedSpriteDrawWrapper(u8 *r, Vdp1 *re, u8 *b) {
  if (CVIDVulkan.Vdp1DistortedSpriteDraw) CVIDVulkan.Vdp1DistortedSpriteDraw(r, re, b);
}
static void VIDVulkanVdp1PolygonDrawWrapper(u8 *r, Vdp1 *re, u8 *b) {
  if (CVIDVulkan.Vdp1PolygonDraw) CVIDVulkan.Vdp1PolygonDraw(r, re, b);
}
static void VIDVulkanVdp1PolylineDrawWrapper(u8 *r, Vdp1 *re, u8 *b) {
  if (CVIDVulkan.Vdp1PolylineDraw) CVIDVulkan.Vdp1PolylineDraw(r, re, b);
}
static void VIDVulkanVdp1LineDrawWrapper(u8 *r, Vdp1 *re, u8 *b) {
  if (CVIDVulkan.Vdp1LineDraw) CVIDVulkan.Vdp1LineDraw(r, re, b);
}
static void VIDVulkanVdp1UserClippingWrapper(u8 *r, Vdp1 *re) {
  if (CVIDVulkan.Vdp1UserClipping) CVIDVulkan.Vdp1UserClipping(r, re);
}
static void VIDVulkanVdp1SystemClippingWrapper(u8 *r, Vdp1 *re) {
  if (CVIDVulkan.Vdp1SystemClipping) CVIDVulkan.Vdp1SystemClipping(r, re);
}
static void VIDVulkanVdp1LocalCoordinateWrapper(u8 *r, Vdp1 *re) {
  if (CVIDVulkan.Vdp1LocalCoordinate) CVIDVulkan.Vdp1LocalCoordinate(r, re);
}
static void VIDVulkanVdp1ReadFrameBufferWrapper(u32 t, u32 a, void *o) {
  if (CVIDVulkan.Vdp1ReadFrameBuffer) CVIDVulkan.Vdp1ReadFrameBuffer(t, a, o);
}
static void VIDVulkanVdp1WriteFrameBufferWrapper(u32 t, u32 a, u32 v) {
  if (CVIDVulkan.Vdp1WriteFrameBuffer) CVIDVulkan.Vdp1WriteFrameBuffer(t, a, v);
}
static void VIDVulkanVdp1EraseWriteWrapper(void) {
  if (CVIDVulkan.Vdp1EraseWrite) CVIDVulkan.Vdp1EraseWrite();
}
static void VIDVulkanVdp1FrameChangeWrapper(void) {
  if (CVIDVulkan.Vdp1FrameChange) CVIDVulkan.Vdp1FrameChange();
}
static int VIDVulkanVdp2ResetWrapper(void) { return CVIDVulkan.Vdp2Reset ? CVIDVulkan.Vdp2Reset() : 0; }
static void VIDVulkanVdp2DrawStartWrapper(void) {
  if (CVIDVulkan.Vdp2DrawStart) CVIDVulkan.Vdp2DrawStart();
}
static void VIDVulkanVdp2DrawEndWrapper(void) {
  if (CVIDVulkan.Vdp2DrawEnd) CVIDVulkan.Vdp2DrawEnd();
}
static void VIDVulkanVdp2DrawScreensWrapper(void) {
  if (CVIDVulkan.Vdp2DrawScreens) CVIDVulkan.Vdp2DrawScreens();
}
static void VIDVulkanGetGlSizeWrapper(int *w, int *h) {
  if (CVIDVulkan.GetGlSize) CVIDVulkan.GetGlSize(w, h);
}
static void VIDVulkanSetSettingValueWrapper(int t, int v) {
  if (CVIDVulkan.SetSettingValue) CVIDVulkan.SetSettingValue(t, v);
}
static void VIDVulkanSyncWrapper(void) {
  if (CVIDVulkan.Sync) CVIDVulkan.Sync();
}
static void VIDVulkanGetNativeResolutionWrapper(int *w, int *h, int *i) {
  if (CVIDVulkan.GetNativeResolution) CVIDVulkan.GetNativeResolution(w, h, i);
}
static void VIDVulkanVdp2DispOffWrapper(void) {
  if (CVIDVulkan.Vdp2DispOff) CVIDVulkan.Vdp2DispOff();
}
static void VIDVulkanOnUpdateColorRamWordWrapper(u32 a) {
  if (CVIDVulkan.OnUpdateColorRamWord) CVIDVulkan.OnUpdateColorRamWord(a);
}

/* ── Libretro Vulkan interface management ── */

void VIDVulkanSetInterface(const struct retro_hw_render_interface *iface)
{
  if (iface) {
    libretro_vulkan_init(iface);
  }
}

void VIDVulkanSwapBuffers(void)
{
  libretro_vulkan_swap_buffers();
}

int VIDVulkanIsActive(void)
{
  return vulkan_active;
}
