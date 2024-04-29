#pragma once
#include <spdlog/spdlog.h>

namespace Logger {
	void init();

	std::shared_ptr<spdlog::logger> getLogger();
}  // namespace Logger

#define PYX_ENGINE_ERROR(...)                \
	Logger::getLogger()->error(__VA_ARGS__); \
	Logger::getLogger()->dump_backtrace()
#define PYX_ENGINE_WARNING(...) Logger::getLogger()->warn(__VA_ARGS__)
#define PYX_ENGINE_INFO(...) Logger::getLogger()->info(__VA_ARGS__)
#define PYX_ENGINE_TRACE(...) Logger::getLogger()->trace(__VA_ARGS__)
#define PYX_ENGINE_DEBUG(...) Logger::getLogger()->debug(__VA_ARGS__)

#define PYX_ENGINE_ASSERT_ERROR(x)                       \
	{                                                    \
		if (!(x)) {                                      \
			PYX_ENGINE_ERROR(                            \
				"[[assertion failure]] {0} | {1} : {2}", \
				__FUNCTION__,                            \
				__FILE__,                                \
				__LINE__                                 \
			);                                           \
		}                                                \
	}

#define PYX_ENGINE_ASSERT_WARNING(x)                     \
	{                                                    \
		if (!(x)) {                                      \
			PYX_ENGINE_WARNING(                          \
				"[[assertion failure]] {0} | {1} : {2}", \
				__FUNCTION__,                            \
				__FILE__,                                \
				__LINE__                                 \
			);                                           \
		}                                                \
	}

#define PYX_ENGINE_ASSERT_INFO(x)                        \
	{                                                    \
		if (!(x)) {                                      \
			PYX_ENGINE_INFO(                             \
				"[[assertion failure]] {0} | {1} : {2}", \
				__FUNCTION__,                            \
				__FILE__,                                \
				__LINE__                                 \
			);                                           \
		}                                                \
	}

#define VK_CHECK(x)                                              \
	{                                                            \
		if ((x) != VK_SUCCESS) {                                 \
			PYX_ENGINE_ASSERT_WARNING("Vulkan function failed"); \
		}                                                        \
	}
