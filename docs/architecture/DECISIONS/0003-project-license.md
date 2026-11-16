# ADR-0003: License the project under GPL-3.0-or-later

- Status: **Proposed**
- Date: 2026-11-12

## Context

melonDS is GPL-3.0-or-later and is linked into the program. The engine we import (from FFCCEOT-Recomp and PH Recomp)
is GPL-3.0; FFCC's `docs/LEGAL.md` states GPL-3.0-or-later, while its `LICENSE` is the GPL-3.0 text and the README says
"GPL-3.0" (the "or later" wording must be confirmed with the authors; PH says "GPL-3.0").

## Options

- MIT/BSD/zlib for our code: impossible for the combined program; we could only license *separate* files permissively,
  which adds bookkeeping without benefit while melonDS is linked.
- **GPL-3.0-or-later** for our new files, keeping inherited files under their existing notices.

## Decision

GPL-3.0-or-later for new code. Inherited files keep their copyright and licence notices. If the upstream engine turns
out to be "GPL-3.0-only", the combined work is distributed under GPL-3.0 and our files remain "-or-later".
Third-party components are listed in `NOTICE.md` and `third_party/README.md` with their licences and versions.

## Consequences

- Anyone distributing builds must offer corresponding source; our release process attaches the source tag.
- Code from Shipwright (no licence) and the D-pad patch (no licence) cannot be used. Wind Waker HD Recomp (MPL-2.0) and
  2S2H (CC0)/libultraship (MIT) could be used with attribution, but we currently take only design ideas.
- Contributions are accepted under the project licence with a Developer Certificate of Origin sign-off (see
  `docs/legal/PUBLICATION_AND_BRANDING.md`).
