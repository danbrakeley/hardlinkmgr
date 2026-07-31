# Roadmap <!-- omit in toc -->

- [Overview of Problems](#overview-of-problems)
- [In-app display/browser for log/history](#in-app-displaybrowser-for-loghistory)
- [More general file management](#more-general-file-management)
- [Move counts from toolbar to status bar](#move-counts-from-toolbar-to-status-bar)
- [**SHELVED FOR NOW** Tree view inside the table](#shelved-for-now-tree-view-inside-the-table)

## Overview of Problems

Pri: 1 (highest) to 5 (lowest)

| Pri | Name                                    | Notes                                                 |
| --- | --------------------------------------- | ----------------------------------------------------- |
| 2   | Move counts from toolbar to status bar  | UX cleanup, but would free up toolbar space if needed |
| 3   | More appropriate file icons             | How much overhead does it add to use the OS icons?    |
| 3   | Add new mode for finding links          | In a tab with the Match Finder?                       |
| 3   | Add new mode for creating new links     | Maybe in a tab with the Match Finder?                 |
| 4   | Starting search result paths            | default to following view?                            |
| 5   | Resizable columns in search results     |                                                       |
| 5   | Link button is confusing                | Move it? Rename it? Remove it?                        |
| 5   | In-app display/browser for log/history  | Should update live as things happen                   |
| 5   | More general file management            | ie rename/delete... need specific use case first      |
| 5   | In-app icons                            | Icons have been improved, but do another pass         |
| √   | ~~Improve finding matches~~             | **Match Finder shipped as Milestone 7 (2026-07-22)**  |
| √   | ~~Testing~~                             | **Unit & Integration tests added 2026-07-28**         |
| √   | ~~Build/release workflows~~             | **Github actions added 2026-07-28**                   |
| √   | ~~App Icon~~                            | **Made an icon (NOT gen AI!) 2026-07-29**             |
| √   | ~~Keep log/history of actions/errors~~  | **Audit log shipped 2026-07-29**                      |
| √   | ~~About dialog~~                        | **Shipped 2026-07-30**                                |
| √   | ~~Detect when there's a newer version~~ | **About Dialog can do a manual check 2026-07-30**     |

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
| ^ | /path/to/current/folder              | @@ | Aa | Filter         |
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
