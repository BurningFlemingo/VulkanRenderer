#include "Memory.h"
#include <vulkan/vulkan_core.h>
#include <iostream>

namespace {
	uint32_t findMemoryTypeIndex(
		uint32_t disiredProperties,
		const VkMemoryType requiredMemTypes[],
		uint32_t memTypeCount,
		uint32_t allowedMemTypes
	);
	VkMemoryAllocateInfo getMemoryAllocInfo(
		const VkPhysicalDevice pDevice,
		const VkDevice device,
		const VkBuffer buffer,
		const VkMemoryPropertyFlags memoryProps
	);
}  // namespace

BufferInfo createBuffer(
	const VkPhysicalDevice pDevice,
	const VkDevice device,
	const size_t size,
	const VkBufferUsageFlags usage,
	const VkMemoryPropertyFlags memProps
) {
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	VkBuffer bufferHandle{};
	vkCreateBuffer(device, &bufferCreateInfo, nullptr, &bufferHandle);

	VkMemoryAllocateInfo memAllocInfo{
		getMemoryAllocInfo(pDevice, device, bufferHandle, memProps)
	};
	VkDeviceMemory bufferMemory{};
	VkResult res{
		vkAllocateMemory(device, &memAllocInfo, nullptr, &bufferMemory)
	};
	if (res != VK_SUCCESS) {
		std::cout << "could not allocate vertex buffer" << std::endl;
	}

	vkBindBufferMemory(device, bufferHandle, bufferMemory, 0);

	BufferInfo buffer{ .handle = bufferHandle, .memory = bufferMemory };

	return buffer;
}

void destroyBuffer(const VkDevice device, BufferInfo buffer) {
	vkFreeMemory(device, buffer.memory, nullptr);
	vkDestroyBuffer(device, buffer.handle, nullptr);
}

namespace {
	uint32_t findMemoryTypeIndex(
		uint32_t disiredProperties,
		const VkMemoryType requiredMemTypes[],
		uint32_t memTypeCount,
		uint32_t allowedMemTypes
	) {
		uint32_t memTypeIndex{};
		for (uint32_t i{}; i < memTypeCount; i++) {
			VkMemoryType memType{ requiredMemTypes[i] };

			if ((allowedMemTypes & (1 << i)) &&
				(memType.propertyFlags & disiredProperties) ==
					disiredProperties) {
				memTypeIndex = i;
				break;
			}
		}

		return memTypeIndex;
	}

	VkMemoryAllocateInfo getMemoryAllocInfo(
		const VkPhysicalDevice pDevice,
		const VkDevice device,
		const VkBuffer buffer,
		const VkMemoryPropertyFlags memoryProps
	) {
		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(pDevice, &memProps);

		uint32_t memTypeIndex{ findMemoryTypeIndex(
			memoryProps,
			memProps.memoryTypes,
			memProps.memoryTypeCount,
			memRequirements.memoryTypeBits
		) };

		VkMemoryAllocateInfo memAllocInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = memTypeIndex,
		};

		return memAllocInfo;
	}
}  // namespace
