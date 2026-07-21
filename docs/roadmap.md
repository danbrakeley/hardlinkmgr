# Roadmap <!-- omit in toc -->

- [Overview of Problems](#overview-of-problems)
- [Improve finding matches](#improve-finding-matches)
  - [Generate a list of potential matches: the Search and Search Results UI](#generate-a-list-of-potential-matches-the-search-and-search-results-ui)
  - [**SHELVED FOR NOW** Tree view inside the table](#shelved-for-now-tree-view-inside-the-table)
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

- Generate a list of potential matches, then manually go through list, accepting or rejecting them.
  - Do a first pass to generate a list of potential matches
    - List shows up independent of file view(s)
  - Selecting a row in that list finds the original files in file views (adds views as needed)

- Completely automated
  - REJECTED: doesn't solve original problem, in the same way jdupes doesn't.

### Generate a list of potential matches: the Search and Search Results UI

The main window currently has 1 or more views, stacked vertically on top of each other.

I want to change this to 2 or more views, and all the views are on the left side of a new splitter, and on the right side becomes an area for configuring, running, anb browsing the results of a recursive search for potential matches that we might want to link.

In the following diagram:

- `[ ]` is a checkbox with two states: "checked" aka "on", and "unchecked" aka "off")
- `[ path/to/folder ]` is a text input that accepts a path; starts empty
- `[ text string ]` is a text input that accepts any string (defaults to empty)
- `[ number ]↕` is a numerical input box with up/down arrows on the right, and accepts only zero and positive integers (defaults to "0")
- `<name match type>` is a dropdown that can be "Contains Literal", "Contains Wildcard (*)", and "Regular Expression" (defaults to "Contains Literal")
- `<byte units>` is a dropdown that can be "bytes", "KiB", "MiB", or "GiB" (defaults to "bytes")
- `<time units>` is a dropdown that can be "seconds", "minutes", "hours", "days"

```text
+-------------------------------------------------------------------+
| Match Finder Options:                                             |
|                                                                   |
|     Primary Path:   [ path/to/folder ]  [x] Sync  [x] Subfolders  |  <-- if Sync is checked, then the path in the text box follows the path the first view (on left of main window); typing in the text box unchecks Sync
|     Secondary Path: [ path/to/folder ]  [x] Sync  [x] Subfolders  |  <-- if Sync is checked, then the path in the text box follows the path the first view (on left of main window); typing in the text box unchecks Sync
|                                                                   |
| [ ] File Name: [ text string ] <name  match type>                 |  <-- this "[ ]" starts unchecked
| [x] Size Difference: [ number ]↕ <byte units>                     |  <-- this "[ ]" starts CHECKED
| [ ] Date Difference: [ number ]↕ <time units>                     |  <-- this "[ ]" starts unchecked
| [x] Hard links: Min [ number ]↕, Max [ number ]↕                  |  <-- this "[ ]" starts CHECKED, and Min defaults to 0, and Max defaults to 1
|                                                                   |
| [ Reset All ]                                  [Start Search]     |  <-- "Reset All" returns all the above options back to their default states, and is grayed out if they are already in the default state; ""Start Search" button changes to "Cancel Search" while a search is running
+-------------------------------------------------------------------|
| Match Finder Results                                              |
|                                                                   |
| [ ] | Primary Name    | Secondary Name        | Other Names?      |  <-- this is the header row, and the "[ ]" will check/uncheck all items in the list
| [ ] | File AB.iso     | Also File AB.iso      |                   |  <-- this row begins the results, one result per row
| [ ] | File 321.iso    | Also File 321.iso     | File 32b.iso, ... |
|                                                                   |
|                                       [ Link Selected Matches ]   |
+-------------------------------------------------------------------|
```

- All the "Match Finder Options" fields should be persisted across runs of the application.
  - Paths should be validated upon connection to the server, and cleared if not valid on the current server.

- The table in "Match Finder Results" starts empty, and populates when a search completes.
- Searching takes into account the options at the top while looking for potential matches.

- Once there are search results:
  - Selecting a table row should cause the relevant Views to navigate to the appropriate folder and select/show the appropriate file.
  - Each result row has a checkbox in the first column that starts unchecked.
  - "Link Selected Matches" is only enabled when 1 or more rows are checked.
  - When "Link Selected Matches" is clicked:
    - All selected/checked rows should be linked.
    - TODO: can we reuse existing code paths around linking files?

### **SHELVED FOR NOW** Tree view inside the table

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
