#include "Device.h"

#include "stdint.h"
#include "Logger.h"
#include <vulkan/vulkan_core.h>
#include <iostream>
#include <set>

#include <unordered_map>

namespace {
	struct PhysicalDeviceCapabilities {
		VkPhysicalDeviceType deviceType;
		bool graphicsSupport;
		bool surfaceSupport;
	};

	PhysicalDeviceCapabilities queryPhysicalDeviceCapabilities(
		const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
	);
}  // namespace

VkPhysicalDevice findSuitablePhysicalDevice(
	const VkInstance instance, const VkSurfaceKHR surface
) {
	uint32_t pDeviceCount{};
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &pDeviceCount, nullptr));

	std::vector<VkPhysicalDevice> pDevices(pDeviceCount);
	VK_CHECK(
		vkEnumeratePhysicalDevices(instance, &pDeviceCount, pDevices.data())
	);

	uint32_t heighestScore{};
	VkPhysicalDevice pDevice{};
	for (const auto& pDeviceTest : pDevices) {
		PhysicalDeviceCapabilities pCapabilities{
			queryPhysicalDeviceCapabilities(pDeviceTest, surface)
		};

		uint32_t score{};
		switch (pCapabilities.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: {
				score += 400;
			} break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: {
				score += 300;
			} break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU: {
				score += 200;
			} break;
			default: {
				score += 100;
			}
		}

		if (pCapabilities.surfaceSupport) {
			score += 5000;
		}
		if (pCapabilities.graphicsSupport) {
			score += 5000;
		}

		if (score >= heighestScore) {
			pDevice = pDeviceTest;
			heighestScore = score;
		}
	}

	return pDevice;
}

VkDevice createLogicalDevice(
	const std::unordered_map<QueueFamily, uint32_t> queueFamilyIndices,
	const VkPhysicalDevice pDevice,
	const VkSurfaceKHR surface
) {
	std::vector<const char*> requiredDeviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	};

	VkPhysicalDeviceVulkan13Features vulkan13Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE,
	};
	VkPhysicalDeviceVulkan12Features vulkan12Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &vulkan13Features,
		.descriptorIndexing = VK_TRUE,
		.bufferDeviceAddress = VK_TRUE,

	};
	VkPhysicalDeviceVulkan11Features vulkan11Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &vulkan12Features
	};
	VkPhysicalDeviceFeatures defaultFeatures{
		.samplerAnisotropy = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 requiredFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &vulkan11Features,
		.features = defaultFeatures,
	};

	std::vector<float> prioritys(queueFamilyIndices.size(), 1.f);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	std::set<uint32_t> uniqueIndices;
	for (const auto& queueFamily : queueFamilyIndices) {
		if (uniqueIndices.contains(queueFamily.second)) {
			continue;
		}
		VkDeviceQueueCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = queueFamily.second,
			.queueCount = (uint32_t)queueFamilyIndices.count(queueFamily.first),
			.pQueuePriorities = prioritys.data()
		};
		queueCreateInfos.emplace_back(info);
		uniqueIndices.insert(queueFamily.second);
	}

	VkDeviceCreateInfo deviceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &requiredFeatures,
		.queueCreateInfoCount = (uint32_t)queueCreateInfos.size(),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = (uint32_t)requiredDeviceExtensions.size(),
		.ppEnabledExtensionNames = requiredDeviceExtensions.data(),
	};

	VkDevice device{};
	VK_CHECK(vkCreateDevice(pDevice, &deviceCreateInfo, nullptr, &device));

	return device;
}

std::unordered_map<QueueFamily, uint32_t> getDeviceQueueIndices(
	const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
) {
	uint32_t queueFamilyPropsCount{};
	vkGetPhysicalDeviceQueueFamilyProperties(
		pDevice, &queueFamilyPropsCount, nullptr
	);

	std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropsCount
	);
	vkGetPhysicalDeviceQueueFamilyProperties(
		pDevice, &queueFamilyPropsCount, queueFamilyProps.data()
	);

	bool graphicsQueueFound{};
	bool presentationQueueFound{};
	bool transferQueueFound{};
	bool computeQueueFound{};

	std::unordered_map<QueueFamily, uint32_t> queueFamilyToIndex;
	for (uint32_t i{}; i < queueFamilyProps.size(); i++) {
		VkQueueFamilyProperties& queueFamily{ queueFamilyProps[i] };

		for (uint32_t j{}; j < queueFamily.queueCount; j++) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT &&
				!graphicsQueueFound) {
				queueFamilyToIndex[QueueFamily::graphics] = i;
				graphicsQueueFound = true;
				continue;
			}

			VkBool32 presentationSupport{};
			vkGetPhysicalDeviceSurfaceSupportKHR(
				pDevice, i, surface, &presentationSupport
			);
			if (presentationSupport && !presentationQueueFound) {
				queueFamilyToIndex[QueueFamily::presentation] = i;
				presentationQueueFound = true;
				continue;
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT &&
				!computeQueueFound) {
				queueFamilyToIndex[QueueFamily::compute] = i;
				computeQueueFound = true;
				continue;
			}
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT &&
				!transferQueueFound) {
				queueFamilyToIndex[QueueFamily::transfer] = i;
				transferQueueFound = true;
				continue;
			}
		}
	}

	return queueFamilyToIndex;
}

namespace {
	PhysicalDeviceCapabilities queryPhysicalDeviceCapabilities(
		const VkPhysicalDevice pDevice, const VkSurfaceKHR surface
	) {
		VkPhysicalDeviceProperties props{};
		VkPhysicalDeviceFeatures feats{};
		vkGetPhysicalDeviceProperties(pDevice, &props);
		vkGetPhysicalDeviceFeatures(pDevice, &feats);

		uint32_t queueFamilyPropCount{};
		vkGetPhysicalDeviceQueueFamilyProperties(
			pDevice, &queueFamilyPropCount, nullptr
		);

		std::vector<VkQueueFamilyProperties> queueFamilys(queueFamilyPropCount);
		vkGetPhysicalDeviceQueueFamilyProperties(
			pDevice, &queueFamilyPropCount, queueFamilys.data()
		);

		bool surfaceSupported{ false };
		bool graphicsSupported{ false };
		for (uint32_t i{}; i < queueFamilys.size(); i++) {
			VkBool32 queueSupportsSurface{ false };
			vkGetPhysicalDeviceSurfaceSupportKHR(
				pDevice, i, surface, &queueSupportsSurface
			);

			surfaceSupported |= queueSupportsSurface != 0;

			graphicsSupported |=
				queueFamilys[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;

			if (surfaceSupported && graphicsSupported) {
				break;
			}
		}
		PhysicalDeviceCapabilities capabilities{ .deviceType = props.deviceType,
												 .graphicsSupport =
													 graphicsSupported,
												 .surfaceSupport =
													 surfaceSupported };
		return capabilities;
	}
}  // namespace
