#include "Logger.h"
#include <iostream>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <memory>
#include "assert.h"

namespace {
	std::shared_ptr<spdlog::logger> s_EngineLogger{ nullptr };
};

void Logger::init() {
	spdlog::set_level(spdlog::level::trace);
	spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%l---%$] %v");

	s_EngineLogger = spdlog::stdout_color_mt("PyxEngine");
	s_EngineLogger->enable_backtrace(32);
}
std::shared_ptr<spdlog::logger> Logger::getLogger() {
	assert(s_EngineLogger != nullptr);

	return s_EngineLogger;
}
