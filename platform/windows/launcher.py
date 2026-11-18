#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare and run the Windows developer alpha. stdout is the launcher's status channel."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import msvcrt
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
import st_build

ENGINE = ROOT / ".tools/upstream/ffcc"
BIN = ENGINE / "build/release"
BUILD = ROOT / "build/launcher"
MODE = ("--functions", "ours", "--dsprot", "original")
GENERATION = re.compile(r"[a-f0-9]{64}(?:\.previous)?")
MODULE_FILES = {"st_game.dll", "sha256"}
STEPS = {"extract": "Read the local ROM data", "discover": "Discover game functions",
         "recompile": "Translate ARM code to C", "recompile_2": "Translate with corrected function boundaries",
         "compile": "Compile C to Windows x64 objects", "link": "Link the game module",
         "manifest": "Verify and record artifact checksums"}
BUILD_STAGES = ("extract", "discover", "recompile", "compile", "link", "manifest")
SCAN_PHASES = {
    "candidates": ("Checking candidate functions", "checked"),
    "adjacent": ("Checking for adjacent functions", "functions checked"),
    "pointers": ("Scanning function pointers", "modules scanned"),
    "gaps": ("Scanning for code in gaps", "modules scanned"),
}


class SetupError(Exception):
    pass


def status(message: str) -> None:
    print(message, flush=True)


def local_path(path: Path) -> Path:
    if not path.is_relative_to(ROOT / "build") or path.resolve() != path or path.is_symlink():
        raise SetupError("The build folder must be a regular directory inside this checkout.")
    return path


def managed_contents(folder: Path) -> list[Path]:
    files = list(folder.iterdir())
    for file in files:
        if file.name not in MODULE_FILES or not file.is_file():
            raise SetupError("A cache folder contains files the launcher did not create.")
        local_path(file)
    return files


def generations() -> list[Path]:
    cache = local_path(BUILD / "cache")
    if not cache.exists():
        return []
    return [item for item in cache.iterdir() if GENERATION.fullmatch(item.name) and item.is_dir()]


def cache_info(show_log: bool = False) -> None:
    sizes = []
    for entry in generations():
        module = entry / "st_game.dll"
        try:
            if entry.resolve() == entry and not module.is_symlink() and module.is_file():
                sizes.append(module.stat().st_size)
        except FileNotFoundError:
            continue  # another launcher published a generation during a read-only scan
    status(f"CACHE {len(sizes)} {sum(sizes) / (1024**2):.1f}")
    if show_log:
        runs = local_path(BUILD / "runs")
        logs = (log for log in runs.glob("tmp*/prepare.log") if log.is_file() and log.resolve() == log)
        latest = max(logs, key=lambda log: log.stat().st_mtime_ns, default=None)
        if latest:
            status(f"LOG {latest.parent.name}")


def remove_cache() -> None:
    entries = [(local_path(folder), managed_contents(folder)) for folder in generations()]
    for folder, files in entries:
        for file in files:
            file.unlink()
        folder.rmdir()
    status(f"Removed {len(entries)} cached build(s).")
    cache_info()


def rom_info(rom: Path) -> tuple[str, str]:
    with rom.open("rb") as f:
        digest = hashlib.file_digest(f, "sha1").hexdigest()
    if digest not in st_build.KNOWN:
        raise SetupError("Unsupported ROM. Select Spirit Tracks USA/Australia, revision 0.")
    return digest, st_build.KNOWN[digest][0]


