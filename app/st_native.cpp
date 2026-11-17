// SPDX-License-Identifier: GPL-3.0-or-later
// st-native (working name): the program. Plays the user's own Spirit Tracks dump through the game module built from it
// (tools/build/st_build.py) on the engine's runtime: both screens in one window (melonDS's software renderer),
// keyboard, mouse as the touch screen, gamepad, sound, and the cartridge save written atomically. The threading follows
// the engine's own program (game/src/main.cpp, FFCCEOT-Recomp): the runtime runs on a game thread (its guest threads
// are fibers there), and the main thread owns the window and its events.
//
//   st_native <rom.nds> --module <st_game.dll> [options]
//     --save <file>          the cartridge save (default: st-native's folder in the user's application data, BKIE.sav)
//     --no-save              start from a blank cartridge save and write nothing
//     --scale <n>            initial window size, in multiples of 256x384 (default 2)
//     --fullscreen           start in fullscreen (F11 switches modes while playing)
//     --script <file>        scripted input (format below), on top of the keyboard, mouse and gamepad
//     --headless             no window, no sound, no pacing: for scripted runs (needs --exit-after or a `quit` line)
//     --exit-after <frames>  quit after this many frames
//     --hashes <file>        write each frame's number, state hash (main RAM, work RAMs, CPU clock) and the
//                            instructions the fallback interpreter ran
//     --shots <dir>          where the script's `shot` lines write pictures of both screens (BMP); without it,
//                            `shot` lines are ignored
//     --language en|fr|es    the console's language, which the game plays in (default en)
//     --player-name <name>   the proposed name when creating a new save (default Link)
//     --counts <file>        write the call counters (game/st/hooks/st_probe.hpp): hooks named st_count_<addr>,
//                            one line each, after the run
//     --ram-dump <frame>:<file>  write the 4 MB main RAM after that frame (may be repeated)
//     --record <file>        write this session's input as a script (below) when the program ends; with a save,
//                            its starting contents are copied to <file>.start.sav (replay it with --read-only-save)
//     --read-only-save       read the save but never write it (replaying a recorded session)
//     --control <dir>        pause and take commands from <dir>/in.txt (run, touch, drag, press, tap, shot, quit;
//                            see StHost::control), for exploring by hand; with --record the input becomes a script
//     --cpu-speed 1|2|4     the ARM9 at that multiple of the DS's clock (default 4, the engine's speed-up; 1 is
//                            the DS's own timing)
//     --slow-cart            cartridge reads with the card's delays (default: without them, the engine's speed-up)
//     --wireless-group <name>  a local-wireless group of this process's own. Copies running at the same time
//                            otherwise get consecutive instances, each with its own MAC address, which the game sees
//                            (a scripted run then plays differently from frame 7); in a group of its own a run is
//                            instance 0 with the default MAC, exactly as when it runs alone
//     --frame-profile <csv>  numeric production/presentation/audio-queue diagnostics (no game data)
//     --perf, --trace-threads, --trace-calls   the runtime's logs
//   Script: one step per line, "#" starts a comment. Frames count from 1, the first frame shown.
//     <frame> press <KEY>[+<KEY>...] [<frames held, default 6>]
//     <frame> tap <KEY>[+<KEY>...]                           (pressed and released within the frame)
//     <frame> touch <x> <y> [<frames held, default 6>]       (bottom screen, 0-255 x 0-191)
//     <frame> drag <x1> <y1> <x2> <y2> [<frames, default 6>] (a stylus stroke in a straight line)
//     <frame> shot <name>
//     <frame> type <text>                     (only while the save-name editor accepts input)
//     <frame> backspace                       (same editor)
//     <frame> quit
//   Keys: A B X Y L R START SELECT UP DOWN LEFT RIGHT.
// Keyboard: arrows = D-pad, X = A, Z = B, S = X, A = Y, Q = L, W = R, Enter = Start, Backspace = Select,
//           Tab (held) = fast forward, F11 = full screen. Mouse: the bottom screen is the touch screen.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ntr/recomp.h"
#include "ntr/rom.hpp"
#include "ntr/runtime.hpp"
#include "game_session.hpp"
#include "name_keyboard.hpp"
#include "screen_context.hpp"
#include "st_hooks.hpp"
#include "st_probe.hpp"
#include "utf8_path.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr int kW = st::ds_width, kH = st::ds_screen_height;
constexpr uint64_t kFrameNs = 16'715'316;  // one DS frame: 560,190 cycles at 33.513982 MHz (59.8261 Hz)

struct KnownDump {
    const char* sha1;
    const char* code;
    const char* release;
};
constexpr KnownDump kKnown[] = {
    {"1f0921b5e99db9d92d24710f1b004e4f3c997b73", "BKIE", "USA/Australia, revision 0"},
};

struct Options {
    fs::path rom, module, save, script, hashes, shots, counts, record, control, frame_profile;
    std::string wireless_group, player_name = "Link";
    std::vector<std::pair<uint64_t, std::string>> ram_dumps;
    bool no_save = false, read_only_save = false, headless = false, perf = false, trace_threads = false,
         trace_calls = false, slow_cart = false, fullscreen = false, gx_check = false;
    int scale = 2, cpu_speed = 4;
    double cinematic_aspect = 0;
    uint64_t exit_after = 0;
    uint8_t language = 1;
};

// ---- scripted input
enum : uint32_t { kA = 1, kB = 2, kSelect = 4, kStart = 8, kRight = 16, kLeft = 32, kUp = 64, kDown = 128,
                  kR = 256, kL = 512, kX = 1024, kY = 2048 };

struct Step {
    enum Kind { Press, Tap, Touch, Shot, Type, EraseName, Quit } kind;
    uint64_t frame = 0, len = 6;
    uint32_t keys = 0;
    int x = 0, y = 0;
    int x2 = -1, y2 = -1;  // drag: the touch moves in a straight line from (x, y) to (x2, y2) over len frames
    std::string name;
};

uint32_t key_bit(const std::string& k) {
    static const struct { const char* n; uint32_t b; } names[] = {
        {"A", kA}, {"B", kB}, {"X", kX}, {"Y", kY}, {"L", kL}, {"R", kR}, {"START", kStart}, {"SELECT", kSelect},
        {"UP", kUp}, {"DOWN", kDown}, {"LEFT", kLeft}, {"RIGHT", kRight}};
    for (auto& n : names)
        if (k == n.n) return n.b;
    return 0;
}

