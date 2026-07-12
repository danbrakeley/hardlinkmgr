# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project state

Pre-implementation, but the stack is decided. The repo contains the product
vision (`README.md`), two SMB feasibility spikes under `spikes/`, and the
decision record `docs/decisions/0001-choose-project-language.md`. The application
itself is not scaffolded yet — there is no app build system, so this file has no
build/test/run commands to give until one exists. Add them here once the CMake
project lands.

## Technology stack (decided — see ADR 0001)

- **Language:** C++.
- **GUI:** **Qt Widgets** (not Qt Quick/QML). Use the model/view framework — it
  maps almost 1:1 onto this app (see UI notes below). Native look-and-feel is the
  reason Qt was chosen over Go/Fyne and Rust/egui.
- **SMB:** **libsmb2**, carried as a **patched fork**. Vanilla libsmb2 cannot
  create hard links; the patch adds `smb2_link`/`smb2_link_async` (an SMB2
  `SET_INFO` with `FILE_LINK_INFORMATION`, wire-identical to the rename info it
  already sends) plus a set-info encoder case and, on MSVC, a `libsmb2.syms`
  export entry. The exact edits are in `spikes/01_libsmb2/README.md`. This patch
  must be re-applied/verified whenever libsmb2 is updated.
- **Linking:** dynamic Qt libraries are acceptable (keeps the executable small);
  Qt LGPLv3 is satisfied by dynamic linking.

### SMB is proven, with one caveat to respect

`spikes/01_libsmb2` confirmed against the real target server: enumerate a
directory, read inode + hard-link count, and create a hard link (link count
1 → 2, shared inode). Caveat baked into the design: **SMB2 directory enumeration
never returns the hard-link count** — only a per-file stat does — so populating
the link-count column requires one stat per entry (fetch lazily / in parallel).
Inode (`IndexNumber`/`FileId`) *is* available from enumeration.

## What this app is

A GUI desktop app for manually deduplicating files on an SMB share by replacing near-duplicate files with hard links. The key difference from tools like `jdupes`: matches are **not** decided automatically. The user manually selects which files are "the same," picks a primary to keep, and the app replaces the others with hard links to it. This exists because the target files are often *slightly* different (including different sizes), so content-hashing tools miss them.

## Hard constraints (these drive the tech-stack choice — treat as requirements)

- **Instant startup** — no splash screen or loading bar.
- **Small binary + low memory footprint** — the README explicitly rules out Electron / a web rendering stack for this reason. Prefer native/lightweight GUI toolkits.
- **Cross-platform, in priority order:** Windows/amd64 and Linux/amd64 are required; macOS/arm is a nice-to-have, low priority.
- **Native look-and-feel** on each platform is a goal.
- Must operate over **SMB** (connect to `host_or_ip:port`) and manipulate **hard links + inode metadata** (hard link count, inode number are shown in the UI and are core to the feature). Proven feasible via the patched libsmb2 (see stack section); re-verify if libsmb2 or the target server changes.

## UI model (from the README — the intended structure)

- **Main window toolbar:** SMB connect/disconnect control (connect → spinner/abort → disconnect states) and a "Link" button (enabled only when ≥2 files are selected across any file list).
- **One or more filesystem views**, each with its own path box, case-insensitive plain-text search filter, and a "matches / total" count label. Each view keeps the full file list in memory but only displays entries matching the filter. Folders sort to the top. Columns: icon, name, size, date modified, hard-link count, inode number.
- **Hard Link dialog:** lists the selected files, requires choosing the primary to keep, and on confirm replaces the non-primary files with hard links to the primary.

### How this maps to Qt (implementation guidance)

Lean on the model/view framework rather than hand-rolling. A custom
`QAbstractTableModel` holds the full in-memory file list per view; wrap it in a
`QSortFilterProxyModel` for the case-insensitive filter (the "matches / total"
label is `proxy->rowCount()` vs `source->rowCount()`), folders-to-top and column
sorting (override `lessThan`), and render the icon column with a delegate. The
"Link enabled when ≥2 files selected across any list" rule is app-level logic
over the views' selection models.
