# Hard Link Manager

## Problem

I have a single SMB file share that needs a bunch of large files to be in multiple sub folders at the same time, each with a different name. I want to use hard links to avoid actually storing the files multiple times.

The share as a lot of existing files that are already copied in multiple places, and in some cases the files are slightly different (including slightly different sizes), so just running a program like jdupes won't catch everything. Instead, I need to be able to manually identify which files should be the same, and then choose which one to keep, and which one to replace with a hard link.

## Proposed Solution

- GUI application
- App starts instantly (no splash screen or loading bars)
- Keep resource usage low by avoiding a whole web stack (aka no electron)
- Cross platform (Windows & Linux required, macOS is nice-to-have)
- Ideally looks like a native app on each platform

To start, my vision of the App's UI is:

- Application Window
  - Toolbar
    - Text box where you enter the URL of the SMB share to connect to, in the format `smb://user@host_or_ip:port/share` (port is optional).
    - Button for connecting/disconnecting to given share
      - when connected, it shows a disconnect icon, and clicking it disconnects
      - when disconnected, it shows a connect icon, and clicking it pops up a dialog asking for the password (left empty if no password is needed), then attempts to connect
      - during a connection attempt, the icon changes to a spinner, and clicking the button aborts the connection attempt and returns to the disconnected state
    - Button "Link" for creating hard links; only active when at least two files are selected in any of the file lists (see below). Triggers the "Hard Link" dialog (see below).
  - The main view under the toolbar shows one or more views until the SMB share's filesystem
    - Each filesystem view includes:
      - Toolbar
        - Icon button that navigates up to the immediate parent folder (disabled at the share root)
        - Text box with current path (doesn't include the `smb://...` URL, only shows `/absolute/path/for/this/view`). Each view starts at `/`, the root of the connected share.
        - Icon toggle button (with explanatory tooltip): folders always sorted to the top (default) vs folders sorted amongst the files
        - Icon toggle button (with explanatory tooltip): name sorting case insensitive (default) vs case sensitive
        - Text box with search filter (just simple plain text match, case insensitive)
        - Text label that shows total files, and if there's a filter, number of files that match the filter, in the form "filter_matches / total_files".
      - File list
        - Keeps full file/folder list in memory, but only shows entries that match the search filter in the
        - Folders sorted to the top
        - Columns:
          - icon: Folder icon for folders, file icon for files (keep it simple for now)
          - name
          - size
          - date modified
          - hard Link count
          - inode number
  - Hard Link dialog
    - Triggered by the main window toolbar Link button
    - Shows a list of all the selected files
    - Requires the user to choose the primary file that will be kept
    - At the bottom there's a "Hard Link" button that replaces the non-primary files with a hard link to the primary file.

## Milestones

Each milestone is independently verifiable against the real share.

1. **SMB session + connect toolbar.** An `SmbSession` wrapper around the patched
   libsmb2 (async API, driven from the Qt event loop) with connect/abort/disconnect,
   wired to the toolbar's URL box, password dialog, and 3-state connect button.
   *Verify: connect to the real server, abort mid-attempt, disconnect.*
2. **Single file browser view.** Directory enumeration into a
   `QAbstractTableModel` + `QSortFilterProxyModel`, with the search filter, live
   "matches / total" count, folders-first sorting, and navigation (double-click
   folders, editable path box). The hard-link-count column shows a placeholder,
   since SMB2 enumeration doesn't return link counts.
   *Verify: browse the real share, filter, sort.*
3. **Lazy hard-link-count fill-in.** On directory load, queue a per-file async
   stat (bounded in-flight window, visible rows first) to populate the link-count
   column; cancel stale requests on navigation.
   *Verify: link counts populate correctly against known hard-linked files.*
4. **Cross-view selection + Hard Link dialog + link execution.** Enable the Link
   button when ≥2 files are selected across all views; the dialog requires
   choosing a primary, then per victim: rename to a temp name, hard-link the
   primary into the victim's path, delete the temp (rename back on failure, so
   the victim is never lost until the link exists), then refresh affected views.
   *Verify: end-to-end on throwaway files — link count 1 → 2, shared inode,
   victim content replaced.*
5. **Multiple views + polish.** Add/remove views on a splitter, error surfacing,
   remember the last server URL (`QSettings`, password never persisted), flip to
   a GUI (non-console) executable.
6. **Linux build.** Exercise the existing `linux-*` CMake presets, fix any
   platform fallout, document the Linux setup below.

## Build

On every platform, libsmb2 is pulled from the patched fork via CMake
`FetchContent`, so no manual checkout is needed.

### Windows

Uses the MSVC Qt kit (see `docs/decisions/0001-choose-project-language.md`).
Qt is found from the installed kit via the `windows` preset.

First-time build (or any time after editing `CMakeLists.txt`), configure then build:

```powershell
cmake --preset windows
cmake --build --preset windows-release
# -> build\windows\bin\Release\hardlinkmgr.exe
```

For day-to-day builds where only source has changed, just rebuild:

```powershell
cmake --build --preset windows-release
```

Use `windows-debug` in place of `windows-release` for a debug build.

The exe runs standalone: a post-build `windeployqt` step stages Qt's runtime DLLs
and plugins next to it (it re-runs each build but no-ops when they're current), so
no need to have Qt on `PATH`.

### Linux

Tested on Ubuntu 26.04. One-time setup on a fresh system (on top of `git`,
`curl`, and `build-essential`):

```bash
sudo apt install cmake ninja-build qt6-base-dev qt6-wayland libgl1-mesa-dev
```

- `cmake` + `ninja-build` — the `linux-*` presets use the Ninja generator.
- `qt6-base-dev` — Qt Widgets/Network development files (Qt 6.10 on 26.04).
- `qt6-wayland` — Qt's Wayland platform plugin, so the app runs natively on
  Ubuntu's default Wayland session.
- `libgl1-mesa-dev` — OpenGL headers, required when linking against Qt6::Gui.

Configure then build (reconfigure only after `CMakeLists.txt` edits):

```bash
cmake --preset linux-release
cmake --build --preset linux-release
# -> build/linux-release/bin/hardlinkmgr
```

Use `linux-debug` in place of `linux-release` for a debug build; unlike the
multi-config Windows preset, each Linux preset is single-config with its own
build directory, so both coexist. Qt links dynamically against the system
packages and the binary runs in place — no deploy step.
