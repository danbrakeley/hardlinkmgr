# Roadmap (or just a list of ideas to explore)

## Tree view inside the table

When viewing a folder that contains a long list of folders, but each folder only contains 2 files, it would be great to expand all the folders in the same table.

## Copy filters across views

- Button to quickly copy the filter from the view above it
- Toggle to keep filters in sync with the view above it

## Filter by size of file selected in previous view

- Ensure what you see in view 2 is within X bytes of what is selected in view 1.
  - If multiple things are selected in view 1, create two byte ranges centered on each selected file.

## View Toolbar improvements

- Better "parent folder" icon
  - The up caret is hard to even see, let alone know it does at a glance.
- Better toolbar grouping/layout
  - Filter buttons should be grouped more obviously
- Toggle button "on" state should be more obvious

## Main Window Toolbar improvements

- connect/disconnect doesn't need text
- connect/disconnect should be to left of connection string
- "Link" button is confusing: "Link Selected Files"? <-- too verbose?

## Log all actions

- keep a log file
- add a log viewer inside the app (in the Help menu?)

## Other useful actions

- Delete file/folder?
- Rename file/folder?

## Menus?

- File
  - Connect <-- grayed out if connected
  - Disconnect <-- grayed out if not connected
  - ---
  - Exit
- Edit
  - Link Selected Files
  - Unlink Selected Files <-- grayed out unless selection is >2 files with identical inode)
- View
  - Expand all folders in selected view
  - Collapse all folders in selected view
- Help
  - View Logs
  - ---
  - See project in GitHub
  - ---
  - About

## App icon

Need one
