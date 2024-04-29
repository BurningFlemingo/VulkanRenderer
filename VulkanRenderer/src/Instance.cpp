#include "Instance.h"

#include "Logger.h"
#include "Extensions.h"
#include "Layers.h"

#include <vector>
#include <string>
#include <vulkan/vulkan_core.h>

namespace {
	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	) {
		std::string msgIntro{ "[Validation Layer]" };
		switch (messageSeverity) {
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				PYX_ENGINE_INFO("{0} {1}", msgIntro, pCallbackData->pMessage);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
				PYX_ENGINE_TRACE("{0} {1}", msgIntro, pCallbackData->pMessage);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				PYX_ENGINE_WARNING(
					"{0} {1}", msgIntro, pCallbackData->pMessage
				);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				PYX_ENGINE_ERROR("{0} {1}", msgIntro, pCallbackData->pMessage);
				break;
			default:
				break;
		}

		return VK_FALSE;
	}

	constexpr VkDebugUtilsMessengerCreateInfoEXT c_DebugMessengerInfo{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debugCallback
	};

}  // namespace

InstanceObjects createInstance(SDL_Window* window) {
	std::vector<const char*> requiredExtensions{
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
	};
	{
		std::optional<std::vector<const char*>> surfaceExtensions{
			getRequiredSurfaceExtensions(window)
		};
		if (surfaceExtensions.has_value() == true) {
			requiredExtensions.insert(
				requiredExtensions.end(),
				surfaceExtensions.value().begin(),
				surfaceExtensions.value().end()
			);
		}
	}

	std::vector<const char*> optionalExtensions{
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};
	std::vector<const char*> layers{ "VK_LAYER_KHRONOS_validation" };

	std::vector<const char*> instanceExtensions{ getValidatedInstanceExtensions(
		window, requiredExtensions, optionalExtensions
	) };

	std::vector<const char*> instanceLayers{ getValidatedLayers(layers) };

	VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "AppName",
		.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.pEngineName = "PyxEngine",
		.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
		.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0),
	};

	VkInstanceCreateInfo instanceInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = &c_DebugMessengerInfo,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = (uint32_t)layers.size(),
		.ppEnabledLayerNames = layers.data(),
		.enabledExtensionCount = (uint32_t)instanceExtensions.size(),
		.ppEnabledExtensionNames = instanceExtensions.data(),
	};
	VkInstance instance{};
	VkResult res{ vkCreateInstance(&instanceInfo, nullptr, &instance) };
	if (res != VK_SUCCESS) {
		PYX_ENGINE_ERROR("could not create vulkan instance: {0}", (int)res);
	}

	VkDebugUtilsMessengerEXT debugMessenger{};

	auto vkCreateDebugUtilsMessengerEXT{
		reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
		)
	};

	res = vkCreateDebugUtilsMessengerEXT(
		instance, &c_DebugMessengerInfo, nullptr, &debugMessenger
	);

	if (res != VK_SUCCESS) {
		PYX_ENGINE_WARNING(
			"could not create vulkan debug messenger: {0}", (int)res
		);
	}

	InstanceObjects objects{ .instance = instance,
							 .debugMessenger = debugMessenger };
	return objects;
}

void deleteInstance(
	VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger
) {
	auto vkDestroyDebugUtilsMessengerEXT{
		reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
		)
	};

	vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	vkDestroyInstance(instance, nullptr);
}
