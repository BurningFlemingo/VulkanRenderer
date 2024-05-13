#include "Renderer.h"

#include <SDL2/SDL.h>
#include <SDL_vulkan.h>
#include <ranges>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <array>

#include "Logger.h"
#include "DeletionQueue.h"
#include "Device.h"
#include "Swapchain.h"

#include <iostream>
#include <fstream>
#include <glm/glm.hpp>

#include "Instance.h"
#include "Memory.h"
#include "Commands.h"

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
};

struct FrameState {
	VkFence renderFinishFence;
	VkSemaphore renderFinishSemaphore;
	VkSemaphore imageAvaliableSemaphore;

	VkCommandBuffer cmdBuffer;
};

namespace {
	struct VulkanState {
		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;

		DeletionQueue objectDeletionQueue;

		VkPhysicalDevice pDevice;
		VkDevice device;

		std::unordered_map<QueueFamily, uint32_t> queueFamilyIndices;
		std::unordered_map<QueueFamily, VkQueue> queues;
		VkCommandPool cmdPool;

		VkBuffer vertexBuffer;
		VkDeviceMemory vertexBufferMemory;

		VkSurfaceKHR surface;

		VkSwapchainKHR swapchain;
		VkExtent2D swapchainExtent;
		std::vector<VkImageView> swapchainImageViews;

		std::vector<VkFramebuffer> framebuffers;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;
		VkRenderPass renderPass;

		static constexpr uint32_t FRAMES_IN_FLIGHT{ 2 };
		std::array<FrameState, FRAMES_IN_FLIGHT> frames;
	};

	VulkanState* s_State{ nullptr };

}  // namespace

