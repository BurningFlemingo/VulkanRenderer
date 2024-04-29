#include "Swapchain.h"
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <vector>
#include <vulkan/vulkan_core.h>

struct SurfaceCapabilities {
	uint32_t minImageCount;
	VkExtent2D extent;
	VkSurfaceTransformFlagBitsKHR currentTransform;
};

namespace {
	SurfaceCapabilities selectSurfaceCapabilities(
		const VkPhysicalDevice pDevice,
		const VkSurfaceKHR surface,
		const uint32_t imagesToCreate,
		SDL_Window* window
	);

	VkSurfaceFormatKHR selectFormat(
		const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
	);
	VkPresentModeKHR selectPresentMode(
		const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
	);
}  // namespace

SwapchainInfo createSwapchain(
	const VkPhysicalDevice pDevice,
	const VkDevice device,
	const VkSurfaceKHR surface,
	SDL_Window* window,
	const uint32_t imagesToCreate
) {
	SurfaceCapabilities capabilities{
		selectSurfaceCapabilities(pDevice, surface, imagesToCreate, window)
	};

	VkSurfaceFormatKHR surfaceFormat{ selectFormat(pDevice, surface) };
	VkPresentModeKHR presentMode{ selectPresentMode(pDevice, surface) };

	VkSwapchainCreateInfoKHR swapchainCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = capabilities.minImageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = capabilities.extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = presentMode,
		.clipped = VK_TRUE,
	};

	VkSwapchainKHR swapchain{};
	VkResult res{
		vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)
	};

	uint32_t imageCount{};
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);

	std::vector<VkImage> images(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

	std::vector<VkImageView> imageViews(imageCount);
	for (int i{}; i < imageCount; i++) {
		VkImageViewCreateInfo viewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D, 
			.format = surfaceFormat.format, 
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
				.levelCount = 1, 
				.layerCount = 1, 
			},
		};

		vkCreateImageView(device, &viewCreateInfo, nullptr, &imageViews[i]);
	}

	SwapchainInfo info{
		.swapchain = swapchain,
		.imageViews = std::move(imageViews),
		.extent = swapchainCreateInfo.imageExtent,
		.format = swapchainCreateInfo.imageFormat,
	};
	return info;
}

void deleteSwapchain(
	const VkDevice device,
	VkSwapchainKHR swapchain,
	const std::vector<VkImageView>& imageViews
) {
	for (const auto& view : imageViews) {
		vkDestroyImageView(device, view, nullptr);
	}
	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

namespace {
	SurfaceCapabilities selectSurfaceCapabilities(
		const VkPhysicalDevice pDevice,
		const VkSurfaceKHR surface,
		const uint32_t imagesToCreate,
		SDL_Window* window
	) {
		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			pDevice, surface, &surfaceCapabilities
		);

		uint32_t minImageCount{ std::clamp(
			imagesToCreate,
			surfaceCapabilities.minImageCount,
			surfaceCapabilities.maxImageCount
		) };

		VkExtent2D surfaceExtent{};
		if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF) {
			int width, height{};
			SDL_Vulkan_GetDrawableSize(window, &width, &height);
			surfaceExtent.width = std::clamp(
				(uint32_t)width,
				surfaceCapabilities.minImageExtent.width,
				surfaceCapabilities.maxImageExtent.width
			);
			surfaceExtent.height = std::clamp(
				(uint32_t)height,
				surfaceCapabilities.minImageExtent.height,
				surfaceCapabilities.maxImageExtent.height
			);
		} else {
			surfaceExtent = surfaceCapabilities.currentExtent;
		}

		SurfaceCapabilities capabilities{
			.minImageCount = minImageCount,
			.extent = surfaceExtent,
			.currentTransform = surfaceCapabilities.currentTransform,
		};
		return capabilities;
	}

	VkSurfaceFormatKHR selectFormat(
		const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
	) {
		uint32_t surfaceFormatCount{};
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			pDevice, surface, &surfaceFormatCount, nullptr
		);

		std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			pDevice, surface, &surfaceFormatCount, surfaceFormats.data()
		);

		VkFormat format{ VK_FORMAT_UNDEFINED };
		VkColorSpaceKHR colorSpace{};
		for (const auto& surfaceFormat : surfaceFormats) {
			switch (surfaceFormat.format) {
				case VK_FORMAT_R8G8B8A8_SRGB:
				case VK_FORMAT_B8G8R8A8_SRGB:
					format = surfaceFormat.format;
					break;
				default:
					if (format != VK_FORMAT_B8G8R8A8_SRGB &&
						format != VK_FORMAT_R8G8B8A8_SRGB) {
						format = surfaceFormat.format;
					}
					break;
			}

			if (surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				colorSpace = surfaceFormat.colorSpace;
			}
		}
		VkSurfaceFormatKHR surfaceFormat{ .format = format,
										  .colorSpace = colorSpace };

		return surfaceFormat;
	}

	VkPresentModeKHR selectPresentMode(
		const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
	) {
		uint32_t surfacePresentModeCount{};
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			pDevice, surface, &surfacePresentModeCount, nullptr
		);

		std::vector<VkPresentModeKHR> surfacePresentModes(
			surfacePresentModeCount
		);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			pDevice,
			surface,
			&surfacePresentModeCount,
			surfacePresentModes.data()
		);

		VkPresentModeKHR presentMode{ VK_PRESENT_MODE_FIFO_KHR };
		for (const auto& surfacePresentMode : surfacePresentModes) {
			if (surfacePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = surfacePresentMode;
				break;
			}
		}

		return presentMode;
	}

}  // namespace