// One script line. Returns false with `error` set for a bad line; `blank` for a blank line or a comment.
bool parse_step(std::string line, const std::string& where, Step& s, bool& blank, std::string& error) {
    {
        line = line.substr(0, line.find('#'));
        std::istringstream ls(line);
        s = Step{};
        std::string verb;
        blank = !(ls >> s.frame);
        if (blank) return true;
        ls >> verb;
        if (verb == "press" || verb == "tap") {
            std::string keys;
            ls >> keys;
            s.kind = verb == "press" ? Step::Press : Step::Tap;
            for (size_t p = 0; p <= keys.size();) {
                const size_t q = std::min(keys.find('+', p), keys.size());
                const uint32_t b = key_bit(keys.substr(p, q - p));
                if (!b) return error = where + "unknown key in '" + keys + "'", false;
                s.keys |= b;
                p = q + 1;
            }
            if (s.kind == Step::Press) ls >> s.len;
            else s.len = 1;
        } else if (verb == "touch") {
            s.kind = Step::Touch;
            if (!(ls >> s.x >> s.y) || s.x < 0 || s.x >= kW || s.y < 0 || s.y >= kH)
                return error = where + "touch needs x (0-255) and y (0-191)", false;
            ls >> s.len;
        } else if (verb == "drag") {
            s.kind = Step::Touch;
            auto in_screen = [](int x, int y) { return x >= 0 && x < kW && y >= 0 && y < kH; };
            if (!(ls >> s.x >> s.y >> s.x2 >> s.y2) || !in_screen(s.x, s.y) || !in_screen(s.x2, s.y2))
                return error = where + "drag needs x1 y1 x2 y2 (x 0-255, y 0-191)", false;
            ls >> s.len;
        } else if (verb == "shot") {
            s.kind = Step::Shot;
            if (!(ls >> s.name)) return error = where + "shot needs a name", false;
        } else if (verb == "type") {
            s.kind = Step::Type;
            if (!std::getline(ls >> std::ws, s.name)) return error = where + "type needs text", false;
        } else if (verb == "backspace") {
            s.kind = Step::EraseName;
        } else if (verb == "quit") {
            s.kind = Step::Quit;
        } else {
            return error = where + "unknown step '" + verb + "'", false;
        }
        if (s.frame == 0 || s.len == 0) return error = where + "frames count from 1", false;
    }
    return true;
}

bool parse_script(const fs::path& path, std::vector<Step>& steps, std::string& error) {
    std::ifstream in(path);
    if (!in) return error = "cannot read " + path.string(), false;
    std::string line;
    for (int no = 1; std::getline(in, line); no++) {
        Step s;
        bool blank = false;
        if (!parse_step(line, path.string() + ":" + std::to_string(no) + ": ", s, blank, error)) return false;
        if (!blank) steps.push_back(s);
    }
    return true;
}

// Both screens, top above bottom, as a 32-bit BMP (the runtime's 0xAARRGGBB is BMP's byte order).
bool write_bmp(const fs::path& path, const uint32_t* top, const uint32_t* bottom) {
    const uint32_t w = kW, h = bottom ? 2 * kH : kH, size = w * h * 4;
    uint8_t hdr[54] = {'B', 'M'};
    auto put32 = [&](int at, uint32_t v) { std::memcpy(hdr + at, &v, 4); };
    put32(2, 54 + size);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    hdr[26] = 1;
    hdr[28] = 32;
    put32(34, size);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(hdr), sizeof hdr);
    for (int y = int(h) - 1; y >= 0; y--) {  // bottom-up
        const uint32_t* row = y < kH ? top + y * kW : bottom + (y - kH) * kW;
        f.write(reinterpret_cast<const char*>(row), kW * 4);
    }
    return bool(f);
}

bool write_atomically(const fs::path& path, const uint8_t* data, size_t size) {
    const fs::path tmp = fs::path(path).concat(".tmp");
    {
        std::ofstream f(tmp, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data), std::streamsize(size));
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);  // replaces the old save in one step
    return !ec;
}

std::vector<uint8_t> read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// ---- state shared by the main thread (window, events) and the game thread (runtime)
struct Shared {
    std::mutex m;
    std::vector<uint32_t> pixels = std::vector<uint32_t>(kW * kH * 3);
    bool fresh = false, shared_scene = false, cinematic = false;
    uint64_t frame_number = 0, published_ns = 0;
    uint32_t frame_event = 0;
    std::atomic<double> cinematic_aspect{0};
    std::atomic<uint32_t> held{0}, pressed{0};
    std::atomic<int> touch_x{0}, touch_y{0};
    std::atomic<bool> touching{false}, fast{false}, quit{false}, done{false};
    std::mutex pause_mutex;
    std::condition_variable pause_cv;
    bool paused = false;
};

void queue_name_text(st::NameKeyboard& keyboard, std::string_view text) {
    const char* next = text.data();
    size_t remaining = text.size();
    while (remaining) keyboard.queue(SDL_StepUTF8(&next, &remaining));
}

class StHost : public st::NameHost {
public:
    StHost(const Options& o, Shared& sh, std::vector<Step> script, FILE* hashes, bool track_scene)
        : o_(o), sh_(sh), script_(std::move(script)), hashes_(hashes), track_scene_(track_scene) {}
    void attach(ntr::Runtime* rt) { rt_ = rt; }
    uint64_t frames() const { return frame_; }
    uint64_t interp_total() const { return interp_total_; }
    uint64_t cinematic_frames() const { return cinematic_frames_; }
    void set_audio(SDL_AudioStream* a) { audio_ = a; }
    void pause_audio(bool paused) {
        if (!audio_) return;
        std::lock_guard lock(audio_mutex_);
        audio_paused_ = paused;
        audio_started_ = false;
        SDL_PauseAudioStreamDevice(audio_);
        SDL_ClearAudioStream(audio_);
    }
    void set_profile(std::ostream* stream) { profile_ = stream; }

