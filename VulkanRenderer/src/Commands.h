#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>

struct ImmCommandInfo {
	VkCommandPool pool;
	VkCommandBuffer cmdBuffer;
};

ImmCommandInfo
	beginTransientCommand(const VkDevice device, const uint32_t queueIndex);

bool endTransientCommand(
	const VkDevice device, const VkQueue queue, ImmCommandInfo& cmdInfo
);

void copyBuffer(
	const VkDevice device,
	const uint32_t transferQueueIndex,
	const VkQueue transferQueue,
	const VkBuffer srcBuffer,
	const VkBuffer dstBuffer,
	const VkDeviceSize size
);
