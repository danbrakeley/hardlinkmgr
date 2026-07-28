---
date: 2026-07-23
consulted: Claude (design and implementation of milestone 7)
---

# Find Duplicate Candidates with a Metadata-Only Match Finder

## Context and Problem Statement

The app's core workflow — pairing an original in one folder tree with its
near-duplicate in another, then hard-linking them — was entirely manual:
navigate both views, guess a filter string, eyeball sizes. The roadmap's
top-priority problem ("Improve finding matches") is that this is too slow.
Content hashing is off the table by design: the near-duplicates are often
*slightly* different files (including different sizes), which is the very
reason tools like `jdupes` don't work here. How should the app help the user
find candidate pairs faster without taking the match decision away from them?

## Decision Drivers

- **The user decides what matches.** Full automation was already rejected in
  the roadmap: it re-creates the `jdupes` failure mode this app exists to
  avoid.
- **Cheap signals only.** Candidate discovery must be fast over SMB, so it can
  only use what directory enumeration already returns (name, size, mtime,
  inode). SMB2 enumeration never returns link counts, and a per-file stat
  fan-out across two whole trees would be far too slow.
- **Fit the existing architecture:** the single async `SmbSession` on the GUI
  thread, replies correlated by path, no per-operation cancellation, and the
  proven rename → link → unlink replacement sequence.
- **Real trees are messy:** the two search roots may overlap or be identical,
  candidates live several folders deep, and byte sizes of true matches are
  close but not always equal.

## Considered Options

- **Match Finder: a search that generates a reviewable candidate list**
  (chosen)
- **Better manual tooling** (tree view inside the table, synced filters,
  size-range filters)
- **Fully automated linking** (rejected up front)

## Decision Outcome

Chosen option: **Match Finder**, because it removes the slow part (finding
candidates across two trees) while keeping the human decision (which
candidates are real matches) — the mostly-manual improvements still leave the
user doing tree-by-tree legwork, and full automation fails the k.o. driver.
Shipped as milestone 7; UI design in `docs/roadmap.md`, manual checks in
`docs/testing.md` (Milestone 7).

The feature's own design decisions, and why:

- **Enumeration-only search.** The recursive search issues only
  `listDirectory` calls — never `statFile`. Size windows come from enumeration
  sizes; "already hard-linked" pairs are excluded via the inode that
  enumeration *does* return (equal nonzero inodes). Link counts are never
  needed by the algorithm.
- **Pairing = size-sorted window sweep.** All gathered files sorted by size,
  each paired with the files within the configured Size Difference above it.
  Size Min and Size Difference are always applied (a Size Min of 0 means no
  minimum; the difference is always finite, which bounds the work). One result
  row per unordered pair; hard caps on pairs examined and matches emitted
  guard pathological inputs.
- **One traversal, membership derived from paths.** Overlapping or identical
  primary/secondary roots are valid, so each directory is listed at most once
  and its primary/secondary membership is *computed from its path relative to
  the two roots*, not propagated down the traversal (propagation mishandles a
  root discovered late inside the other tree).
- **Cancel = stop scheduling + drop late replies.** `SmbSession` gained no
  per-operation cancel. The searcher bounds listings in flight (the milestone
  3 stat-pump pattern), and Cancel Search clears its pending set so late
  replies miss it and are dropped.
- **Extract the link engine instead of reusing the dialog.** The
  rename → link → unlink state machine moved out of `HardLinkDialog` into
  `src/core/LinkRunner` (per-job signals, no widget coupling). The dialog and
  the panel's "Link Selected Matches" both drive it, so there is exactly one
  code path that mutates files.
- **Results are session-scoped; options are not.** The panel lives in the
  connected central widget and dies on disconnect (results reference
  server-specific paths). The options persist in `QSettings` and saved paths
  are validated on connect — cleared if absent on that server.
