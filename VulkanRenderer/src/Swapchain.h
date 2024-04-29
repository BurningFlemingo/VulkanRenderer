#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>

typedef struct SDL_Window SDL_Window;

struct SwapchainInfo {
	VkSwapchainKHR swapchain;
	std::vector<VkImageView> imageViews;
	VkExtent2D extent;

	VkFormat format;
};

SwapchainInfo createSwapchain(
	const VkPhysicalDevice pDevice,
	const VkDevice device,
	const VkSurfaceKHR surface,
	SDL_Window* window,
	const uint32_t imagesToCreate
);

void deleteSwapchain(
	const VkDevice device,
	VkSwapchainKHR swapchain,
	const std::vector<VkImageView>& imageViews
);