    bool on_frame(const ntr::Frame& f) override {
        const auto profile_start = profile_ ? SDL_GetTicksNS() : 0;
        ++frame_;
        name_keyboard.finish_frame();
        if (save_dirty_ && frame_ >= save_written_at_ + kSaveQuiet) flush_save();
        interp_total_ += f.interp_steps;
        if (hashes_)  // frame, state hash (RAM + clock), instructions on the interpreter, RAM-only hash
            std::fprintf(hashes_, "%llu %016llx %llu %016llx\n", (unsigned long long)frame_,
                         (unsigned long long)f.state_hash, (unsigned long long)f.interp_steps,
                         (unsigned long long)f.ram_hash);
        if (!o_.control.empty() && !control(f)) return false;
        const bool shared_scene = track_scene_ && screen_context_.update(*rt_, f);
        const double aspect = o_.cinematic_aspect > 0 ? o_.cinematic_aspect : sh_.cinematic_aspect.load();
        const auto camera_start = profile_ ? SDL_GetTicksNS() : 0;
        const auto* cinematic = shared_scene && aspect > 0 ? rt_->render_cinematic(aspect, screen_context_.ui_placement()) : nullptr;
        const double camera_ms = profile_ ? double(SDL_GetTicksNS() - camera_start) / 1e6 : 0;
        cinematic_frames_ += cinematic != nullptr;
        if (!o_.headless && f.top && f.bottom) {
            bool wake = false;
            {
                std::lock_guard<std::mutex> lock(sh_.m);
                wake = !sh_.fresh;
                std::memcpy(sh_.pixels.data(), f.top, kW * kH * 4);
                std::memcpy(sh_.pixels.data() + kW * kH, f.bottom, kW * kH * 4);
                sh_.shared_scene = shared_scene;
                sh_.cinematic = cinematic != nullptr;
                if (cinematic) std::memcpy(sh_.pixels.data() + 2 * kW * kH, cinematic, kW * kH * 4);
                sh_.frame_number = frame_;
                sh_.published_ns = SDL_GetTicksNS();
                sh_.fresh = true;
            }
            // Coalesce readiness notifications while a newer frame is already pending. Wake
            // the window promptly instead of adding up to 4 ms of polling jitter to each frame.
            if (wake && sh_.frame_event) {
                SDL_Event event{};
                event.type = sh_.frame_event;
                SDL_PushEvent(&event);
            }
        }
        if (profile_) {
            const int queued = audio_ ? SDL_GetAudioStreamQueued(audio_) : 0;
            *profile_ << frame_ << ',' << profile_start << ','
                << (profile_previous_ ? double(profile_start - profile_previous_) / 1e6 : 0) << ','
                << f.emu_ms << ',' << camera_ms << ',' << double(SDL_GetTicksNS() - profile_start) / 1e6 << ','
                << double(queued) / 192.0 << ',' << audio_empty_feeds_ << ',' << audio_dropped_samples_ << '\n';
            profile_previous_ = profile_start;
        }
        // The script's steps for this frame; input set now is what the game reads in the next frame.
        uint32_t held = 0, pressed = 0;
        bool touch = false;
        int tx = 0, ty = 0;
        for (const Step& s : script_) {
            const bool active = frame_ >= s.frame && frame_ < s.frame + s.len;
            if (s.kind == Step::Press && active) {
                held |= s.keys;
                if (frame_ == s.frame) pressed |= s.keys;
            } else if (s.kind == Step::Tap && frame_ == s.frame) {
                pressed |= s.keys;
            } else if (s.kind == Step::Touch && active) {
                touch = true, tx = s.x, ty = s.y;
                if (s.x2 >= 0 && s.len > 1) {
                    const int64_t i = int64_t(frame_ - s.frame), n = int64_t(s.len - 1);
                    tx = s.x + int((s.x2 - s.x) * i / n), ty = s.y + int((s.y2 - s.y) * i / n);
                }
            } else if (s.kind == Step::Shot && frame_ == s.frame && f.top && f.bottom && !o_.shots.empty()) {
                const fs::path p = o_.shots / (s.name + ".bmp");
                if (!write_bmp(p, f.top, f.bottom)) std::fprintf(stderr, "cannot write %s\n", p.string().c_str());
                else std::printf("frame %llu: shot %s\n", (unsigned long long)frame_, p.string().c_str());
                if (cinematic)
                    write_bmp(o_.shots / (s.name + "_cinematic.bmp"), cinematic, nullptr);
            } else if (s.kind == Step::Type && frame_ == s.frame) {
                queue_name_text(name_keyboard, s.name);
            } else if (s.kind == Step::EraseName && frame_ == s.frame) {
                name_keyboard.queue(st::NameKeyboard::backspace);
            } else if (s.kind == Step::Quit && frame_ >= s.frame) {
                return false;
            }
        }
        st::probe::current_frame = frame_ + 1;  // calls from now on happen while the next frame is emulated
        if (o_.exit_after && frame_ >= o_.exit_after) return false;
        {
            std::unique_lock lock(sh_.pause_mutex);
            if (sh_.paused) {
                rt_->set_buttons(0, 0);
                rt_->release_touch();
                touched_ = false;
                sh_.pause_cv.wait(lock, [&] { return !sh_.paused; });
                deadline_ = 0;
            }
        }
        if (sh_.quit) return false;
        pace();
        held |= sh_.held.load();
        pressed |= sh_.pressed.exchange(0);
        rt_->set_buttons(held, pressed);
        if (!touch && sh_.touching) touch = true, tx = sh_.touch_x, ty = sh_.touch_y;
        if (touch) rt_->touch(tx, ty), touched_ = true;
        else if (touched_) rt_->release_touch(), touched_ = false;
        if (!o_.record.empty()) record(held, pressed, touch, tx, ty);
        return true;
    }

    // The session's input as script lines (one segment per key and per touch position), in frame order.
    std::vector<std::string> recording() {
        record(0, 0, false, 0, 0);  // close what is still held
        std::sort(rec_.begin(), rec_.end());
        std::vector<std::string> lines;
        for (auto& r : rec_) lines.push_back(r.second);
        return lines;
    }

    void on_audio(const int16_t* samples, size_t frames) override {
        if (!audio_) return;
        std::lock_guard lock(audio_mutex_);
        if (audio_paused_) return;
        const int queued = SDL_GetAudioStreamQueued(audio_);
        if (profile_ && audio_started_ && frame_ >= 180 && queued == 0) ++audio_empty_feeds_;
        if (queued > 48000 * 4 / 10) { // keep latency bounded (~100 ms)
            audio_dropped_samples_ += frames;
            return;
        }
        SDL_PutAudioStreamData(audio_, samples, int(frames * 4));
        // Prime 50 ms before playback (also after resuming). Short scheduling hitches can then
        // consume queued samples instead of immediately starving the device. Sustained slow
        // execution still needs a rendering fix; increasing the buffer cannot repair it.
        if (!audio_started_ && SDL_GetAudioStreamQueued(audio_) >= 48000 * 4 / 20)
            audio_started_ = SDL_ResumeAudioStreamDevice(audio_);
    }

