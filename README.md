# Hard Link Manager

## Problem

I have a single SMB file share that needs a bunch of large files to be in multiple sub folders at the same time, each with a different name. I want to use hard links to avoid actually storing the files multiple times.

The share as a lot of existing files that are already copied in multiple places, and in some cases the files are slightly different (including slightly different sizes), so just running a program like jdupes won't catch everything. Instead, I need to be able to manually identify which files should be the same, and then choose which one to keep, and which one to replace with a hard link.

## Proposed Solution

- GUI application
- App starts instantly (no splash screen or loading bars)
- Keep executable size low and memory footprint while running low (avoid a web stack just to render, aka avoid electron)
- Cross platform (Windows/amd64 and Linux/amd64 required; macOS/arm nice-to-have, but low priority)
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

## TODO

- Decide on language and libs/packages.
