// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct SDL_Process;

std::filesystem::path launcher_checkout(const std::filesystem::path& executable_dir);
SDL_Process* start_launcher_worker(const std::filesystem::path& checkout, const std::vector<std::string>& options);
bool open_launcher_log(const std::filesystem::path& checkout, const std::string& run);

struct LauncherLockDeleter {
    void operator()(void* file) const;
};
using LauncherLock = std::unique_ptr<void, LauncherLockDeleter>;
LauncherLock lock_launcher_session(const std::filesystem::path& checkout);