    // The runtime reports every backup write the game makes, and one in-game save is many small writes. Writing the
    // whole file each time stalled the game thread for over a second per save; the file is now written once the game
    // has stopped writing for kSaveQuiet frames, and when the runtime stops (flush_save).
    void on_save(const uint8_t* data, size_t size) override {
        if (o_.no_save || o_.read_only_save) return;
        pending_save_.assign(data, data + size);
        save_dirty_ = true;
        save_written_at_ = frame_;
    }
    void flush_save() {
        if (!save_dirty_) return;
        save_dirty_ = false;
        if (!write_atomically(o_.save, pending_save_.data(), pending_save_.size()))
            std::fprintf(stderr, "cannot write the save %s\n", o_.save.string().c_str());
    }

private:
    // --control <dir>: the game pauses at a frame boundary and waits for commands appended to <dir>/in.txt, one per
    // line: "run <frames>"; "shot <name>" (both screens of the current frame to <dir>/<name>.bmp); "quit"; or a script
    // step without its frame number ("touch x y n", "drag x1 y1 x2 y2 n", "press KEYS n", "tap KEYS"), which starts
    // at the current frame. It reports "ready <frame>" (and "shot <name>", "error <text>") in <dir>/out.txt. With
    // --record, the session's input is written as an ordinary script when it ends: exploration by hand, replayed
    // as a script.
    bool control(const ntr::Frame& f) {
        if (run_left_ > 0) return --run_left_, true;
        const fs::path in = o_.control / "in.txt", out = o_.control / "out.txt";
        auto report = [&](const std::string& text) {
            std::ofstream(out, std::ios::app) << text << "\n";
        };
        report("ready " + std::to_string(frame_));
        for (;;) {
            std::vector<std::string> lines;
            {
                std::ifstream f_in(in);
                for (std::string l; std::getline(f_in, l);) lines.push_back(l);
            }
            if (ctl_lines_ >= lines.size()) {
                if (sh_.quit) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            const std::string line = lines[ctl_lines_++];
            std::istringstream ls(line);
            std::string verb;
            if (!(ls >> verb)) continue;
            if (verb == "run") {
                uint64_t n = 0;
                ls >> n;
                if (n) return run_left_ = n - 1, true;
            } else if (verb == "shot") {
                std::string name;
                ls >> name;
                if (f.top && f.bottom && !name.empty() && write_bmp(o_.control / (name + ".bmp"), f.top, f.bottom))
                    report("shot " + name);
                else report("error shot " + name);
            } else if (verb == "quit") {
                return false;
            } else {
                Step s;
                bool blank = false;
                std::string error;
                if (parse_step(std::to_string(frame_) + " " + line, "control: ", s, blank, error) && !blank &&
                    s.kind != Step::Shot && s.kind != Step::Quit)
                    script_.push_back(s);
                else report("error " + (error.empty() ? line : error));
            }
        }
    }

    // Input given to the game for the frame after frame_, as segments: a key held over frames [from, n) becomes
    // "<from> press KEY <n - from>" (pressed at from), a press and release within one frame "<n> tap KEY", a touch at
    // one position over [from, n) "<from> touch x y <n - from>". Replaying the lines gives the game the same input.
    void record(uint32_t held, uint32_t pressed, bool touch, int tx, int ty) {
        static const char* kNames[12] = {"A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L", "X", "Y"};
        const uint64_t n = frame_;
        auto line = [&](uint64_t at, const std::string& text) { rec_.emplace_back(at, std::to_string(at) + " " + text); };
        for (int k = 0; k < 12; k++) {
            const uint32_t bit = 1u << k;
            const bool down = held & bit;
            const bool again = (pressed & bit) && key_from_[k];  // released and pressed again since the last frame
            if (key_from_[k] && (!down || again)) {
                line(key_from_[k], std::string("press ") + kNames[k] + " " + std::to_string(n - key_from_[k]));
                key_from_[k] = 0;
            }
            if (down && !key_from_[k]) key_from_[k] = n;
            else if (!down && (pressed & bit)) line(n, std::string("tap ") + kNames[k]);
        }
        if (touch_from_ && (!touch || tx != touch_x_ || ty != touch_y_)) {
            line(touch_from_, "touch " + std::to_string(touch_x_) + " " + std::to_string(touch_y_) + " " +
                                  std::to_string(n - touch_from_));
            touch_from_ = 0;
        }
        if (touch && !touch_from_) touch_from_ = n, touch_x_ = tx, touch_y_ = ty;
    }

    void pace() {
        if (o_.headless || sh_.fast) {
            deadline_ = 0;
            return;
        }
        const uint64_t now = SDL_GetTicksNS();
        if (!deadline_ || now > deadline_ + 100'000'000) deadline_ = now;  // first frame, or far behind: resync
        deadline_ += kFrameNs;
        if (deadline_ > now) SDL_DelayPrecise(deadline_ - now);
    }

    const Options& o_;
    Shared& sh_;
    std::vector<Step> script_;
    FILE* hashes_;
    bool track_scene_;
    st::ScreenContext screen_context_;
    ntr::Runtime* rt_ = nullptr;
    SDL_AudioStream* audio_ = nullptr;
    std::mutex audio_mutex_;
    bool audio_started_ = false, audio_paused_ = false;
    std::ostream* profile_ = nullptr;
    uint64_t profile_previous_ = 0, audio_empty_feeds_ = 0, audio_dropped_samples_ = 0;
    uint64_t frame_ = 0, interp_total_ = 0, deadline_ = 0;
    uint64_t cinematic_frames_ = 0;
    static constexpr uint64_t kSaveQuiet = 30;  // half a second without backup writes
    uint64_t run_left_ = 0;   // --control: frames still to run before pausing again
    size_t ctl_lines_ = 0;    // --control: commands read so far
    std::vector<uint8_t> pending_save_;
    bool save_dirty_ = false;
    uint64_t save_written_at_ = 0;
    bool touched_ = false;
    uint64_t key_from_[12] = {}, touch_from_ = 0;  // the recorder's open segments (0: none)
    int touch_x_ = 0, touch_y_ = 0;
    std::vector<std::pair<uint64_t, std::string>> rec_;
};

void* find_hook(const char* name) {
    void* hook = st::find_hook(name);
    if (!hook) hook = st::probe::count_hook(name);
    if (!hook) std::fprintf(stderr, "the game module needs the hook %s, which this program does not have\n", name);
    return hook;
}

struct LoadedModule {
#ifdef _WIN32
    HMODULE handle = nullptr;
    ~LoadedModule() { if (handle) FreeLibrary(handle); }
#else
    void* handle = nullptr;
    ~LoadedModule() { if (handle) dlclose(handle); }
#endif
};

bool load_module(const fs::path& path, LoadedModule& library, ntr::GameCode& code, std::string& error) {
#ifdef _WIN32
    library.handle = LoadLibraryW(fs::absolute(path).c_str());
    if (!library.handle) return error = "cannot load " + path_to_utf8(path) + " (error " + std::to_string(GetLastError()) + ")", false;
    auto symbol = GetProcAddress(library.handle, NTR_GAME_LOAD_SYMBOL);
    auto entry = reinterpret_cast<NtrGameLoadFn>(reinterpret_cast<void*>(symbol));
#else
    library.handle = dlopen(fs::absolute(path).c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library.handle) return error = "cannot load " + path_to_utf8(path), false;
    auto entry = reinterpret_cast<NtrGameLoadFn>(dlsym(library.handle, NTR_GAME_LOAD_SYMBOL));
#endif
    if (!entry) return error = path.string() + " is not a game module", false;
    NtrHostApi api;
    ntr_fill_host_api(&api);
    api.find_hook = find_hook;
    const NtrGameInfo* info = entry(&api);
    if (!info || info->abi != NTR_ABI_VERSION)
        return error = path.string() + " was not built for this program, or lacks a hook it needs", false;
    code = {info->functions, info->function_count, info->overlays, info->overlay_count};
    return true;
}

bool parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        auto next_path = [&] { return path_from_utf8(next()); };
        if (a == "--module") o.module = next_path();
        else if (a == "--save") o.save = next_path();
        else if (a == "--no-save") o.no_save = true;
        else if (a == "--scale") o.scale = std::max(1, std::atoi(next().c_str()));
        else if (a == "--cinematic-aspect") {
            const auto text = next();
            char* end = nullptr;
            o.cinematic_aspect = std::strtod(text.c_str(), &end);
            if (text.empty() || *end || !(o.cinematic_aspect >= 0.5 && o.cinematic_aspect <= 4.0))
                return std::fprintf(stderr, "--cinematic-aspect needs a ratio from 0.5 to 4.0\n"), false;
        }
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--script") o.script = next_path();
        else if (a == "--headless") o.headless = true;
        else if (a == "--gx-check") o.gx_check = true;
        else if (a == "--exit-after") o.exit_after = std::strtoull(next().c_str(), nullptr, 10);
        else if (a == "--hashes") o.hashes = next_path();
        else if (a == "--frame-profile") o.frame_profile = next_path();
        else if (a == "--shots") o.shots = next_path();
        else if (a == "--counts") o.counts = next_path();
        else if (a == "--record") o.record = next_path();
        else if (a == "--read-only-save") o.read_only_save = true;
        else if (a == "--ram-dump") {
            const std::string v = next();
            const size_t colon = v.find(':');
            if (colon == std::string::npos || colon == 0) return std::fprintf(stderr, "--ram-dump <frame>:<file>\n"), false;
            o.ram_dumps.emplace_back(std::strtoull(v.c_str(), nullptr, 10), v.substr(colon + 1));
        }
        else if (a == "--perf") o.perf = true;
        else if (a == "--trace-threads") o.trace_threads = true;
        else if (a == "--trace-calls") o.trace_calls = true;
        else if (a == "--cpu-speed") {
            const int m = std::atoi(next().c_str());
            if (m != 1 && m != 2 && m != 4) return std::fprintf(stderr, "--cpu-speed 1, 2 or 4\n"), false;
            o.cpu_speed = m;
        }
        else if (a == "--slow-cart") o.slow_cart = true;
        else if (a == "--wireless-group") o.wireless_group = next();
        else if (a == "--control") o.control = next_path();
        else if (a == "--player-name") o.player_name = next();
        else if (a == "--language") {
            const std::string l = next();
            o.language = l == "fr" ? 2 : l == "es" ? 5 : 1;
        } else if (!a.empty() && a[0] != '-' && o.rom.empty()) o.rom = path_from_utf8(a);
        else return std::fprintf(stderr, "unknown option %s\n", a.c_str()), false;
    }
    if (o.rom.empty() || o.module.empty()) {
        std::fprintf(stderr, "usage: st_native <rom.nds> --module <st_game.dll> [options] (see app/st_native.cpp)\n");
        return false;
    }
    if (o.headless && !o.exit_after && o.script.empty() && o.control.empty()) {
        std::fprintf(stderr, "--headless needs --exit-after or a script with a quit step\n");
        return false;
    }
    return true;
}

