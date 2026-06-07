#include "RBGGeneratorVulkan.h"

RBGGeneratorVulkan::RBGGeneratorVulkan() {}
RBGGeneratorVulkan::~RBGGeneratorVulkan() {}

int RBGGeneratorVulkan::init(VIDVulkan *, int, int) { return 0; }
void RBGGeneratorVulkan::resize(int, int) {}
void RBGGeneratorVulkan::update(VIDVulkan::RBGDrawInfo *, const vdp2rotationparameter_struct &, const vdp2rotationparameter_struct &) {}
VkImageView RBGGeneratorVulkan::getTexture(int) { return VK_NULL_HANDLE; }
void RBGGeneratorVulkan::onFinish() {}
void RBGGeneratorVulkan::updateDescriptorSets(int) {}
