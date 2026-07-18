# Roadmap <!-- omit in toc -->

- [Overview of Problems](#overview-of-problems)
- [Improve finding matches](#improve-finding-matches)
  - [Tree view inside the table](#tree-view-inside-the-table)
- [Keep log/history of actions/errors](#keep-loghistory-of-actionserrors)
- [Link button is confusing](#link-button-is-confusing)
- [More general file management](#more-general-file-management)
- [Do we need menus? Why?](#do-we-need-menus-why)
- [Move counts from toolbar to status bar](#move-counts-from-toolbar-to-status-bar)

## Overview of Problems

Pri: 1 (highest) to 5 (lowest)

| Pri | Name                                   | Notes                                                        |
| --- | -------------------------------------- | ------------------------------------------------------------ |
| 1   | Improve finding matches                |                                                              |
| 2   | Keep log/history of actions/errors     | Could be useful for debugging                                |
| 3   | Build/release workflows                | right now I just build an EXE by hand, no releases           |
| 4   | Link button is confusing               | is it?                                                       |
| 5   | App Icon                               | Right now we have a generic app icon that looks ... generic. |
| 5   | Do we need menus?                      | Why or why not?                                              |
| 5   | More general file management           | ie rename/delete... need specific use case first             |
| 5   | Move counts from toolbar to status bar | UX cleanup, but would free up toolbar space if needed        |
| 5   | Parent folder icon doesn't read well   | Doesn't read as having to do with folder navigation          |
| 5   | Toolbar toggle buttons don't read well | Toggle-ability and current state not obvious                 |

## Improve finding matches

The process of matching a file in the top view with potential matches in the bottom view needs to be faster.

Currently, my work flow involves:

- knowing that the originals are in Folder A and the potential duplicates are in Folder B
- identifying a potential candidate in Folder A
  - candidate is: has only 1 hard link (indicating it is not already de-duplicated), and a large file whose file extension indicates it is media file
  - candidates are generally not found in Folder A itself, but nested in children of Folder A
- looking for a match in Folder B
  - identify core name from original file and use that as filter
  - duplicates are not in the root of Folder B, but in sub folders of sub folders
  - match is often BUT NOT ALWAYS the exact same size
    - when bytes don't match, they are very close
    - should gather examples and come up with a rule of thumb, maybe a fixed number of bytes? or percentage of total size?

Solution Ideas:

- Still mostly manual:
  - View nested folders in the same view, with same filters ("Tree view inside the table" below)
  - Sync filter strings across views
  - Add filter for exact size, but also size range
    - View 2 could show files within X% of the size of selected file in view 1?

- Generate a list of potential matches, then manually go through list, accepted or rejecting them.
  - Do a first pass to generate a list of potential matches
    - List shows up independent of file view(s)
  - Selecting a row in that list finds the original files in file views (adds views as needed)

- Completely automated
  - REJECTED: doesn't solve original problem, in the same way jdupes doesn't.

### Tree view inside the table

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

## Keep log/history of actions/errors

- keep a log file
- every file action is slogged
- add a log viewer inside the app (in the Help menu?)

## Link button is confusing

- "Link" button is confusing: "Link Selected Files"? <-- too verbose?

## More general file management

- Create new hard link to existing file
- Delete file/folder
- Rename file/folder

## Do we need menus? Why?

- Unclear if we need them. What would they provide that we don't already have?
- What kind of expectations are there on macOS?

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