uint32_t key_for(SDL_Scancode sc) {
    switch (sc) {
    case SDL_SCANCODE_UP: return kUp;
    case SDL_SCANCODE_DOWN: return kDown;
    case SDL_SCANCODE_LEFT: return kLeft;
    case SDL_SCANCODE_RIGHT: return kRight;
    case SDL_SCANCODE_X: return kA;
    case SDL_SCANCODE_Z: return kB;
    case SDL_SCANCODE_S: return kX;
    case SDL_SCANCODE_A: return kY;
    case SDL_SCANCODE_Q: return kL;
    case SDL_SCANCODE_W: return kR;
    case SDL_SCANCODE_RETURN: return kStart;
    case SDL_SCANCODE_BACKSPACE: return kSelect;
    default: return 0;
    }
}

uint32_t pad_key_for(Uint8 b) {
    switch (SDL_GamepadButton(b)) {
    case SDL_GAMEPAD_BUTTON_EAST: return kA;  // by position, as on the DS: A right, B bottom, X top, Y left
    case SDL_GAMEPAD_BUTTON_SOUTH: return kB;
    case SDL_GAMEPAD_BUTTON_NORTH: return kX;
    case SDL_GAMEPAD_BUTTON_WEST: return kY;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return kL;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return kR;
    case SDL_GAMEPAD_BUTTON_START: return kStart;
    case SDL_GAMEPAD_BUTTON_BACK: return kSelect;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return kUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return kDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return kLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return kRight;
    default: return 0;
    }
}

}  // namespace

