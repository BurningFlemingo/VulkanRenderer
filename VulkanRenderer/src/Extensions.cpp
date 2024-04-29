#include "Extensions.h"
#include "Logger.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <string>

#include <vulkan/vulkan.h>

namespace {
	// supportedExtensionsOut will be appended to
	std::vector<const char*> getUnsupportedExtensions(
		const std::vector<const char*>& extensionsToQuery,
		const std::vector<VkExtensionProperties>& supportedExtensionsList,
		std::vector<const char*>& supportedExtensionsOut
	);
}  // namespace

std::vector<const char*> getValidatedInstanceExtensions(
	SDL_Window* window,
	const std::vector<const char*>& requiredExtensions,
	const std::vector<const char*>& optionalExtensions
) {
	uint32_t supportedExtensionsListCount{};
	vkEnumerateInstanceExtensionProperties(
		nullptr, &supportedExtensionsListCount, nullptr
	);

	std::vector<VkExtensionProperties> supportedExtensionsList(
		supportedExtensionsListCount
	);
	vkEnumerateInstanceExtensionProperties(
		nullptr, &supportedExtensionsListCount, supportedExtensionsList.data()
	);

	std::vector<const char*> selectedExtensions{};
	{
		std::vector<const char*> requiredExtensionsNotSupported{
			getUnsupportedExtensions(
				requiredExtensions, supportedExtensionsList, selectedExtensions
			)
		};
		for (const auto& extension : requiredExtensionsNotSupported) {
			PYX_ENGINE_ERROR(
				"required extension: {0} is not supported", extension
			);
		}

		std::vector<const char*> optionalExtensionsNotSupported{
			getUnsupportedExtensions(
				optionalExtensions, supportedExtensionsList, selectedExtensions
			)
		};
		for (const auto& extension : optionalExtensionsNotSupported) {
			PYX_ENGINE_INFO(
				"optional extension: {0} is not supported", extension
			);
		}
	}

	return selectedExtensions;
}

std::optional<std::vector<const char*>>
	getRequiredSurfaceExtensions(SDL_Window* window) {
	uint32_t sdlExtensionCount{};
	SDL_bool res{
		SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr)
	};
	if (!res) {
		PYX_ENGINE_ERROR("could not get required SDL instance extensions");
		return {};
	}

	std::vector<const char*> requiredSurfaceExtensions(sdlExtensionCount);

	res = SDL_Vulkan_GetInstanceExtensions(
		window, &sdlExtensionCount, requiredSurfaceExtensions.data()
	);
	if (!res) {
		PYX_ENGINE_ERROR("could not get required SDL instance extensions");
		return {};
	}

	return requiredSurfaceExtensions;
}
namespace {
	std::vector<const char*> getUnsupportedExtensions(
		const std::vector<const char*>& extensionsToQuery,
		const std::vector<VkExtensionProperties>& supportedExtensionsList,
		std::vector<const char*>& supportedExtensionsOut
	) {
		std::vector<const char*> unsupportedExtensions{};
		for (const auto& extension : extensionsToQuery) {
			bool extensionFound{ std::ranges::any_of(
				supportedExtensionsList,
				[&](const VkExtensionProperties& prop) {
					return std::string_view(extension).compare(
							   std::string_view(prop.extensionName)
						   ) == 0;
				}
			) };
			if (!extensionFound) {
				unsupportedExtensions.emplace_back(extension);
			} else {
				supportedExtensionsOut.emplace_back(extension);
			}
		}

		return unsupportedExtensions;
	}
}  // namespace
