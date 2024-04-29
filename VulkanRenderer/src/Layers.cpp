#include "Layers.h"

#include "Logger.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <string>
#include <ranges>

#include <vulkan/vulkan.h>

namespace {
	// supportedLayersOut will be appended to
	std::vector<const char*> getUnsupportedLayers(
		const std::vector<const char*>& layersToQuery,
		const std::vector<VkLayerProperties>& supportedLayersList,
		std::vector<const char*>& supportedLayersOut
	);
}  // namespace

std::vector<const char*>
	getValidatedLayers(const std::vector<const char*>& layers) {
	uint32_t supportedLayersListCount{};
	vkEnumerateInstanceLayerProperties(&supportedLayersListCount, nullptr);

	std::vector<VkLayerProperties> supportedLayersList(supportedLayersListCount
	);
	vkEnumerateInstanceLayerProperties(
		&supportedLayersListCount, supportedLayersList.data()
	);

	std::vector<const char*> selectedLayers{};

	std::vector<const char*> unsupportedLayers{
		getUnsupportedLayers(layers, supportedLayersList, selectedLayers)
	};

	for (const auto& layer : unsupportedLayers) {
		PYX_ENGINE_ERROR("layer: {0} is not supported", layer);
	}

	return selectedLayers;
}

namespace {
	std::vector<const char*> getUnsupportedLayers(
		const std::vector<const char*>& layersToQuery,
		const std::vector<VkLayerProperties>& supportedLayersList,
		std::vector<const char*>& supportedLayersOut
	) {
		std::vector<const char*> unsupportedLayers{};
		for (const auto& layer : layersToQuery) {
			bool layerFound{ std::ranges::any_of(
				supportedLayersList,
				[&](const VkLayerProperties& prop) {
					return std::string_view(layer).compare(
							   std::string_view(prop.layerName)
						   ) == 0;
				}
			) };
			if (!layerFound) {
				unsupportedLayers.emplace_back(layer);
			} else {
				supportedLayersOut.emplace_back(layer);
			}
		}

		return unsupportedLayers;
	}
}  // namespace