static int run_game(Options o, st::GamePresentation* presentation, st::GamePauseMenu* menu,
                    bool& close_requested, uint64_t test_menu_after = 0) {
    // The dump: a known release only (tools/build/st_build.py builds the module from the same dump).
    std::vector<uint8_t> rom = read_file(o.rom);
    if (rom.size() < 0x200) return std::fprintf(stderr, "cannot read %s\n", path_to_utf8(o.rom).c_str()), 2;
    const std::string sha1 = ntr::to_hex(ntr::sha1(rom));
    const KnownDump* dump = nullptr;
    for (const auto& k : kKnown)
        if (sha1 == k.sha1) dump = &k;
    if (!dump) {
        std::fprintf(stderr, "%s: SHA-1 %s is not a known Spirit Tracks dump\n", path_to_utf8(o.rom).c_str(), sha1.c_str());
        return 2;
    }
    if (!st::select_release(dump->code)) return std::fprintf(stderr, "no addresses for %s\n", dump->code), 2;
    std::printf("ROM: Spirit Tracks %s (%s)\n", dump->release, dump->code);

    LoadedModule library;
    ntr::GameCode code;
    std::string error;
    if (!load_module(o.module, library, code, error)) return std::fprintf(stderr, "%s\n", error.c_str()), 2;
    std::printf("module: %s, %zu recompiled functions, %zu overlays\n", path_to_utf8(o.module).c_str(),
                code.function_count, code.overlay_count);

    std::vector<Step> script;
    if (!o.script.empty() && !parse_script(o.script, script, error)) return std::fprintf(stderr, "%s\n", error.c_str()), 2;
    if (!o.shots.empty()) fs::create_directories(o.shots);
    FILE* hashes = nullptr;
    if (!o.hashes.empty() && !(hashes = std::fopen(o.hashes.string().c_str(), "w")))
        return std::fprintf(stderr, "cannot write %s\n", o.hashes.string().c_str()), 2;

    if (o.frame_profile.empty())
        if (const char* path = std::getenv("ST_NATIVE_FRAME_PROFILE")) o.frame_profile = path_from_utf8(path);
    std::ofstream frame_profile, present_profile;
    if (!o.frame_profile.empty()) {
        frame_profile.open(o.frame_profile);
        present_profile.open(fs::path(o.frame_profile).concat(".present.csv"));
        if (!frame_profile || !present_profile)
            return std::fprintf(stderr, "cannot open frame profile output\n"), 2;
        frame_profile << "frame,timestamp_ns,interval_ms,guest_ms,camera_ms,host_ms,audio_queued_ms,audio_empty_feeds,audio_dropped_samples\n";
        present_profile << "frame,interval_ms,present_ms,age_ms\n";
    }
    const SDL_InitFlags systems = o.headless ? 0 : SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | (presentation ? 0 : SDL_INIT_VIDEO);
    if (!SDL_InitSubSystem(systems))
        return std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError()), 1;
    struct SdlScope {
        SDL_InitFlags systems;
        bool sharing_window;
        ~SdlScope() { if (sharing_window) SDL_QuitSubSystem(systems); else SDL_Quit(); }
    } sdl{systems, presentation != nullptr};

    ntr::RuntimeOptions ro;
    if (!o.no_save) {
        if (o.save.empty()) {
            char* pref = SDL_GetPrefPath("st-native", "st-native");
            o.save = path_from_utf8(pref ? pref : ".") / (std::string(dump->code) + ".sav");
            SDL_free(pref);
        }
        ro.save_data = read_file(o.save);
        std::printf("save: %s (%s%s)\n", path_to_utf8(o.save).c_str(), ro.save_data.empty() ? "new" : "loaded",
                    o.read_only_save ? ", read-only" : "");
        if (!o.record.empty() && !ro.save_data.empty()) {  // a recorded session replays from the same save
            const fs::path start = fs::path(o.record).concat(".start.sav");
            if (!write_atomically(start, ro.save_data.data(), ro.save_data.size()))
                return std::fprintf(stderr, "cannot write %s\n", start.string().c_str()), 2;
        }
    }
    ro.ram_dumps = o.ram_dumps;
    ro.frame_hash = hashes != nullptr;
    ro.perf = o.perf;
    ro.trace_threads = o.trace_threads;
    ro.trace_calls = o.trace_calls;
    ro.gx_check = o.gx_check;
    ro.language = o.language;
    ro.console_name = o.player_name.empty() ? "Link" : o.player_name;
    if (!o.wireless_group.empty()) {  // read by the runtime's local wireless (engine patch 0011)
#ifdef _WIN32
        _putenv_s("NTR_WIRELESS_GROUP", o.wireless_group.c_str());
#else
        setenv("NTR_WIRELESS_GROUP", o.wireless_group.c_str(), 1);
#endif
    }
    ro.cpu_speed = o.cpu_speed;
    ro.fast_cart = !o.slow_cart;
    ro.cinematic_camera = presentation != nullptr || o.cinematic_aspect > 0;

    Shared sh;
    if (!o.headless) {
        const auto event = SDL_RegisterEvents(1);
        if (event != Uint32(-1)) sh.frame_event = event;
    }
    StHost host(o, sh, std::move(script), hashes, ro.cinematic_camera);
    if (frame_profile.is_open()) host.set_profile(&frame_profile);

    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> owned_window(nullptr, SDL_DestroyWindow);
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(nullptr, SDL_DestroyRenderer);
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(nullptr, SDL_DestroyTexture);
    std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> audio(nullptr, SDL_DestroyAudioStream);
    SDL_Window* window = presentation ? presentation->window() : nullptr;
    if (!o.headless) {
        if (!presentation) {
            owned_window.reset(SDL_CreateWindow("Spirit Tracks Native", kW * o.scale, 2 * kH * o.scale, SDL_WINDOW_RESIZABLE));
            window = owned_window.get();
            if (window) renderer.reset(SDL_CreateRenderer(window, nullptr));
            if (!renderer)
                return std::fprintf(stderr, "cannot open the window: %s\n", SDL_GetError()), 1;
            SDL_SetRenderLogicalPresentation(renderer.get(), kW, 2 * kH, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
            if (!SDL_SetRenderVSync(renderer.get(), SDL_RENDERER_VSYNC_ADAPTIVE)) SDL_SetRenderVSync(renderer.get(), 1);
            texture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kW, 2 * kH));
            if (!texture) return std::fprintf(stderr, "cannot create the game texture: %s\n", SDL_GetError()), 1;
            SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST);
        }
        if (o.fullscreen && !SDL_SetWindowFullscreen(window, true))
            std::fprintf(stderr, "cannot start in fullscreen: %s\n", SDL_GetError());
        const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, 48000};
        audio.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr));
        if (!audio) std::fprintf(stderr, "no sound: %s\n", SDL_GetError());
        host.set_audio(audio.get());
    }

    // The game thread: the runtime and its guest threads (fibers) live there.
    int rc = 0;
    std::thread game([&] {
        auto rt = ntr::Runtime::create(std::move(rom), code, host, ro);
        host.attach(rt.get());
        rc = rt->run();
        host.flush_save();  // a save the game finished writing in its last half second
        if (rc != 0) std::fprintf(stderr, "the runtime stopped: %s\n", ntr::last_fatal());
        sh.done = true;
        if (!o.headless) {
            SDL_Event wake{};
            wake.type = SDL_EVENT_USER;
            SDL_PushEvent(&wake);
        }
    });

    bool view_error = false;
    bool saw_shared_scene = false, saw_cinematic = false;
    int test_stage = 0;
    if (!o.headless) {
        uint32_t kb = 0, pad = 0, stick = 0;
        bool mouse_down = false;
        bool menu_open = false;
        uint64_t presented_frames = 0, previous_present_ns = 0;
        std::vector<uint32_t> frame(kW * kH * 3);

        auto change_menu = [&](bool open) {
            kb = pad = stick = 0;
            mouse_down = false;
            sh.held = sh.pressed = 0;
            sh.touching = sh.fast = false;
            host.name_keyboard.discard_pending();
            if (open && SDL_TextInputActive(window)) SDL_StopTextInput(window);
            host.pause_audio(open);
            {
                std::lock_guard lock(sh.pause_mutex);
                sh.paused = open;
            }
            sh.pause_cv.notify_one();
            menu_open = open;
            if (open) menu->open();
            else menu->close();
        };

        auto test_key = [&](SDL_Scancode scancode, SDL_Keycode keycode) {
            SDL_Event key{};
            key.type = SDL_EVENT_KEY_DOWN;
            key.key.windowID = SDL_GetWindowID(window);
            key.key.scancode = scancode;
            key.key.key = keycode;
            key.key.down = true;
            if (!SDL_PushEvent(&key)) view_error = sh.quit = true;
        };
        while (!sh.done) {
            if (presentation) {
                sh.cinematic_aspect = presentation->cinematic_aspect();
                const bool edit_name = !menu_open && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
                                       host.name_keyboard.active();
                if (edit_name && !SDL_TextInputActive(window)) SDL_StartTextInput(window);
                if (!edit_name && SDL_TextInputActive(window)) SDL_StopTextInput(window);
            }
            SDL_Event e;
            if (SDL_WaitEventTimeout(&e, 4)) {
                do {
                    if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                        close_requested = true;
                        sh.quit = true;
                        break;
                    }
                    if (menu && e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                        (e.key.scancode == SDL_SCANCODE_ESCAPE || e.key.scancode == SDL_SCANCODE_F1)) {
                        change_menu(!menu_open);
                        continue;
                    }
                    if (presentation && e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                        e.key.scancode == SDL_SCANCODE_F2) {
                        mouse_down = false;
                        sh.touching = false;
                        presentation->toggle_classic_layout();
                        continue;
                    }
                    if (menu_open) {
                        menu->input(e);
                        switch (menu->action()) {
                        case st::PauseAction::leave: sh.quit = true; break;
                        case st::PauseAction::resume: change_menu(false); break;
                        case st::PauseAction::stay: break;
                        }
                        continue;
                    }
                    switch (e.type) {
                    case SDL_EVENT_WINDOW_FOCUS_LOST: kb = 0; mouse_down = false; sh.touching = false; sh.fast = false; break;
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        if (presentation) mouse_down = sh.touching = false;
                        break;
                    case SDL_EVENT_TEXT_INPUT:
                        if (presentation && SDL_TextInputActive(window)) queue_name_text(host.name_keyboard, e.text.text);
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        if (presentation && SDL_TextInputActive(window) && e.key.scancode == SDL_SCANCODE_BACKSPACE)
                            host.name_keyboard.queue(st::NameKeyboard::backspace);
                        else if (presentation && SDL_TextInputActive(window) && e.key.scancode == SDL_SCANCODE_A &&
                                 (e.key.mod & SDL_KMOD_CTRL) && !e.key.repeat)
                            host.name_keyboard.queue(st::NameKeyboard::select_all);
                        else if (e.key.scancode == SDL_SCANCODE_F11 && !e.key.repeat)
                            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
                        else if (presentation && SDL_TextInputActive(window)) break;
                        else if (e.key.scancode == SDL_SCANCODE_TAB) sh.fast = true;
                        else if (const uint32_t b = key_for(e.key.scancode)) {
                            kb |= b;
                            if (!e.key.repeat) sh.pressed |= b;
                        }
                        break;
                    case SDL_EVENT_KEY_UP:
                        if (e.key.scancode == SDL_SCANCODE_TAB) sh.fast = false;
                        kb &= ~key_for(e.key.scancode);
                        break;
                    case SDL_EVENT_GAMEPAD_ADDED: SDL_OpenGamepad(e.gdevice.which); break;
                    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                        if (const uint32_t b = pad_key_for(e.gbutton.button)) pad |= b, sh.pressed |= b;
                        break;
                    case SDL_EVENT_GAMEPAD_BUTTON_UP: pad &= ~pad_key_for(e.gbutton.button); break;
                    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
                            stick = (stick & ~(kLeft | kRight)) | (e.gaxis.value < -16000 ? kLeft : 0) |
                                    (e.gaxis.value > 16000 ? kRight : 0);
                        else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
                            stick = (stick & ~(kUp | kDown)) | (e.gaxis.value < -16000 ? kUp : 0) |
                                    (e.gaxis.value > 16000 ? kDown : 0);
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                    case SDL_EVENT_MOUSE_MOTION: {
                        if (renderer) SDL_ConvertEventToRenderCoordinates(renderer.get(), &e);
                        float x = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x : e.button.x;
                        float y = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y : e.button.y;
                        if (presentation && !presentation->game_coordinates(x, y, mouse_down)) {
                            mouse_down = false;
                            sh.touching = false;
                            break;
                        }
                        if (!presentation) y -= kH;
                        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
                            mouse_down = x >= 0 && x < kW && y >= 0 && y < kH;
                        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) mouse_down = false;
                        if (mouse_down) {
                            sh.touch_x = std::clamp(int(x), 0, kW - 1);
                            sh.touch_y = std::clamp(int(y), 0, kH - 1);
                        }
                        sh.touching = mouse_down;
                        break;
                    }
                    default: break;
                    }
                } while (SDL_PollEvent(&e));
                sh.held = kb | pad | stick;
            }
            if (sh.quit) {
                if (menu_open) change_menu(false);
                continue;
            }
            if (menu_open) {
                if (!menu->draw()) view_error = sh.quit = true;
                if (test_menu_after && !view_error && (test_stage == 1 || test_stage == 3)) {
                    test_key(test_stage == 1 ? SDL_SCANCODE_F1 : SDL_SCANCODE_ESCAPE,
                             test_stage == 1 ? SDLK_F1 : SDLK_ESCAPE);
                    ++test_stage;
                }
                continue;
            }
            bool draw, shared_scene = false, cinematic = false;
            uint64_t frame_number = 0, published_ns = 0;
            {
                std::lock_guard<std::mutex> lock(sh.m);
                draw = sh.fresh;
                if (draw) {
                    frame.swap(sh.pixels);
                    shared_scene = sh.shared_scene;
                    cinematic = sh.cinematic;
                    frame_number = sh.frame_number;
                    published_ns = sh.published_ns;
                    sh.fresh = false;
                }
            }
            if (!draw) continue;
            const auto present_start = present_profile.is_open() ? SDL_GetTicksNS() : 0;
            if (presentation) {
                saw_shared_scene |= shared_scene;
                saw_cinematic |= cinematic;
                if (!presentation->present_game(frame.data(), shared_scene, mouse_down, cinematic)) view_error = sh.quit = true;
            } else {
                const bool drawn = SDL_UpdateTexture(texture.get(), nullptr, frame.data(), kW * 4) &&
                                   SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255) &&
                                   SDL_RenderClear(renderer.get()) &&
                                   SDL_RenderTexture(renderer.get(), texture.get(), nullptr, nullptr) &&
                                   SDL_RenderPresent(renderer.get());
                if (!drawn) view_error = sh.quit = true;
            }
            if (present_profile.is_open()) {
                const auto end = SDL_GetTicksNS();
                present_profile << frame_number << ','
                    << (previous_present_ns ? double(end - previous_present_ns) / 1e6 : 0) << ','
                    << double(end - present_start) / 1e6 << ',' << double(end - published_ns) / 1e6 << '\n';
                previous_present_ns = end;
            }
            if (test_menu_after && !view_error) {
                ++presented_frames;
                if (test_stage == 0 && presented_frames == test_menu_after) {
                    test_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
                    test_stage = 1;
                } else if (test_stage == 2 && presented_frames == test_menu_after + 10) {
                    test_key(SDL_SCANCODE_F2, SDLK_F2);
                } else if (test_stage == 2 && presented_frames == test_menu_after + 30) {
                    test_key(SDL_SCANCODE_F1, SDLK_F1);
                    test_stage = 3;
                } else if (test_stage == 4 && presented_frames == test_menu_after + 40) {
                    test_key(SDL_SCANCODE_F2, SDLK_F2);
                } else if (test_stage == 4 && presented_frames == test_menu_after + 60) {
                    sh.quit = true;
                }
            }
        }
    }
    if (presentation && SDL_TextInputActive(window)) SDL_StopTextInput(window);
    game.join();
    if (o.cinematic_aspect > 0)
        std::printf("cinematic: %llu frames reframed\n", (unsigned long long)host.cinematic_frames());
    if (!rc && (view_error || (test_menu_after && (test_stage != 4 || !saw_shared_scene || !saw_cinematic)))) rc = 1;
    if (hashes) std::fclose(hashes);
    if (!o.record.empty()) {
        std::ofstream rec(o.record);
        rec << "# Recorded by st_native: " << dump->code << ", " << host.frames() << " frames, "
            << (o.no_save ? "from a blank save (replay with --no-save)"
                          : "from the save copied to " + fs::path(o.record).concat(".start.sav").filename().string() +
                                " (replay with --save <that file> --read-only-save)")
            << ". Our own input data: no game data in this file.\n";
        for (const std::string& line : host.recording()) rec << line << "\n";
        rec << host.frames() << " quit\n";
        std::printf("recorded the session's input: %s\n", o.record.string().c_str());
    }
    if (!o.counts.empty()) {
        if (FILE* f = std::fopen(o.counts.string().c_str(), "w")) {
            st::probe::report(f);
            std::fclose(f);
        } else {
            std::fprintf(stderr, "cannot write %s\n", o.counts.string().c_str());
        }
    }
    std::printf("ran %llu frames; %llu instructions on the fallback interpreter; exit %d\n",
                (unsigned long long)host.frames(), (unsigned long long)host.interp_total(), rc);
    return rc;
}

