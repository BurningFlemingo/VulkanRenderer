#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

struct BufferInfo {
	VkBuffer handle;
	VkDeviceMemory memory;
};

BufferInfo createBuffer(
	const VkPhysicalDevice pDevice,
	const VkDevice device,
	const size_t size,
	const VkBufferUsageFlags usage,
	const VkMemoryPropertyFlags memProps
);

void destroyBuffer(const VkDevice device, BufferInfo buffer);
void copyBuffer(
	const VkDevice device,
	const uint32_t transferQueueIndex,
	const VkQueue transferQueue,
	const VkBuffer srcBuffer,
	const VkBuffer dstBuffer,
	const VkDeviceSize size
);
