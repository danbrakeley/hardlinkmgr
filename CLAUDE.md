# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project state

All six POC milestones are complete (list in `docs/milestones.md`; future ideas in `docs/roadmap.md`). The repo contains the product vision and build docs (`README.md`), two SMB feasibility spikes under `spikes/`, the decision records under `docs/decisions/`, and a CMake build of the app against Qt Widgets and the patched libsmb2. Milestone 1 (see `docs/milestones.md`) is implemented: `src/smb/SmbSession.{h,cpp}` wraps libsmb2's async API (driven on the GUI thread via QSocketNotifier + a tick timer — never block, and never destroy the context from inside a libsmb2 callback; set the session's teardown-pending flag instead), and `src/ui/MainWindow.{h,cpp}` has the URL box / password prompt / 3-state connect toolbar. Milestone 2 added the file browser: `SmbSession::listDirectory` (async opendir; readdir/closedir are local iteration), `src/models/FileListModel` + `FileFilterProxyModel` (folders-first in both sort directions via a sort() override that records the order), and `src/ui/FileBrowserView` (path box, filter, matches/total label). Milestone 3 added the lazy link-count fill-in: `SmbSession::statFile` (pipelined `smb2_stat_async`) driven by a bounded stat pump in `FileBrowserView` (`kMaxStatsInFlight`, visible rows first, navigation drops the unsent backlog and orphans in-flight replies by clearing the path→row map). Milestone 4 added the link flow: `SmbSession` mutating ops (rename/link/unlink) with request-id completion signals that always fire asynchronously (queued even on immediate failure and on teardown-flush, so a handler never re-enters a context being destroyed), and `src/ui/HardLinkDialog` running the per-victim rename → link → unlink sequence with rename-back on link failure. Milestone 5 added multiple views (vertical `QSplitter`, add/remove views on the fly; Link gathers/dedupes selection across all views and refreshes all views after a run), the remembered last URL (`QSettings`; password never persisted), and the flip to a GUI-subsystem executable (`WIN32_EXECUTABLE ON` — no stdout on Windows anymore). Milestone 6 (Linux build) is done: on Ubuntu 26.04 the `linux-*` presets build with system Qt 6.10 and no source changes; apt packages are documented in README's Linux build section, and the against-the-real-share spot checks live in `docs/testing.md` (Milestone 6). Post-POC, the Match Finder (roadmap's "Generate a list of potential matches") is implemented: `src/core/MatchSearcher` recursively enumerates the primary and secondary paths via concurrent `listDirectory` calls (bounded in flight, replies correlated by path, cancel = stop scheduling and drop late replies; each directory's primary/secondary membership is derived from its path vs the two roots so overlapping/nested roots are listed once) and pairs files by size window, dropping pairs that already share an inode; `src/core/LinkRunner` is the per-victim rename → link → unlink engine extracted from `HardLinkDialog` (which now drives it), reused by `src/ui/MatchFinderPanel` — the right side of a new horizontal splitter in `MainWindow` — whose checkable results (`src/models/MatchResultsModel` + `src/ui/CheckBoxHeader`) feed "Link Selected Matches"; selecting a result row reveals both files in the two views via `FileBrowserView::navigateToAndReveal`. Match Finder options persist under `matchfinder/` QSettings keys, and saved paths are validated on connect (cleared if absent on that server). ADR 0003 then fixed the browser to exactly two views and removed the add/remove-view UI (per-view close button, "Add View" toolbar action): the Match Finder's two-tree model made arbitrary views unused in practice, so `MainWindow` now creates the pair once on connect and `FileBrowserView` has no close affordance. Gotcha discovered in milestone 2: `smb2_destroy_context` flushes pending callbacks with `SMB2_STATUS_SHUTDOWN`, so SmbSession callbacks must check `m_inTeardown` and only release resources on that path.

### Threading escape hatch (decided, deliberately deferred)

`SmbSession` runs libsmb2 on the GUI thread; nothing there blocks (DNS runs on QHostInfo's worker pool — do not reintroduce libsmb2's own blocking resolver). If milestone 2/3 testing shows UI stutter during large directory enumerations or the per-file stat fan-out, the agreed fix is `moveToThread` on the session — its surface is already all signals/slots, so internals and callers stay unchanged. Do **not** switch to libsmb2's sync API to solve a stutter: that forfeits abortability and the pipelined stats milestone 3 depends on.

### Build

Windows (MSVC kit, multi-config):

```powershell
cmake --preset windows          # configure (first time, or after CMakeLists.txt edits)
cmake --build --preset windows-release
# -> build\windows\bin\Release\hardlinkmgr.exe (runs standalone; windeployqt stages Qt DLLs)
```

Use `windows-debug` for a debug build.

Linux (system Qt from apt — package list in README's Linux build section; single-config presets, one build dir each):

```bash
cmake --preset linux-release    # configure (first time, or after CMakeLists.txt edits)
cmake --build --preset linux-release
# -> build/linux-release/bin/hardlinkmgr (runs in place; Qt links dynamically)
```

Use `linux-debug` in place of `linux-release` for a debug build.

A `Makefile` wraps the presets above as short aliases (`make` is installed on both Windows and Linux dev machines here) — `configure`, `release`, `debug`, `test-unit`, `test-all`, `clean` — so the specific `--preset`/`--target` names don't need to be remembered; it branches on `$(OS)` internally to pick the right preset/target names per platform. `configure` is a separate manual step (run after editing `CMakeLists.txt`); the other targets don't invoke it. Not used by the release GitHub Action (`.github/workflows/release.yml`), which calls the CMake presets directly — decided deliberately: CI's steps are already one-line preset invocations, so routing them through the Makefile would add an indirection layer without removing any duplication.

### Tests

Automated suite (architecture in ADR 0004; per-item coverage map in `docs/testing.md`). App code lives in the `hardlinkmgr_core` static library so the `tests/` executables link the same objects; suites are labeled `unit` (serverless) vs `docker` (ctest starts/stops a Samba container from `tests/docker/` on port 10445, share on a named volume, ground truth verified via `docker exec stat`). The `tst_*` executables are `EXCLUDE_FROM_ALL` (`tests/CMakeLists.txt` aggregates them into `add_custom_target(hlm_tests)`, plus an `hlm_tests_unit` subset for just the "unit"-labeled suites, so a plain `cmake --build` only builds the app) — build `tests/hlm_tests_unit` or `tests/hlm_tests` (Windows; the VS generator needs the subdirectory-qualified name for a target outside the top-level build dir) or `hlm_tests_unit`/`hlm_tests` (Linux/Ninja) before running ctest. On Windows all suites share one `windeployqt` run (an `hlm_tests_deploy_qt` target scanning `tst_pathutil`, since every suite links identical Qt modules) rather than one per exe — cheaper, and avoids a file-copy race when building suites with `--parallel`. Run with `ctest --preset windows-unit` (fast, serverless) or `ctest --preset windows-all` (needs Docker); `linux-*` equivalents exist. Test seams: `MainWindow::setPasswordPrompt` (replaces the modal password dialog), `objectName`s on test-relevant widgets (`mw.*`/`fbv.*`/`mfp.*`/`hld.*`), pure logic extracted into `core/MatchPairing.h` + `core/MatchConflicts.h`. Every executable must call `Q_INIT_RESOURCE(icons)` (qrc lives in the static lib). Gotcha found by the suite: completion signals are emitted from inside `smb2_service()`, so `disconnectFromShare()`/`abortConnect()` defer teardown via `m_inService` when called from a handler — don't remove that guard.

## Technology stack (decided — see ADR 0001)

- **Language:** C++.
- **GUI:** **Qt Widgets** (not Qt Quick/QML). Use the model/view framework — it maps almost 1:1 onto this app (see UI notes below). Native look-and-feel is the reason Qt was chosen over Go/Fyne and Rust/egui.
- **SMB:** **libsmb2**, carried as a **patched fork**. Vanilla libsmb2 cannot create hard links; the patch adds `smb2_link`/`smb2_link_async` (an SMB2 `SET_INFO` with `FILE_LINK_INFORMATION`, wire-identical to the rename info it already sends) plus a set-info encoder case and, on MSVC, a `libsmb2.syms` export entry. The exact edits are in `spikes/01_libsmb2/README.md`. This patch must be re-applied/verified whenever libsmb2 is updated.
- **Linking:** dynamic Qt libraries are acceptable (keeps the executable small); Qt LGPLv3 is satisfied by dynamic linking.

### SMB is proven, with one caveat to respect

`spikes/01_libsmb2` confirmed against the real target server: enumerate a directory, read inode + hard-link count, and create a hard link (link count 1 → 2, shared inode). Caveat baked into the design: **SMB2 directory enumeration never returns the hard-link count** — only a per-file stat does — so populating the link-count column requires one stat per entry (fetch lazily / in parallel). Inode (`IndexNumber`/`FileId`) *is* available from enumeration.

## What this app is

A GUI desktop app for manually deduplicating files on an SMB share by replacing near-duplicate files with hard links. The key difference from tools like `jdupes`: matches are **not** decided automatically. The user manually selects which files are "the same," picks a primary to keep, and the app replaces the others with hard links to it. This exists because the target files are often *slightly* different (including different sizes), so content-hashing tools miss them.

## Hard constraints (these drive the tech-stack choice — treat as requirements)

- **Instant startup** — no splash screen or loading bar.
- **Small binary + low memory footprint** — the README's constraints ("lightweight", "low resource usage") rule out Electron / a web rendering stack. Prefer native/lightweight GUI toolkits.
- **Cross-platform, in priority order:** Windows/amd64 and Linux/amd64 are required; macOS/arm is a nice-to-have, low priority.
- **Native look-and-feel** on each platform is a goal.
- Must operate over **SMB** (connect via a `smb://user@host_or_ip:port/share` URL; password prompted at connect time, never persisted) and manipulate **hard links + inode metadata** (hard link count, inode number are shown in the UI and are core to the feature). Proven feasible via the patched libsmb2 (see stack section); re-verify if libsmb2 or the target server changes.

## UI model (as built — the running app is the source of truth; planned changes live in `docs/roadmap.md`)

- **Main window toolbar:** SMB connect/disconnect control (connect → spinner/abort → disconnect states) and a "Link" button (enabled only when ≥2 files are selected across any file list).
- **Exactly two filesystem views** (fixed — see ADR 0003), each with its own path box, case-insensitive plain-text search filter, and a "matches / total" count label. Each view keeps the full file list in memory but only displays entries matching the filter. Folders sort to the top. Columns: icon, name, size, date modified, hard-link count, inode number.
- **Hard Link dialog:** lists the selected files, requires choosing the primary to keep, and on confirm replaces the non-primary files with hard links to the primary.

### How this maps to Qt (implementation guidance)

Lean on the model/view framework rather than hand-rolling. A custom `QAbstractTableModel` holds the full in-memory file list per view; wrap it in a `QSortFilterProxyModel` for the case-insensitive filter (the "matches / total" label is `proxy->rowCount()` vs `source->rowCount()`), folders-to-top and column sorting (override `lessThan`), and render the icon column with a delegate. The "Link enabled when ≥2 files selected across any list" rule is app-level logic over the views' selection models.
