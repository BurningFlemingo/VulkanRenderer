#pragma once

#include <unordered_map>
#include <vector>
#include "vulkan/vulkan.h"

enum class QueueFamily {
	none,
	graphics,
	presentation,
	compute,
	transfer,
	nQueueFamilies
};

struct LogicalDeviceInfo {
	VkDevice logicalDevice;
	uint32_t queueFamiliesIndices[(size_t)QueueFamily::nQueueFamilies];
};

VkPhysicalDevice findSuitablePhysicalDevice(
	const VkInstance instance, const VkSurfaceKHR surface
);
std::unordered_map<QueueFamily, uint32_t> getDeviceQueueIndices(
	const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
);
VkDevice createLogicalDevice(
	const std::unordered_map<QueueFamily, uint32_t> queueFamilyIndices,
	const VkPhysicalDevice pDevice,
	const VkSurfaceKHR surface
);
