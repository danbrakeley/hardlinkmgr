# SMB2 hard-link spike (go-smb2)

The Go counterpart to `../01_libsmb2`. Same question, same server: can we read
inode + link count and **create a hard link** over SMB2 — here via
[`go-smb2`](https://github.com/hirochachacha/go-smb2), a pure-Go SMB2/3 client?

`main.go` connects, lists a directory (with per-entry inode/link-count), stats a
source file, creates a hard link, and verifies the source's link count went
`1 -> 2` with a **shared inode**. Verdict + exit code mirror spike 01.

## Finding up front

Out of the box, go-smb2 does **less** than libsmb2 for our needs:

- **No hard-link creation.** It has `Rename`, `Symlink`, `Remove` — but no `Link`.
- **No inode / link count.** Its public `FileStat` exposes times, size,
  attributes, name — but not `NumberOfLinks` or `IndexNumber`.

The good news: the protocol layer already has everything. Internally the library
defines the `FileLinkInformation` class (0x0B) and a ready
`FileLinkInformationType2Encoder`, and its `FileAllInformation` decoder already
provides `StandardInformation().NumberOfLinks()` and
`InternalInformation().IndexNumber()`. `Stat` even decodes `FileAllInformation`
already — it just drops those two fields on the floor. So both gaps are thin
"expose what's already there" patches, not new protocol work.

The catch, versus libsmb2: go-smb2 keeps its protocol types in an **`internal/`
package**, so a downstream program cannot construct a `SetInfoRequest` or touch
those decoders. **The patch must live inside a fork of the module**, consumed via
a `replace` directive. That's exactly how this spike is wired.

## The fork patch (4 edits, all in `./go-smb2`)

### 1. `client.go` — expose the two fields on `FileStat`

```go
type FileStat struct {
  ...
  FileName       string
  NumberOfLinks  uint32   // added
  IndexNumber    int64    // added
}
```

### 2. `client.go` — populate them in `(*File).stat()`

`(*File).stat()` issues a `FileAllInformation` query and already has `std` in
hand; add the two fields:

```go
    FileName:      base(f.name),
    NumberOfLinks: std.NumberOfLinks(),                     // added
    IndexNumber:   info.InternalInformation().IndexNumber(), // added
  }, nil
```

### 3. `client.go` — route `Share.Stat`/`Share.Lstat` through `(*File).stat()`

**This is the edit that actually makes the fields appear.** By default
`Share.Stat`/`Share.Lstat` open the file and return `f.fileStat` — the struct
built from the lighter **CREATE response**, which carries size/times/attributes
but *not* `NumberOfLinks`/`IndexNumber` (they come back as 0). Change both
methods to return the `FileAllInformation` query instead:

```go
  fi, err := f.stat()   // was: fi, err := f.fileStat, nil
```

Symptom if you skip this: the link is created correctly and `size` reads fine,
but `ino`/`nlink` are `0` — a false INCONCLUSIVE. (The other `FileStat` sites —
`newFile` from CREATE, and `ReadDir` from `FileDirectoryInformation` — still
leave these zero because those SMB2 responses genuinely don't carry them; see the
ReadDir note below.)

### 4. `link.go` (new file) — the `Link` method

A copy of `Rename` with the info class flipped to `FileLinkInformation` and the
matching Type-2 encoder. Full source is in `./go-smb2/link.go`. The desired
access mask mirrors the one the libsmb2 spike used successfully against the
target server (`GENERIC_READ | FILE_READ_ATTRIBUTES | DELETE`).

## Build & run

The `replace` in `go.mod` points at the patched fork, so a normal build picks it
up. No cgo, no Kerberos, no external runtime — one static `.exe`.

```powershell
go build -o gosmb2-link-spike.exe .

$env:SMB2_PASSWORD = 'secret'
.\gosmb2-link-spike.exe -host 192.168.1.10 -share media -path dedup-test -user dan original.bin link.bin
```

```sh
# Linux/macOS
go build -o gosmb2-link-spike .
SMB2_PASSWORD=secret ./gosmb2-link-spike -host 192.168.1.10 -share media -path dedup-test -user dan original.bin link.bin
```

Put a throwaway file (e.g. `original.bin`) in the `-path` subdir first. Add
`-domain WORKGROUP` if your server needs it. Set `KEEP_LINK=1` to keep the link
instead of cleaning it up. Expected on success:

```text
=== Verdict ===
  source nlink: (>=2 expected) now 2
  inode:        source=1234567890  link=1234567890  (EQUAL)

  PASS: a real hard link was created and the server reports the shared
  inode and incremented link count this app depends on.
```

Interpretation and the server-capability caveat are identical to spike 01 —
if the link is created but `ino`/`nlink` come back zero/unchanged, that's a
property of the SMB server, not the client, and it changes the architecture.

## An architectural note: link count is NOT free in directory listings

SMB2 directory enumeration (`FileDirectoryInformation` / even the `FileId*`
variants) **never carries `NumberOfLinks`**. Only a per-file query
(`FileAllInformation` / `FileStandardInformation`) returns it. So showing a
hard-link-count column in the app's file list requires **one Stat per entry**,
in *either* library — this spike does exactly that in its listing loop. Inode
(`IndexNumber` / `FileId`) *is* available in enumeration via the `FileId*`
classes; link count is not. Budget for the N extra round trips (or fetch them
lazily / in parallel) in the real UI.

## go-smb2 vs libsmb2 — how the two paths actually compare

|                         | **libsmb2 (C)** — spike 01                                | **go-smb2 (Go)** — this spike          |
| ----------------------- | --------------------------------------------------------- | -------------------------------------- |
| Read inode + link count | Works unpatched (in `smb2_stat_64`)                       | Needs a 2-field fork patch to expose   |
| Create hard link        | Needs `smb2_link` patch (+ encoder + `.def`/CMake fixes)  | Needs a `Link` method in a fork        |
| Where the patch lives   | Vendored C source you compile                             | Forked module via `replace`            |
| Build friction          | CMake + MSVC quirks (`-Werror`, `.syms` export, DLL copy) | `go build` — one static binary, no cgo |
| Cross-compile Win↔Linux | Per-platform builds (cgo-ish toolchain)                   | `GOOS=… go build`, trivial             |
| Distribution            | `.exe` + `smb2.dll`                                       | single `.exe`                          |

Net: **libsmb2 needs slightly less patching; go-smb2 needs slightly more but is
far smoother to build, cross-compile, and ship.** Both reach the same place, and
both hinge on the same server-capability question — which spike 01 already
confirmed PASSES on the target server. The real decision now rides on GUI
toolkit + developer velocity (the earlier discussion), not on SMB feasibility.

## Next

Run this against the same server/share/file as spike 01 and confirm an identical
PASS. Then the language choice is a GUI/velocity call, not an SMB one.