def toolchain(logs: Path) -> tuple[dict[str, str], list[Path]]:
    env = os.environ.copy()
    llvm = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "LLVM/bin"
    env["PATH"] = str(llvm) + os.pathsep + env.get("PATH", "")
    tools = [shutil.which(name, path=env["PATH"]) for name in ("clang-cl.exe", "lld-link.exe")]
    if not all(tools):
        raise SetupError("LLVM tools are missing. See docs/BUILDING.md for the Windows setup.")

    vswhere = (Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) /
               "Microsoft Visual Studio/Installer/vswhere.exe")
    if not vswhere.is_file():
        raise SetupError("Visual Studio C++ Build Tools are missing. See docs/BUILDING.md.")
    result = subprocess.run([vswhere, "-latest", "-products", "*", "-version", "[17.0,18.0)",
                             "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                             "-property", "installationPath", "-utf8"], capture_output=True,
                            encoding="utf-8", env=env, creationflags=subprocess.CREATE_NO_WINDOW)
    vcvars = Path(result.stdout.strip()) / "VC/Auxiliary/Build/vcvars64.bat"
    if result.returncode or not vcvars.is_file():
        raise SetupError("Install the Visual Studio 2022 Desktop development with C++ workload.")
    # cmd only receives the toolchain's installation path; ROM paths go straight to Python and the host.
    cmd = Path(os.environ["SystemRoot"]) / "System32/cmd.exe"
    result = subprocess.run(f'"{cmd}" /d /u /s /c ""{vcvars}" >nul && set"', capture_output=True,
                            encoding="utf-16-le", errors="replace", env=env,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        (logs / "toolchain.log").write_text(result.stderr, encoding="utf-8")
        raise SetupError("Visual Studio setup failed. See toolchain.log in build/launcher/runs.")
    for line in result.stdout.splitlines():
        name, sep, value = line.partition("=")
        if sep and name:
            env[name.upper()] = value
    env["PATH"] = os.pathsep.join([str(Path(tool).parent) for tool in tools] + [env["PATH"]])
    return env, [Path(tool) for tool in tools]


def cache_key(digest: str, code: str, recomp: Path, tools: list[Path], env: dict[str, str]) -> str:
    config = ROOT / "game/st/config" / code.lower()
    inputs = [Path(st_build.__file__), recomp, *tools]
    inputs += sorted(config.glob("*.txt"))
    inputs += [ROOT / "tools/re" / name for name in ("ndsrom.py", "blz.py", "discover.py")]
    inputs += sorted((ENGINE / "runtime/include/ntr").glob("*.h"))
    capstone = Path(importlib.util.find_spec("capstone").origin).parent
    inputs += sorted(p for p in capstone.rglob("*") if p.suffix in (".py", ".dll"))
    fingerprint = [digest, *MODE, sys.version, env.get("INCLUDE", "")]
    fingerprint += [f"{path.name}={st_build.sha256(path)}" for path in inputs]
    return hashlib.sha256("\n".join(fingerprint).encode()).hexdigest()


def cached_module(folder: Path) -> Path | None:
    module, checksum = local_path(folder / "st_game.dll"), local_path(folder / "sha256")
    if not module.is_file() or not checksum.is_file():
        return None
    expected = checksum.read_text(encoding="ascii", errors="replace").strip()
    return module if expected == st_build.sha256(module) else None


def recover_generation(folder: Path) -> Path:
    backup = local_path(folder.with_suffix(".previous"))
    if backup.exists() and not folder.exists():
        managed_contents(backup)
        backup.replace(folder)
    return backup


def publish_generation(ready: Path, folder: Path, backup: Path) -> None:
    # The lock covers both moves. A future invocation can recover an interrupted promotion.
    ready, folder, backup = local_path(ready), local_path(folder), local_path(backup)
    if backup.exists():
        for file in managed_contents(backup):
            file.unlink()
        backup.rmdir()
    if folder.exists():
        managed_contents(folder)
        folder.replace(backup)
    try:
        ready.replace(folder)
    except OSError:
        recover_generation(folder)
        raise
    if backup.exists():
        for file in managed_contents(backup):
            file.unlink()
        backup.rmdir()


def prepare(rom: Path, folder: Path, recomp: Path, env: dict[str, str], logs: Path, force: bool) -> Path:
    local_path(folder)
    backup = recover_generation(folder)
    module = cached_module(folder)
    if module is not None and not force:
        status("Using the cached Windows x64 module. Its SHA-256 matches.")
        return module

    (logs / "prepare.log").write_text(f"Build mode: {' '.join(MODE)}\n", encoding="utf-8")
    status(f"LOG {logs.name}")
    status("DETAIL Rebuilding from the ROM." if force else "DETAIL No compatible module in the cache. Building locally.")
    out = local_path(logs / "module-build")
    ready = local_path(logs / "ready")
    argv = [sys.executable, "-I", "-X", "utf8", "-u", st_build.__file__, "--rom", rom, "--out", out,
            "--recomp", recomp, "--progress", *MODE]
    start = time.monotonic()
    with (logs / "prepare.log").open("a", encoding="utf-8", buffering=1) as log:
        with subprocess.Popen(argv, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, encoding="utf-8", errors="replace",
                              creationflags=subprocess.CREATE_NO_WINDOW) as build:
            for line in build.stdout:
                log.write(line)
                parts = line.split()
                if len(parts) == 2 and parts[0] == "BEGIN" and parts[1] in STEPS:
                    stage = parts[1]
                    label = STEPS[stage]
                    if stage in BUILD_STAGES:
                        status(f"STAGE {BUILD_STAGES.index(stage) + 1} {len(BUILD_STAGES)} {label}")
                    status(f"DETAIL {label}...")
                elif len(parts) == 3 and parts[0] == "END" and parts[1] in STEPS:
                    status(f"DETAIL {STEPS[parts[1]]} completed in {parts[2]} s.")
                elif len(parts) == 5 and parts[0] == "SCAN" and parts[2] in SCAN_PHASES:
                    _, round_number, phase, done, total = parts
                    label, unit = SCAN_PHASES[phase]
                    count = f"{done} / {total}" if phase != "candidates" else done
                    status(f"PROGRESS Pass {round_number} · {label} · {count} {unit}")
                elif len(parts) == 3 and parts[0] == "COMPILE":
                    status(f"PROGRESS Compiled {parts[1]} / {parts[2]} C files.")
            if build.wait() != 0:
                raise SetupError(f"Game preparation failed. See build/launcher/runs/{logs.name}/prepare.log.")

    ready.mkdir()
    module = out / "st_game.dll"
    (ready / "sha256").write_text(st_build.sha256(module), encoding="ascii")
    module.replace(ready / module.name)
    local_path(out / "logs").replace(local_path(logs / "steps"))
    (out / "timings.txt").replace(logs / "timings.txt")
    publish_generation(ready, folder, backup)
    shutil.rmtree(local_path(out))
    status(f"DETAIL Module published to the cache in {time.monotonic() - start:.1f} s.")
    return folder / module.name


def run(args: argparse.Namespace) -> None:
    if args.verify:
        cache_info(show_log=True)
    status("Checking the ROM...")
    digest, code = rom_info(args.rom)
    if args.verify:
        status("ROM verified. Ready to play.")
        return
    native, recomp = BIN / "st_native.exe", BIN / "recompiler/ntr-recomp.exe"
    if not native.is_file() or not recomp.is_file():
        raise SetupError("Build st-native first with tools/build/build_program.cmd.")
    if importlib.util.find_spec("capstone") is None:
        raise SetupError("Capstone is missing. See the launcher prerequisites in docs/BUILDING.md.")

    cache = local_path(BUILD / "cache")
    cache.mkdir(parents=True, exist_ok=True)
    runs = local_path(BUILD / "runs")
    runs.mkdir(exist_ok=True)
    logs = Path(tempfile.mkdtemp(dir=runs))
    try:
        status("Checking the build tools and local cache...")
        env, tools = toolchain(logs)
        key = cache_key(digest, code, recomp, tools, env)
        module = prepare(args.rom, cache / key, recomp, env, logs, args.rebuild)
        cache_info()
        if args.prepare_for_window:
            status(f"MODULE {key}")
            return
        if args.rebuild:
            status("Module is ready. Use Play to start the game.")
            return
        if args.prepare_only:
            status(f"Module ready in build/launcher/cache/{key}/st_game.dll.")
            return
        status("Starting the game...")
        command = [native, args.rom, "--module", module, "--scale", str(args.scale), "--language", args.language,
                   "--player-name", args.player_name, "--volume", str(args.volume),
                   "--audio-interp", args.audio_interp, "--mic-mode", args.mic_mode,
                   "--mic-gain", str(args.mic_gain)]
        if args.fullscreen:
            command.append("--fullscreen")
        if args.mute_unfocused:
            command.append("--mute-unfocused")
        if args.output_device:
            command.extend(["--output-device", args.output_device])
        if args.input_device:
            command.extend(["--input-device", args.input_device])
        with (logs / "game.log").open("wb") as log:
            with subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                  creationflags=subprocess.CREATE_NO_WINDOW) as game:
                status("Game is running. Close the game window to return here.")
                if game.wait():
                    raise SetupError(f"The game stopped with an error. See build/launcher/runs/{logs.name}/game.log.")
        status("Ready to play.")
    except OSError as exc:
        (logs / "error.log").write_text(str(exc), encoding="utf-8")
        raise SetupError(f"A local file or tool could not be opened. See build/launcher/runs/{logs.name}/error.log.") from exc


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path)
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--verify", action="store_true")
    actions.add_argument("--prepare-only", action="store_true", help="prepare and check the cache without playing")
    actions.add_argument("--prepare-for-window", action="store_true", help=argparse.SUPPRESS)
    actions.add_argument("--rebuild", action="store_true", help="build from scratch and replace the cached module")
    actions.add_argument("--cache-info", action="store_true", help="show the launcher's cache footprint")
    actions.add_argument("--remove-cache", action="store_true", help="remove the launcher's cached modules")
    parser.add_argument("--scale", type=int, default=2, choices=(1, 2, 3))
    parser.add_argument("--language", choices=("en", "fr", "es"), default="en")
    parser.add_argument("--fullscreen", action="store_true")
    parser.add_argument("--player-name", default="Link")
    parser.add_argument("--volume", type=int, default=100)
    parser.add_argument("--mute-unfocused", action="store_true")
    parser.add_argument("--audio-interp", choices=("none", "linear", "cosine", "cubic", "gaussian"), default="none")
    parser.add_argument("--output-device", default="")
    parser.add_argument("--mic-mode", choices=("automatic", "real", "virtual", "hybrid", "off"), default="automatic")
    parser.add_argument("--input-device", default="")
    parser.add_argument("--mic-gain", type=int, default=100)
    args = parser.parse_args()
    if args.rom is None and not (args.cache_info or args.remove_cache):
        parser.error("--rom is required for ROM verification and compilation")

    try:
        if args.cache_info:
            cache_info(show_log=True)
        elif args.verify:
            run(args)
        else:
            local_path(BUILD).mkdir(parents=True, exist_ok=True)
            with (BUILD / "session.lock").open("a+b") as lock:
                lock.seek(0)
                try:
                    msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                except OSError as exc:
                    raise SetupError("A launcher session is already preparing or running the game.") from exc
                if args.remove_cache:
                    remove_cache()
                else:
                    run(args)
    except SetupError as exc:
        status(f"ERROR {exc}")
        return 1
    except OSError:
        status("ERROR The selected ROM or the local build folder could not be opened.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
