# Publishing on GitHub

Status (2026-11-18): the primary private repository is
[st-native/st-native](https://github.com/st-native/st-native). The existing GitLab history was copied intact;
GitLab is retained as a backup. New development and CI use GitHub. Visibility changes and releases remain
maintainer decisions. Check [Actions](https://github.com/st-native/st-native/actions) for current CI results.

## 1. What is published, and what never is

| Published (the bundle) | Never published |
|---|---|
| The application (`app/`) | The ROM, and anything extracted from it (modules, files, assets) |
| The runtime and the recompiler: the engine fetched at its pinned commit plus our patches (`spike/m1a/fetch_engine.py`, `deps.lock.json`, `patches/`) | Generated C, compiled game modules, saves, screenshots, hash logs (`build/`) |
| Spirit Tracks hooks and configuration (`game/st/hooks/`, `game/st/config/`): our code and facts (addresses, public SDK names) | The reconstructed C++ and everything about it (historical research, AGENTS.md) |
| Build, verification and publishing tools (`tools/`, except the reconstruction harness) | Disassembly, decompiler output, analysis notes quoting game code |
| Synthetic tests: our own DS program (`poc/sample/`), its cartridge and reference hashes (`spike/m1a/fixtures/`), unit tests, input scripts (`tests/`) | Legal research (`docs/legal/`, `reports/`, `research_notes/`) unless the maintainer decides otherwise |
| Documentation (README, BUILDING, ROADMAP, RISKS, architecture, design, M1a/M1b/DS Protect reports) | Credentials, the dedicated account's e-mail address |

The allowlist is in `tools/publish/make_bundle.py`; anything not named there is left out.

## 2. Making the bundle

```bash
.venv\Scripts\python -I tools\publish\make_bundle.py
```

It writes `build/publish/st-native/` (the files), `build/publish/BUNDLE_MANIFEST.txt` (SHA-256 of each) and
`build/publish/REVIEW.md` (the guard's result, what was removed or left out, folder counts). The game-data guard
(`tools/check_no_game_data.py --tree`) must report the bundle clean; it blocks ROMs, game file formats, the dump's
SHA-1, cartridge headers, NitroSDK build tags, generated C, reconstructed source, screenshots and large binaries.

## 3. Updating the repository

1. Prepare and review the allowlisted bundle using section 2. Keep the private working directory separate from
   the publication checkout; never initialize or push the private workspace root.
2. Use a checkout of the existing GitHub repository so every previous commit remains in its history:

   ```bash
   git clone https://github.com/st-native/st-native.git
   ```

3. Copy only the reviewed bundle into that checkout, preserving its `.git` directory. Review the diff, then stage
   it and run the guard **before every commit**:

   ```bash
   git add -A
   python -I tools/check_no_game_data.py --staged
   git diff --cached --check
   git commit -m "Describe the reviewed change"
   git push origin main
   ```

4. GitHub Actions (`.github/workflows/ci.yml`) runs the data guard and Python tests, then Linux x64/arm64 builds,
   the app/runtime C++ tests, fibers, recompiler difftests and our synthetic cartridge. It uses no user ROM.
   Diagnostic artifacts contain test logs, not game modules or assets. Windows remains checked locally; this
   workflow does not claim validation on the remaining target platforms.
5. The retained GitLab remote is named `gitlab` in the maintainer's publication checkout. `origin` points to
   GitHub. The migration does not delete or change the GitLab repository or force-push either host.

## 4. Decisions to make before publishing

| Decision | Options | Recommendation |
|---|---|---|
| Hosting account | GitHub account `st-native` exists | keep credentials and contact configuration outside Git; use the project account for collaboration |
| Project name | "Spirit Tracks Native" is a working name (PUBLICATION_AND_BRANDING.md §1) | decide before going public |
| How the engine is included | (a) fetched at its pinned commit and patched at build time (the bundle today); (b) imported into `engine/` with its history (REPOSITORY_PLAN.md) | (a) for the first push: smaller, provenance obvious; (b) when we start changing the engine beyond patches |
| The anti-piracy routines | the default build replaces their six entry points (as the engine does for FFCC), **but then the game removes the train's controls on the first train ride and cannot go on**; running the game's own checks (`--dsprot original`) plays through the train ride to Castle Town, saves and reloads (DSPROTECT_ORIGINAL_PATH.md §5) | ask counsel (Q4, Q14) first; only the original-checks path gives a playable game, so it is the candidate default |
| The function list | dsd 0.12.1 (its extractor decrypts the anti-piracy routines on the player's machine) or our discovery (decrypts nothing; boots to the new game, with some functions on the interpreter) | make `--functions ours` the default once its interpreter share is closed, before a public release |
| Public legal documents | none (bundle today), a short LEGAL.md, or the full analysis | a short LEGAL.md written with counsel |
| Superseded ADR-0001 in the public docs | keep (decision record) or leave out | keep: it explains why the project changed |

## 5. Before the project goes public

Publication-readiness checklist (historical branding research: PUBLICATION_AND_BRANDING.md §6):

- [ ] Name and branding decided; original icon; disclaimer in README and NOTICE.
- [ ] Licence and attribution final (`LICENSE`, `NOTICE.md`; the upstreams' "or later" question).
- [ ] Guard clean on every commit in the history; no large or unexplained binaries; manual look at every image.
- [ ] Fresh clone builds without a ROM (CI green on Linux; Windows checked locally).
- [ ] Release artifacts (when there are any) contain no game module, generated C, ROM or saves.
- [ ] Counsel consulted on the open questions (reports/: 15 questions; Q1, Q2, Q4, Q14 at least).
- [ ] Rights-holder contact live; takedown procedure written (PUBLICATION_AND_BRANDING.md §3).
- [ ] Upstream authors credited; engine patches offered upstream.
