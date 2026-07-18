# Milestones

## Initial POC - Completed 2026-07-17

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
   platform fallout, document the Linux setup in the README's build section.