void VulkanRenderer::init(SDL_Window* window) {
	PYX_ENGINE_ASSERT_WARNING(s_State == nullptr);
	Logger::init();

	// VkInstance, VkDebugUtilsMessengerEXT
	auto [instance, debugMessenger]{ createInstance(window) };

	DeletionQueue objectDeletionQueue{};
	objectDeletionQueue.pushDeleter([=]() {
		deleteInstance(instance, debugMessenger);
	});

	VkSurfaceKHR surface{};
	SDL_Vulkan_CreateSurface(window, instance, &surface);

	objectDeletionQueue.pushDeleter([=]() {
		vkDestroySurfaceKHR(instance, surface, nullptr);
	});

	VkPhysicalDevice pDevice{ findSuitablePhysicalDevice(instance, surface) };
	std::unordered_map<QueueFamily, uint32_t> queueFamilyIndices{
		getDeviceQueueIndices(pDevice, surface)
	};
	VkDevice device{
		createLogicalDevice(queueFamilyIndices, pDevice, surface)
	};

	objectDeletionQueue.pushDeleter([=]() {
		vkDestroyDevice(device, nullptr);
	});

	VkSurfaceCapabilitiesKHR surfaceCapabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		pDevice, surface, &surfaceCapabilities
	);

	SwapchainInfo swapchainInfo{ createSwapchain(
		pDevice, device, surface, window, VulkanState::FRAMES_IN_FLIGHT
	) };

	uint64_t swapchainDeleterHandle{ objectDeletionQueue.pushDeleter([=]() {
		deleteSwapchain(
			device, swapchainInfo.swapchain, swapchainInfo.imageViews
		);
	}) };

	std::unordered_map<QueueFamily, VkQueue> queues;
	for (const auto& index : queueFamilyIndices) {
		VkQueue queue{};
		vkGetDeviceQueue(device, index.second, 0, &queue);
		queues[index.first] = queue;
	}

	VkDescriptorPoolSize dPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
									.descriptorCount = 1 };

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &dPoolSize
	};

	VkDescriptorPool descriptorPool{};
	// vkCreateDescriptorPool(
	// 	device, &descriptorPoolCreateInfo, nullptr, &descriptorPool
	// );

	VkDescriptorSetLayoutBinding setLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayout{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	};

	VkDescriptorSetAllocateInfo dSetAllocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	};
	VkPipelineLayout pipelineLayout{};
	VkResult res{ vkCreatePipelineLayout(
		device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout
	) };

	if (res != VK_SUCCESS) {
		std::cout << "could not create pipeline layout" << std::endl;
	}

	std::vector<VkDynamicState> dynamicState{ VK_DYNAMIC_STATE_SCISSOR,
											  VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (uint32_t)dynamicState.size(),
		.pDynamicStates = dynamicState.data(),
	};

	VkPipelineViewportStateCreateInfo viewportCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	std::vector<uint32_t> vShaderBytes;
	std::vector<uint32_t> fShaderBytes;

	std::ifstream vShaderFile(
		"shaders/First.vert.spv", std::ios::binary | std::ios::ate
	);
	if (vShaderFile.is_open()) {
		uint32_t vShaderSize{ (uint32_t)vShaderFile.tellg() };
		vShaderFile.seekg(0, std::ios::beg);

		vShaderBytes = std::vector<uint32_t>(vShaderSize);
		vShaderFile.read((char*)vShaderBytes.data(), vShaderSize);

		vShaderFile.close();
	}
	std::ifstream fShaderFile(
		"shaders/First.frag.spv", std::ios::binary | std::ios::ate
	);
	if (fShaderFile.is_open()) {
		uint32_t fShaderSize{ (uint32_t)fShaderFile.tellg() };
		fShaderFile.seekg(0, std::ios::beg);

		fShaderBytes = std::vector<uint32_t>(fShaderSize);
		fShaderFile.read((char*)fShaderBytes.data(), fShaderSize);

		fShaderFile.close();
	}

	VkShaderModuleCreateInfo vShaderModuleCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = vShaderBytes.size(),
		.pCode = vShaderBytes.data(),
	};

	VkShaderModuleCreateInfo fShaderModuleCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = fShaderBytes.size(),
		.pCode = fShaderBytes.data(),
	};

	VkShaderModule vShaderModule{};
	VkShaderModule fShaderModule{};
	res = vkCreateShaderModule(
		device, &vShaderModuleCreateInfo, nullptr, &vShaderModule
	);
	if (res != VK_SUCCESS) {
		std::cout << "couldnt create vertex shader module" << std::endl;
	}
	vkCreateShaderModule(
		device, &fShaderModuleCreateInfo, nullptr, &fShaderModule
	);
	if (res != VK_SUCCESS) {
		std::cout << "couldnt create fragment shader module" << std::endl;
	}

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages(2);
	shaderStages[0] = VkPipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vShaderModule,
		.pName = "main",
	};
	shaderStages[1] = VkPipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fShaderModule,
		.pName = "main",
	};

	VkVertexInputBindingDescription inputBindingDescription{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(2
	);
	inputAttributeDescriptions[0] = { .location = 0,
									  .binding = 0,
									  .format = VK_FORMAT_R32G32B32_SFLOAT,
									  .offset = offsetof(Vertex, pos) };
	inputAttributeDescriptions[1] = { .location = 1,
									  .binding = 0,
									  .format = VK_FORMAT_R32G32B32_SFLOAT,
									  .offset = offsetof(Vertex, color) };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &inputBindingDescription,
		.vertexAttributeDescriptionCount =
			(uint32_t)inputAttributeDescriptions.size(),
		.pVertexAttributeDescriptions = inputAttributeDescriptions.data()
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo colorBlendState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachmentState,
	};

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0,
	};

	VkAttachmentDescription imageAttachment{
		.format = swapchainInfo.format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference imageAttachmentRefrence{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpassDescription{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &imageAttachmentRefrence,
	};

	VkSubpassDependency colorAttachDependency{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkRenderPassCreateInfo renderPassCreateInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &imageAttachment,
		.subpassCount = 1,
		.pSubpasses = &subpassDescription,
		.dependencyCount = 1,
		.pDependencies = &colorAttachDependency
	};

	VkRenderPass renderPass{};
	res =
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
	if (res != VK_SUCCESS) {
		std::cout << "could not create render pass" << std::endl;
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)shaderStages.size(),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputStateCreateInfo,
		.pInputAssemblyState = &inputAssemblyCreateInfo,
		.pViewportState = &viewportCreateInfo,
		.pRasterizationState = &rasterizationStateCreateInfo,
		.pMultisampleState = &multisampleStateCreateInfo,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicStateCreateInfo,
		.layout = pipelineLayout,
		.renderPass = renderPass,
		.subpass = 0
	};

	VkPipeline firstGraphicsPipeline{};
	res = vkCreateGraphicsPipelines(
		device, 0, 1, &pipelineCreateInfo, nullptr, &firstGraphicsPipeline
	);
	objectDeletionQueue.pushDeleter([=]() {
		vkDestroyPipeline(device, firstGraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
	});

	VkCommandPoolCreateInfo cmdPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queueFamilyIndices[QueueFamily::graphics],
	};

	VkCommandPool cmdPool{};
	res = vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &cmdPool);
	if (res != VK_SUCCESS) {
		std::cout << "could not create command pool" << std::endl;
	}
	objectDeletionQueue.pushDeleter([=]() {
		vkDestroyCommandPool(device, cmdPool, nullptr);
	});

	VkCommandBufferAllocateInfo cmdBufferAllocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = cmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = VulkanState::FRAMES_IN_FLIGHT,
	};
	std::array<FrameState, VulkanState::FRAMES_IN_FLIGHT> frames;
	std::array<VkCommandBuffer, VulkanState::FRAMES_IN_FLIGHT> cmdBuffers;
	vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, cmdBuffers.data());

	VkFenceCreateInfo fenceCreateInfo{ .sType =
										   VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
									   .flags = VK_FENCE_CREATE_SIGNALED_BIT };
	VkSemaphoreCreateInfo semaphoreCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	for (int i{}; i < VulkanState::FRAMES_IN_FLIGHT; i++) {
		VkSemaphore renderFinishSemaphore{};
		VkSemaphore imageAvaliableSemaphore{};
		VkFence renderFinishFence{};
		vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFinishFence);
		vkCreateSemaphore(
			device, &semaphoreCreateInfo, nullptr, &imageAvaliableSemaphore
		);
		vkCreateSemaphore(
			device, &semaphoreCreateInfo, nullptr, &renderFinishSemaphore
		);

		objectDeletionQueue.pushDeleter([=]() {
			vkDestroyFence(device, renderFinishFence, nullptr);
			vkDestroySemaphore(device, imageAvaliableSemaphore, nullptr);
			vkDestroySemaphore(device, renderFinishSemaphore, nullptr);
		});

		FrameState fState{
			.renderFinishFence = renderFinishFence,
			.renderFinishSemaphore = renderFinishSemaphore,
			.imageAvaliableSemaphore = imageAvaliableSemaphore,
			.cmdBuffer = cmdBuffers[i],
		};
		frames[i] = fState;
	}

	if (res != VK_SUCCESS) {
		std::cout << "could not create graphics pipeline" << std::endl;
	}

	Vertex vertexData[3]{
		{ .pos = { -0.5f, 0.5f, 0.f }, .color = { 1.f, 0.f, 0.f } },
		{ .pos = { 0.f, -0.5f, 0.f }, .color = { 0.f, 1.f, 0.f } },
		{ .pos = { 0.5f, 0.5f, 0.f }, .color = { 0.f, 0.f, 1.f } },
	};
	BufferInfo stagingBuffer{ createBuffer(
		pDevice,
		device,
		sizeof(vertexData),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	) };

	BufferInfo vertexBuffer{ createBuffer(
		pDevice,
		device,
		sizeof(vertexData),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	) };

	objectDeletionQueue.pushDeleter([=]() {
		destroyBuffer(device, stagingBuffer);
		destroyBuffer(device, vertexBuffer);
	});

	void* mappedStageMemory{};
	vkMapMemory(
		device,
		stagingBuffer.memory,
		0,
		sizeof(vertexData),
		0,
		&mappedStageMemory
	);
	memcpy(mappedStageMemory, vertexData, sizeof(vertexData));
	vkUnmapMemory(device, stagingBuffer.memory);

	copyBuffer(
		device,
		queueFamilyIndices.at(QueueFamily::graphics),
		queues.at(QueueFamily::graphics),
		stagingBuffer.handle,
		vertexBuffer.handle,
		sizeof(vertexData)
	);

	std::vector<VkFramebuffer> framebuffers;
	framebuffers.reserve(swapchainInfo.imageViews.size());

	for (const auto& imageView : swapchainInfo.imageViews) {
		VkFramebufferCreateInfo frameBufferCreateInfo{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = 1,
			.pAttachments = &imageView,
			.width = swapchainInfo.extent.width,
			.height = swapchainInfo.extent.height,
			.layers = 1
		};
		VkFramebuffer framebuffer{};
		vkCreateFramebuffer(
			device, &frameBufferCreateInfo, nullptr, &framebuffer
		);

		framebuffers.emplace_back(framebuffer);
	}
	objectDeletionQueue.pushDeleter([=]() {
		for (const auto& framebuffer : framebuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}
	});

	vkDestroyShaderModule(device, vShaderModule, nullptr);
	vkDestroyShaderModule(device, fShaderModule, nullptr);

	s_State = new VulkanState{
		.instance = instance,
		.debugMessenger = debugMessenger,
		.objectDeletionQueue = objectDeletionQueue,
		.pDevice = pDevice,
		.device = device,
		.queueFamilyIndices = std::move(queueFamilyIndices),
		.queues = std::move(queues),
		.cmdPool = cmdPool,
		.vertexBuffer = vertexBuffer.handle,
		.vertexBufferMemory = vertexBuffer.memory,
		.surface = surface,
		.swapchain = swapchainInfo.swapchain,
		.swapchainExtent = swapchainInfo.extent,
		.swapchainImageViews = swapchainInfo.imageViews,
		.framebuffers = framebuffers,
		.pipeline = firstGraphicsPipeline,
		.pipelineLayout = pipelineLayout,
		.renderPass = renderPass,
		.frames = frames,
	};
}

