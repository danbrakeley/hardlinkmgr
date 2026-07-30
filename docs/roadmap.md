# Roadmap <!-- omit in toc -->

- [Overview of Problems](#overview-of-problems)
- [About dialog](#about-dialog)
- [Keep log/history of actions/errors](#keep-loghistory-of-actionserrors)
- [In-app display/browser for log/history](#in-app-displaybrowser-for-loghistory)
- [More general file management](#more-general-file-management)
- [Move counts from toolbar to status bar](#move-counts-from-toolbar-to-status-bar)
- [**SHELVED FOR NOW** Tree view inside the table](#shelved-for-now-tree-view-inside-the-table)

## Overview of Problems

Pri: 1 (highest) to 5 (lowest)

| Pri | Name                                   | Notes                                                 |
| --- | -------------------------------------- | ----------------------------------------------------- |
| 3   | Starting search result paths           | default to following view?                            |
| 4   | Move counts from toolbar to status bar | UX cleanup, but would free up toolbar space if needed |
| 5   | Resizable columns in search results    |                                                       |
| 5   | Link button is confusing               | Move it? Rename it? Remove it?                        |
| 5   | In-app display/browser for log/history | Should update live as things happen                   |
| 5   | More general file management           | ie rename/delete... need specific use case first      |
| 5   | In-app icons                           | Icons have been improved, but do another pass         |
| 5   | More appropriate file icons            | How much overhead does it add to use the OS icons?    |
| √   | ~~Improve finding matches~~            | **Match Finder shipped as Milestone 7 (2026-07-22)**  |
| √   | ~~Testing~~                            | **Unit & Integration tests added 2026-07-28**         |
| √   | ~~Build/release workflows~~            | **Github actions added 2026-07-28**                   |
| √   | ~~App Icon~~                           | **Made an icon (NOT gen AI!) 2026-07-29**             |
| √   | ~~Keep log/history of actions/errors~~ | **Audit log shipped 2026-07-29**                      |
| √   | ~~About dialog~~                       | **Shipped 2026-07-30**                                |

## About dialog

**Implemented 2026-07-30** (`src/ui/AboutDialog.{h,cpp}`, opened from the toolbar's About action in `MainWindow`). The spec below is kept as the reference for what it shows.

- add a button to the far right of the main window toolbar that opens the About dialog
- The About dialog should show:
  - on the left
    - the full sized (256x256) app icon
  - on the right
    - the full name: "Hard Link Manager"
    - the version: e.g. "v0.1.2"
  - under the app icon and name
    - a copyright notice: "(C) Copyright 2026 Dan Brakeley"
    - a link to the github page `github.com/danbrakeley/hardlinkmgr`
    - an OK button that closes the dialog (the dialog also has a window title bar with an X that does the same thing)

## Keep log/history of actions/errors

**Implemented 2026-07-29** (`src/core/LogFormat.h` + `src/core/Logger`, written from `SmbSession`; the file lands in the app-data directory as `log.jsonl`). The spec below is kept as the format reference for the in-app viewer item.

- the purpose is to provide an audit trail of what the app did, so any action that results in a change to the files/folders on the server must be logged!
  - additionally, it is useful to log when a connect/disconnect happens, and when any network errors occur.
- each line contains a single valid json object (jsonl format)
  - the log file should use the `.jsonl` extension
- each line MUST include:
  - `level`: `error`, `info` (NO warnings, something is either expected (info) or unexpected (error)).
  - `time`: in the RFC 3339/ISO 8601 format, i.e. "2026-07-29T17:22:18.808Z"
  - `msg`: the generic description of why this log line exists
- each line MAY include:
  - message specific fields, so that logs are structured, and avoid parsing complex `msg` fields to read useful data
- what to log
  - connecting to a server (include url in "smb://..." form, obviously do NOT include passwords)
  - disconnecting from a server (include url)
  - errors during connection or communication with a server (include url)
  - any action that results in a write on the server (rename, delete, create hard link, etc)

## In-app display/browser for log/history

- add a log viewer inside the app (in the Help menu?)

## More general file management

- Create new hard link to existing file
- Delete file/folder
- Rename file/folder

## Move counts from toolbar to status bar

- Move Total/Filtered counts from toolbar to a new status bar
- Use columns in the status bar
- Add count of selected items in this view

```text

+---------------------------------------------------------------------+
| ^ | /path/to/current/folder           | @@ | Aa | Filter        | X |
+---------------------------------------------------------------------+
|   | Name                          | Size | Modified | Links | Inode |
| I | Folder A                      | #    | {date}   | #     | #     |
| I | Folder B                      | #    | {date}   | #     | #     |
| I | Folder C                      | #    | {date}   | #     | #     |
|   | File D                        | #    | {date}   | #     | #     |
+---------------------------------------------------------------------+
|                               | Selected: 0 | Visible: 4 | Total: 4 |   <-- new status bar
+---------------------------------------------------------------------+

```

## **SHELVED FOR NOW** Tree view inside the table

Right now the views of the files in on the SMB share are a list of files/folders in a single folder (nothing from the parent or the child folders are included), viewed in table form.

I want to be able to see the child files/folders in each visible folder (depth of 1 to start, maybe configurable in the future), in a sort of tree view inside the same list.

```text

What we currently have:

+---------------------------------------------------------------------+
| ^ | /path/to/current/folder           | @@ | Aa | Filter    | # | X |
+---------------------------------------------------------------------+
|   | Name                          | Size | Modified | Links | Inode |
| I | Folder A                      | #    | {date}   | #     | #     |
| I | Folder B                      | #    | {date}   | #     | #     |
| I | Folder C                      | #    | {date}   | #     | #     |
|   | File D                        | #    | {date}   | #     | #     |
+---------------------------------------------------------------------+

What I'm thinking:

1. Folders all start collapsed:

+---------------------------------------------------------------------+
| ^ | /path/to/current/folder           | @@ | Aa | Filter    | # | X |
+---------------------------------------------------------------------+
|   |+/-| Name                      | Size | Modified | Links | Inode |
| I | + | Folder A                  | #    | {date}   | #     | #     |
| I | + | Folder B                  | #    | {date}   | #     | #     |
| I | + | Folder C                  | #    | {date}   | #     | #     |
|   |   | File D                    | #    | {date}   | #     | #     |
+---------------------------------------------------------------------+

2. Folders can be expanded:

+---------------------------------------------------------------------+
| ^ | /path/to/current/folder           | @@ | Aa | Filter    | # | X |
+---------------------------------------------------------------------+
|   |+/-| Name                      | Size | Modified | Links | Inode |
| I | - | Folder A                  | #    | {date}   | #     | #     |
|   |   | ↳ File AA                 | #    | {date}   | #     | #     |
|   |   | ↳ File AB                 | #    | {date}   | #     | #     |
| I | - | Folder B                  | #    | {date}   | #     | #     |
|   | + | ↳ Folder BA               | #    | {date}   | #     | #     |
|   |   | ↳ File BB                 | #    | {date}   | #     | #     |
| I | - | Folder C                  | #    | {date}   | #     | #     |
| I |   | ↳ Folder CA               | #    | {date}   | #     | #     |
|   |   | ↳ File CB                 | #    | {date}   | #     | #     |
|   |   | File D                    | #    | {date}   | #     | #     |
+---------------------------------------------------------------------+

```

The new column between the Icon and Name columns will have "+" buttons to allow the user to expand that folder, and "-" buttons on expanded folders to collapse.

File/folder data will need to be retrieved, ideally asynchronously, so some kind of placeholder or spinner will be needed to be in place while that work is happening. And changing the top level path of the view should cancel all outstanding async work.
