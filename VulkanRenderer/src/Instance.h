#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

typedef struct SDL_Window SDL_Window;

struct InstanceObjects {
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
};

InstanceObjects createInstance(SDL_Window* window);

void deleteInstance(
	VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger
);
