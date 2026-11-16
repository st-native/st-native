# Publishing on GitLab.com

Status (2026-11-14): **nothing is published; no account exists.** The bundle is prepared locally for the
maintainer's review. Agents never create the account, the project, a push or a release (AGENTS.md).

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

## 3. Steps for the maintainer (after reviewing)

1. **Account.** Create a GitLab.com account dedicated to the project. Pending: e-mail provider, address, username.
   Turn on two-factor authentication. Use the address only for the project (and as the rights-holder contact,
   PUBLICATION_AND_BRANDING.md §3).
2. **Project.** Create it **private** first. Licence: GPL-3.0-or-later (the bundle's `LICENSE`). Shared runners on.
3. **First push**, from the bundle folder (commands to run yourself; the guard runs again before the commit):

   ```bash
   cd build\publish\st-native
   ```
   ```bash
   git init -b main
   ```
   ```bash
   git add -A
   ```
   ```bash
   python -I tools\check_no_game_data.py --staged
   ```
   ```bash
   git commit -m "Spirit Tracks Native: static recompilation port (initial publication)"
   ```
   ```bash
   git remote add origin https://gitlab.com/<username>/<project>.git
   ```
   ```bash
   git push -u origin main
   ```

4. **CI.** Check the first pipeline (`.gitlab-ci.yml`: guard, unit tests, Linux x64 and arm64 builds with the
   synthetic cartridge). It has not run yet anywhere; fix what fails before going public.
5. **Public.** Only after the checklist in §5 is complete.

## 4. Decisions to make before publishing

| Decision | Options | Recommendation |
|---|---|---|
| Account e-mail provider, address, username | pending | a provider that allows a project-only mailbox; the username can wait for the project name |
| Project name | "Spirit Tracks Native" is a working name (PUBLICATION_AND_BRANDING.md §1) | decide before going public |
| How the engine is included | (a) fetched at its pinned commit and patched at build time (the bundle today); (b) imported into `engine/` with its history (REPOSITORY_PLAN.md) | (a) for the first push: smaller, provenance obvious; (b) when we start changing the engine beyond patches |
| The anti-piracy routines | the default build replaces their six entry points (as the engine does for FFCC), **but then the game removes the train's controls on the first train ride and cannot go on**; running the game's own checks (`--dsprot original`) plays through the train ride to Castle Town, saves and reloads (DSPROTECT_ORIGINAL_PATH.md §5) | ask counsel (Q4, Q14) first; only the original-checks path gives a playable game, so it is the candidate default |
| The function list | dsd 0.12.1 (its extractor decrypts the anti-piracy routines on the player's machine) or our discovery (decrypts nothing; boots to the new game, with some functions on the interpreter) | make `--functions ours` the default once its interpreter share is closed, before a public release |
| Public legal documents | none (bundle today), a short LEGAL.md, or the full analysis | a short LEGAL.md written with counsel |
| Superseded ADR-0001 in the public docs | keep (decision record) or leave out | keep: it explains why the project changed |

## 5. Before the project goes public

From PUBLICATION_AND_BRANDING.md §6, for GitLab:

- [ ] Name and branding decided; original icon; disclaimer in README and NOTICE.
- [ ] Licence and attribution final (`LICENSE`, `NOTICE.md`; the upstreams' "or later" question).
- [ ] Guard clean on every commit in the history; no large or unexplained binaries; manual look at every image.
- [ ] Fresh clone builds without a ROM (CI green on Linux; Windows checked locally).
- [ ] Release artifacts (when there are any) contain no game module, generated C, ROM or saves.
- [ ] Counsel consulted on the open questions (reports/: 15 questions; Q1, Q2, Q4, Q14 at least).
- [ ] Rights-holder contact live; takedown procedure written (PUBLICATION_AND_BRANDING.md §3).
- [ ] Upstream authors credited; engine patches offered upstream.