int st::run_game_cli(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) return 2;
    SDL_SetMainReady();
    bool close_requested = false;
    return run_game(std::move(options), nullptr, nullptr, close_requested);
}

st::GameResult st::play_game_in_window(const GameLaunch& launch, GamePresentation& presentation, GamePauseMenu& menu,
                                      uint64_t test_menu_after) {
    Options o;
    o.rom = launch.rom;
    o.module = launch.module;
    o.language = launch.language;
    o.scale = launch.scale;
    o.fullscreen = launch.fullscreen;
    o.no_save = test_menu_after != 0;
    presentation.set_screen_layout(launch.layout, launch.automatic_join);
    SDL_Window& window = *presentation.window();

    // Resize the existing window for the DS screens and put the launcher back at its previous size on Esc.
    int width = 0, height = 0;
    const bool resize = !(SDL_GetWindowFlags(&window) & SDL_WINDOW_MAXIMIZED) &&
                        SDL_GetWindowSize(&window, &width, &height);
    if (resize && !o.fullscreen) {
        SDL_Rect work_area{};
        const float dpi = SDL_GetWindowDisplayScale(&window);
        const auto size = initial_screen_size(launch.layout);
        const int desired_width = int(size.width * o.scale * dpi);
        const int desired_height = int(size.height * o.scale * dpi);
        if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(&window), &work_area))
            SDL_SetWindowSize(&window, std::min(desired_width, work_area.w), std::min(desired_height, work_area.h));
    }
    bool close_requested = false;
    const int rc = run_game(std::move(o), &presentation, &menu, close_requested, test_menu_after);
    if (!close_requested) {
        if (SDL_GetWindowFlags(&window) & SDL_WINDOW_FULLSCREEN) SDL_SetWindowFullscreen(&window, false);
        if (resize) SDL_SetWindowSize(&window, width, height);
    }
    return {rc, close_requested};
}
