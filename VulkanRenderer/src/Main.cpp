#include "Renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <spdlog/spdlog.h>

constexpr int c_WINDOW_WIDTH{ 1920 / 2 };
constexpr int c_WINDOW_HEIGHT{ 1080 / 2 };

int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		SPDLOG_ERROR("couldnt initialize SDL2: {0}", SDL_GetError());
	}
	SDL_Window* window{ SDL_CreateWindow(
		"my app",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		c_WINDOW_WIDTH,
		c_WINDOW_HEIGHT,
		SDL_WINDOW_VULKAN
	) };

	VulkanRenderer::init(window);

	SDL_Event event{};
	bool running{ true };
	while (running) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_KEYDOWN:
					if (event.key.keysym.scancode == SDL_SCANCODE_TAB) {
						running = false;
					}
					break;
			}
		}

		VulkanRenderer::renderFrame();
	}

	VulkanRenderer::cleanup();
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