void VulkanRenderer::renderFrame() {
	uint32_t swapchainImageIndex{};
	VkResult res{};

	for (size_t frameIndex{}; frameIndex < VulkanState::FRAMES_IN_FLIGHT;
		 frameIndex++) {
		FrameState& frame{ s_State->frames[frameIndex] };
		vkWaitForFences(
			s_State->device, 1, &frame.renderFinishFence, VK_TRUE, UINT64_MAX
		);
		vkResetFences(s_State->device, 1, &frame.renderFinishFence);

		res = vkAcquireNextImageKHR(
			s_State->device,
			s_State->swapchain,
			UINT64_MAX,
			frame.imageAvaliableSemaphore,
			0,
			&swapchainImageIndex
		);

		VkRect2D renderArea{
			.extent = s_State->swapchainExtent,
		};

		VkClearValue clearValue{ { { 0.f, 0.f, 0.f, 1.f } } };

		VkRenderPassBeginInfo renderPassBeginInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = s_State->renderPass,
			.framebuffer = s_State->framebuffers[swapchainImageIndex],
			.renderArea = renderArea,
			.clearValueCount = 1,
			.pClearValues = &clearValue,
		};
		VkCommandBufferBeginInfo cmdBufferBeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};

		vkBeginCommandBuffer(frame.cmdBuffer, &cmdBufferBeginInfo);
		vkCmdBeginRenderPass(
			frame.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE
		);

		VkDeviceSize offset[1]{ 0 };
		vkCmdBindVertexBuffers(
			frame.cmdBuffer, 0, 1, &s_State->vertexBuffer, offset
		);

		VkViewport viewport{ .width = (float)s_State->swapchainExtent.width,
							 .height = (float)s_State->swapchainExtent.height,
							 .minDepth = 0.f,
							 .maxDepth = 1.f };
		VkRect2D scissor{ .extent = s_State->swapchainExtent };

		vkCmdSetViewport(frame.cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(frame.cmdBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(
			frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_State->pipeline
		);

		vkCmdDraw(frame.cmdBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(frame.cmdBuffer);

		vkEndCommandBuffer(frame.cmdBuffer);

		VkPipelineStageFlags pipelineStageFlags{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &frame.imageAvaliableSemaphore,
			.pWaitDstStageMask = &pipelineStageFlags,
			.commandBufferCount = 1,
			.pCommandBuffers = &frame.cmdBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &frame.renderFinishSemaphore,

		};
		res = vkQueueSubmit(
			s_State->queues[QueueFamily::graphics],
			1,
			&submitInfo,
			frame.renderFinishFence
		);

		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &frame.renderFinishSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &s_State->swapchain,
			.pImageIndices = &swapchainImageIndex,

		};
		res = vkQueuePresentKHR(
			s_State->queues[QueueFamily::presentation], &presentInfo
		);
	}
}

void VulkanRenderer::cleanup() {
	PYX_ENGINE_ASSERT_WARNING(s_State != nullptr);

	vkDeviceWaitIdle(s_State->device);
	s_State->objectDeletionQueue.flush();

	delete s_State;
}
