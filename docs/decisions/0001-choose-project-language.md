---
date: 2026-07-12
consulted: Claude (SMB feasibility spikes and toolkit research)
---

# Choose C++ / Qt / libsmb2 for the Hard Link Manager

## Context and Problem Statement

The Hard Link Manager is a cross-platform GUI desktop app for manually
deduplicating files on an SMB share by replacing near-duplicate files with hard
links (see the project `README.md`). Before writing application code we needed to
settle the foundational stack: the implementation language, the GUI toolkit, and
the SMB access library. The riskiest unknown was whether *any* library, in *any*
language, could — over SMB2/3 against the real target server — read inode and
hard-link-count metadata and **create hard links**. That question had to be
answered empirically before the language/GUI choice could be made with
confidence.

## Decision Drivers

- **SMB feasibility (k.o. criterion):** must connect over SMB, read inode number
  and hard-link count, and create hard links on the real server. If a stack
  can't do this, nothing else matters.
- **Instant startup**; no splash screen or loading bar.
- **Small binary + low memory footprint** — the README rules out Electron / a web
  rendering stack. Dynamic linking of shared GUI libraries is acceptable.
- **Cross-platform**, priority order: Windows/amd64 and Linux/amd64 required;
  macOS/arm nice-to-have.
- **Native look-and-feel** on each platform is a stated goal.
- **Table-heavy UI:** the app is essentially a multi-pane, sortable, filterable
  file browser; toolkit quality for multi-column data tables matters a lot.
- **Developer velocity and preference:** the author has ~20 years of C++
  (including prior Qt), plus recent Go/TypeScript. Toolkit ergonomics are a
  first-class concern.

## Considered Options

- **C++ / Qt Widgets / libsmb2** (chosen)
- **Go / Fyne / go-smb2**
- **Rust / egui (or Slint) / OS-mount or patched SMB**

## Decision Outcome

Chosen option: **C++ / Qt Widgets / libsmb2**, because the SMB k.o. criterion is
now proven satisfied with libsmb2 (spike 01), and among the options only Qt
delivers the stated native look-and-feel *and* a best-in-class model/view
framework that maps almost 1:1 onto this app's multi-pane, sortable, filterable
file tables. The author strongly prefers the Qt UI over Fyne and is comfortable
with C++ and with shipping dynamic Qt libraries.

The SMB spikes (below) removed SMB feasibility as a differentiator — both C and
Go can do the job — so the decision rests on GUI fit and developer preference,
where Qt wins clearly.

### SMB spike results (the evidence)

Two throwaway spikes were built and run against the real target server. Both had
to answer: enumerate a directory, read inode + link count, and create a hard link
so the source's link count goes 1 → 2 with a shared inode.

- **`spikes/01_libsmb2` (C):** libsmb2 exposes inode (`smb2_ino`) and link count
  (`smb2_nlink`) unpatched via `smb2_stat`/`smb2_readdir`. It ships **no**
  hard-link creation, but on the wire an SMB2 hard link is a `SET_INFO` with
  `FILE_LINK_INFORMATION` (class `0x0B`), whose Type-2 structure is byte-for-byte
  identical to `FILE_RENAME_INFORMATION` (`0x0A`) that libsmb2 already sends for
  `smb2_rename`. A small patch adds `smb2_link`/`smb2_link_async` (a copy of
  rename with the info-class byte flipped) plus one fallthrough case in the
  set-info encoder. **Confirmed PASS on the target server** — real hard link
  created, shared inode, link count 1 → 2.
- **`spikes/02_go-smb2` (Go):** go-smb2 ships neither hard-link creation nor
  inode/link-count in its public `FileStat`, but its internal protocol layer
  already decodes all of it. A fork patch (add `Link`, expose two `FileStat`
  fields, route `Stat`/`Lstat` through the `FileAllInformation` query) makes it
  work. Link creation confirmed on the target server; metadata read confirmed
  after the fork fix. **Also a PASS**, but requires a forked module and more
  patching than the C path.

Key shared finding: SMB2 directory enumeration never carries `NumberOfLinks`, so
displaying a link-count column requires a **per-file stat per entry** in either
library — a UI cost to budget for, not a stack differentiator.

### Consequences

- Good, because Qt is the only considered option that meets the **native
  look-and-feel** goal across Windows and Linux.
- Good, because Qt's **model/view framework** (`QAbstractItemModel` +
  `QSortFilterProxyModel` + `QTreeView`) maps directly onto the app's needs:
  in-memory list with a case-insensitive filter, "matches / total" counts,
  folders-sorted-to-top, sortable columns, and an icon delegate — mostly
  framework-provided rather than hand-built.
- Good, because **SMB is proven** end-to-end against the real server with the
  patched libsmb2.
- Good, because Qt Widgets **startup is instant** and **dynamic linking keeps the
  executable small**, satisfying those constraints.
- Good, because the author's **existing C++/Qt experience** shortens ramp-up
  (Qt Widgets APIs are stable across the last decade).
- Bad, because it commits us to **maintaining a libsmb2 fork** carrying the
  `smb2_link` patch (plus MSVC build fixes: `.syms` export entry and the
  `examples/CMakeLists.txt` `-Werror`/`_U_` guard).
- Bad, because **C++ has more ceremony** (CMake, manual memory management, slower
  edit-compile-run) than Go or Rust for a solo project.
- Bad, because Qt's **deployment footprint** (several shared libraries, tens of
  MB) is larger than a single Go/Rust static binary; accepted deliberately since
  dynamic Qt libraries are fine.
- Bad, because **cross-platform C++ CI** (build on Windows and Linux, provision
  Qt per runner) is more setup than `go build` / `cargo build`.
- Neutral, because the **per-entry stat** cost for link counts applies
  regardless of language/library.
- Neutral, because **Qt licensing** (LGPLv3) is satisfied by dynamic linking,
  which is the intended distribution model.

## More Information

- Spikes: `spikes/01_libsmb2/` (chosen SMB library, with the `smb2_link` patch and
  a README documenting it) and `spikes/02_go-smb2/` (the Go alternative, for the
  record).
- The libsmb2 `smb2_link` patch is small and self-contained; see
  `spikes/01_libsmb2/README.md` for the exact edits (encoder fallthrough,
  `smb2_link`/`smb2_link_async`, `libsmb2.syms` export, and MSVC build fixes).
