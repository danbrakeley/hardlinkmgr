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
    - Text box where you enter the string of the SMB share to connect to, in the format `host_or_ip:port`.
    - Button for connecting/disconnecting to given share
      - when connected, it shows a disconnect icon, and clicking it disconnects
      - when disconnected, it shows a connect icon, and clicking it attempts to connect
      - during a connection attempt, the icon changes to a spinner, and clicking the button aborts the connection attempt and returns to the disconnected state
    - Button "Link" for creating hard links; only active when at least two files are selected in any of the file lists (see below). Triggers the "Hard Link" dialog (see below).
  - The main view under the toolbar shows one or more views until the SMB share's filesystem
    - Each filesystem view includes:
      - Toolbar
        - Text box with current path (doesn't include `host_or_ip:port`, only shows `/absolute/path/for/this/view`.
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

## Build

Windows, using the MSVC Qt kit (see `docs/decisions/0001-choose-project-language.md`).
libsmb2 is pulled from the patched fork via CMake `FetchContent`, so no manual
checkout is needed. Qt is found from the installed kit via the `windows` preset.

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
