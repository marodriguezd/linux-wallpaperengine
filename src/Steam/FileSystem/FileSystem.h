#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Steam::FileSystem {
std::filesystem::path workshopDirectory (int appID, const std::string& contentID);
std::filesystem::path appDirectory (const std::string& appDirectory, const std::string& path);
/**
 * Existing workshop content roots (~/.../content/<appID>) for the given app,
 * first entry wins (same precedence as workshopDirectory()).
 */
std::vector<std::filesystem::path> workshopRoots (int appID);
} // namespace Steam::FileSystem