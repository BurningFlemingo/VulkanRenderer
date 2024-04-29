#include "Commands.h"
#include <iostream>
#include <vulkan/vulkan_core.h>

ImmCommandInfo
	beginTransientCommand(const VkDevice device, const uint32_t queueIndex) {
	VkCommandPoolCreateInfo cmdPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = queueIndex,
	};

	VkCommandPool cmdPool{};
	VkResult res{
		vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &cmdPool)
	};
	if (res != VK_SUCCESS) {
		std::cout << "could not create command pool" << std::endl;
	}
	VkCommandBufferAllocateInfo cmdBufferAllocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = cmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmdBuffer;
	res = vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, &cmdBuffer);

	VkCommandBufferBeginInfo beginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	ImmCommandInfo cmdInfo{ .pool = cmdPool, .cmdBuffer = cmdBuffer };

	return cmdInfo;
}

bool endTransientCommand(
	const VkDevice device, const VkQueue queue, ImmCommandInfo& cmdInfo
) {
	vkEndCommandBuffer(cmdInfo.cmdBuffer);
	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdInfo.cmdBuffer,
	};
	VkResult res{ vkQueueSubmit(queue, 1, &submitInfo, 0) };

	vkQueueWaitIdle(queue);

	vkDestroyCommandPool(device, cmdInfo.pool, nullptr);

	return res;
}

void copyBuffer(
	const VkDevice device,
	const uint32_t transferQueueIndex,
	const VkQueue transferQueue,
	const VkBuffer srcBuffer,
	const VkBuffer dstBuffer,
	const VkDeviceSize size
) {
	ImmCommandInfo cmdInfo{ beginTransientCommand(device, transferQueueIndex) };

	VkBufferCopy bufCpy{ .size = size };
	vkCmdCopyBuffer(cmdInfo.cmdBuffer, srcBuffer, dstBuffer, 1, &bufCpy);

	endTransientCommand(device, transferQueue, cmdInfo);
}
