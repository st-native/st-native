// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_worker.hpp"

#include <SDL3/SDL.h>
#include <windows.h>
#include <shellapi.h>
#include <algorithm>

#include "utf8_path.hpp"

namespace fs = std::filesystem;

namespace {
constexpr const char* kScript = "platform/windows/launcher.py";
}

fs::path launcher_checkout(const fs::path& executable_dir) {
    const auto root = executable_dir.root_path();
    for (auto folder = executable_dir; !folder.empty() && folder != root; folder = folder.parent_path()) {
        if (fs::is_regular_file(folder / kScript)) return folder;
    }
    return {};
}

SDL_Process* start_launcher_worker(const fs::path& checkout, const std::vector<std::string>& options) {
    const fs::path venv = checkout / ".venv/Scripts/pythonw.exe";
    const std::string python = fs::is_regular_file(venv) ? path_to_utf8(venv) : "pythonw.exe";
    const std::string script = path_to_utf8(checkout / kScript);
    std::vector<const char*> argv = {python.c_str(), "-I", "-X", "utf8", "-u", script.c_str()};
    for (const auto& arg : options) argv.push_back(arg.c_str());
    argv.push_back(nullptr);

    const auto properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetStringProperty(properties, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, path_to_utf8(checkout).c_str());
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    auto* process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    return process;
}

bool open_launcher_log(const fs::path& checkout, const std::string& run) {
    if (!run.starts_with("tmp") || run.size() != 11 || !std::ranges::all_of(run, [](char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        })) return false;
    auto log = checkout / "build/launcher/runs" / run / "prepare.log";
    log.make_preferred();
    std::error_code error;
    if (fs::canonical(log, error) != log || error || !fs::is_regular_file(log, error)) return false;
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", log.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

void LauncherLockDeleter::operator()(void* file) const { CloseHandle(file); }

LauncherLock lock_launcher_session(const fs::path& checkout) {
    // Match the byte range held by the Python preparation worker.
    const auto path = checkout / "build/launcher/session.lock";
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    LauncherLock lock(file == INVALID_HANDLE_VALUE ? nullptr : file);
    OVERLAPPED position{};
    if (!lock || !LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &position)) return {};
    return lock;
}
