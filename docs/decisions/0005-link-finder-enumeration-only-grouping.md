---
date: 2026-08-07
consulted: Claude (design and implementation of the Link Finder tab)
---

# Link Finder Groups by Inode from Enumeration Data Only

## Context and Problem Statement

Link Finder (`docs/roadmap.md`, "Add new Link Finder") recursively searches a single
folder and groups files that already share an inode, showing a Links count per row.
SMB2 directory enumeration never returns a file's hard-link count — only a per-file
`stat` does (the same caveat milestone 3's lazy Links column in `FileBrowserView` was
built around). Enumeration *does* return each file's inode. Should Link Finder's Links
count and its Link Min filter be computed from enumeration data alone, or should it
call `SmbSession::statFile` to get each file's authoritative total link count?

## Decision Drivers

- **Fit the existing architecture.** `MatchSearcher` (ADR 0002) already commits to an
  enumeration-only search — never `statFile` — specifically to keep candidate
  discovery fast over SMB. Link Finder's traversal is the same shape.
- **The authoritative-count route isn't bounded by "candidates already found."** A
  file's hard-link siblings can live entirely outside the searched folder, with
  nothing in the enumeration data hinting at that. Getting the true count for every
  row would mean statting *every file the traversal enumerates*, not just the ones
  that already look interesting — one network round trip per file, as a batch the
  user is sitting and waiting on.
- **That cost model already exists, but only as a lazy background pump.**
  `FileBrowserView`'s Links column fills in via a bounded, incremental `statFile` pump
  that runs while the user browses — never as a single blocking batch. A Link Finder
  search calling `statFile` for a whole tree at once would be a very different (much
  worse) cost profile than that established lazy pattern.
- **`SmbSession` has no worker thread.** It's driven entirely by a `QSocketNotifier`
  and a tick `QTimer` on the GUI thread, so every additional in-flight request is
  direct competition with the search's own listings for the same connection and the
  same event-loop time.

## Considered Options

- **Enumeration-only: bucket by inode from directory listings already gathered**
  (chosen)
- **Authoritative nlink via `statFile` for every enumerated file**

## Decision Outcome

Chosen option: **enumeration-only grouping**, because it costs nothing beyond the
listings the traversal already makes (inode is already in the enumeration payload),
keeps Link Finder's performance model consistent with Match Finder's, and avoids
turning a fast recursive search into an O(files) batch of stat round trips. The
tradeoff, accepted explicitly: the Links count shown is **the number of files sharing
that inode found under the searched path** — a lower bound on the file's true
link count if any of its hard links live outside the search root. `core/LinkGrouping.h`
buckets `linkgrouping::FileRecord`s by inode (skipping `inode == 0`, which means
"unknown," never "shared"), keeps buckets whose size is at least Link Min, and emits
one `Entry` per file with `linkCount` set to the bucket size — no per-file stat calls
anywhere in the path.

Also decided alongside this: the inode-bucketing step is a **computation**, not a
display order. It exists to produce the per-row Links count and to apply the Link Min
filter — it does not imply or enforce that same-inode rows stay adjacent in the
results table, which (unlike Match Finder's fixed-order results) is sortable by any
column.

## More Information

- Revisit if: a future workflow needs the authoritative total link count (e.g., an
  explicit per-row "verify" action that stats just that one file/group on demand would
  fit the lazy-pump pattern much better than statting the whole tree upfront).
- Related, deliberately deferred to a future pass: OS icon lookups in `FileListModel`
  currently run synchronously on the same GUI thread for the same reason discussed
  above (see `docs/roadmap.md`, "Move OS icon lookups off the GUI thread") — a
  different feature hitting the same "GUI thread is the bottleneck, not the network"
  constraint that shaped this decision.
