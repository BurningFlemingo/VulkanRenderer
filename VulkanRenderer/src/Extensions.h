#pragma once

#include <vector>
#include <optional>

// forward declarations
typedef struct SDL_Window SDL_Window;

std::optional<std::vector<const char*>>
	getRequiredSurfaceExtensions(SDL_Window* window);

std::vector<const char*> getValidatedInstanceExtensions(
	SDL_Window* window,
	const std::vector<const char*>& requiredExtensions,
	const std::vector<const char*>& optionalExtensions
);
