#pragma once

#include <string>

// Initialize logging system. 'path' should be a file path, e.g. "logs/app.log".
void init_logging(const std::string& path);
// Shutdown logging and restore std::cout/std::cerr
void shutdown_logging();
