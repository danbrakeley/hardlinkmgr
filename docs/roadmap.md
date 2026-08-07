# Roadmap <!-- omit in toc -->

- [Overview of Problems](#overview-of-problems)
- [Move OS icon lookups off the GUI thread](#move-os-icon-lookups-off-the-gui-thread)
- [In-app display/browser for log/history](#in-app-displaybrowser-for-loghistory)
- [More general file management](#more-general-file-management)
- [**SHELVED FOR NOW** Tree view inside the table](#shelved-for-now-tree-view-inside-the-table)

## Overview of Problems

Pri: 1 (highest) to 5 (lowest)

| Pri | Name                                       | Notes                                                |
| --- | ------------------------------------------ | ---------------------------------------------------- |
| 1   | Resizable columns in search results        |                                                      |
| 2   | Move OS icon lookups off the GUI thread    | See below — measurably slows Match/Link Finder searches |
| 3   | Add new mode for creating new links        | Maybe in a tab with the Match Finder?                |
| 4   | Starting search result paths               | default to following view?                           |
| 5   | In-app display/browser for log/history     | Should update live as things happen                  |
| 5   | More general file management               | ie rename/delete... need specific use case first     |
| √   | ~~Improve finding matches~~                | **Match Finder shipped as Milestone 7 (2026-07-22)** |
| √   | ~~Testing~~                                | **Unit & Integration tests added 2026-07-28**        |
| √   | ~~Build/release workflows~~                | **Github actions added 2026-07-28**                  |
| √   | ~~App Icon~~                               | **Made an icon (NOT gen AI!) 2026-07-29**            |
| √   | ~~Keep log/history of actions/errors~~     | **Audit log shipped 2026-07-29**                     |
| √   | ~~About dialog~~                           | **Shipped 2026-07-30**                               |
| √   | ~~Detect when there's a newer version~~    | **About Dialog can do a manual check 2026-07-30**    |
| √   | ~~Move counts from toolbar to status bar~~ | **Shipped 2026-08-01**                               |
| √   | ~~Link button is confusing~~               | **Removed 2026-08-04; use Match Finder to link**     |
| √   | ~~More appropriate file icons~~            | **View dropdown (OS/generic) shipped 2026-08-05**    |
| √   | ~~Clean up group borders~~                 | **2026-08-06**                                       |
| √   | ~~Add new "Link Finder"~~                  | **Tab view + inode-grouping search shipped 2026-08-07** |

## Move OS icon lookups off the GUI thread

Discovered while building Link Finder: a running Match/Link Finder search visibly
slows down or speeds up depending on which folder a `FileBrowserView` happens to have
open, and clicking "up" to the parent folder speeds a slow search back up.

Root cause, confirmed by an A/B test (same folder, same search, only the icon mode
toggled): `SmbSession` has no worker thread — it's driven entirely by a `QSocketNotifier`
and a tick `QTimer` on the GUI thread (`SmbSession.cpp`), so `smb2_service()` only runs
when the Qt event loop gets to it. `IconUtil::osIcon()` caches OS icons per file
extension, but on a cache *miss* it calls `QFileIconProvider::icon()` synchronously —
a real OS/shell call (`SHGetFileInfo` on Windows) that can be slow, especially for
extensions with a registered shell icon/thumbnail handler. That call runs inside
`FileListModel::data()`, triggered the moment `QTreeView` paints a row it hasn't drawn
before. A folder with many previously-unseen extensions causes a burst of these
synchronous lookups exactly while it renders, which stalls the event loop and
therefore stalls `smb2_service()` for that whole burst — independent of how many SMB
requests are actually queued. Switching a view to "Use generic icon" (which skips
`osIcon()` entirely) removes the slowdown entirely, confirming the cause.

Decided approach: move the OS icon lookup itself off the GUI thread (e.g. via
`QtConcurrent::run`), with `FileListModel::data()` returning a placeholder icon
immediately on a cache miss and filling in the real one via `dataChanged` once the
background lookup completes — the same "placeholder now, fill in lazily" shape as the
existing Links-column stat pump (milestone 3). Needs care before landing: verify
`QFileIconProvider`/shell icon extraction is actually safe to call off the GUI thread
on Windows (COM/thread-apartment considerations for `SHGetFileInfo`) and confirm
Linux's icon-theme backend behaves correctly too — this isn't guaranteed by the Qt
docs and needs to be checked on both platforms before shipping. (A lower-risk fallback
considered and set aside for now: keep the lookup synchronous but stagger it one
extension at a time via chained zero-delay `QTimer::singleShot` calls, so the event
loop — and therefore `smb2_service()` — gets to run between lookups instead of one
long uninterrupted burst. Worth revisiting if the threaded approach turns out to be
too risky.)

## In-app display/browser for log/history

- add a log viewer inside the app (in the Help menu?)

## More general file management

- Create new hard link to existing file
- Delete file/folder
- Rename file/folder

## **SHELVED FOR NOW** Tree view inside the table

Right now the views of the files in on the SMB share are a list of files/folders in a single folder (nothing from the parent or the child folders are included), viewed in table form.

I want to be able to see the child files/folders in each visible folder (depth of 1 to start, maybe configurable in the future), in a sort of tree view inside the same list.

```text

What we currently have:

+---------------------------------------------------------------------+
| ^ | /path/to/current/folder               | @@ | Aa | Filter    | # |
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
| ^ | /path/to/current/folder               | @@ | Aa | Filter    | # |
+---------------------------------------------------------------------+
|   |+/-| Name                      | Size | Modified | Links | Inode |
| I | + | Folder A                  | #    | {date}   | #     | #     |
| I | + | Folder B                  | #    | {date}   | #     | #     |
| I | + | Folder C                  | #    | {date}   | #     | #     |
|   |   | File D                    | #    | {date}   | #     | #     |
+---------------------------------------------------------------------+

2. Folders can be expanded:

+---------------------------------------------------------------------+
| ^ | /path/to/current/folder               | @@ | Aa | Filter    | # |
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
