# Manual Testing

Checklists for verifying each milestone against the real target server (see the
milestone list in the project `README.md`). Automated tests can't cover these:
they depend on a live SMB server's behavior.

## Milestone 1

SMB session + connect toolbar.

- [x] **Connect:** enter `smb://user@host:port/share` for the real share, click
  Connect, enter the password in the prompt. The button shows a spinner labeled
  "Abort" while connecting, then a "Disconnect" state on success; the status bar
  reports the connection.
- [x] **Disconnect:** click Disconnect on an established connection. The app
  returns to the disconnected state and the URL box becomes editable again.
- [x] **Unresolvable host:** connect to a bogus hostname (e.g.
  `smb://user@foo/bar`). The UI stays responsive with the spinner showing (no
  freeze), Abort works during the lookup, and if left alone the attempt fails
  with a "could not resolve host" error in the status bar.
- [x] **Abort mid-attempt:** connect to an unroutable IP (e.g.
  `smb://user@192.0.2.1/share`) so the attempt hangs, then click Abort while the
  spinner is showing. The app returns immediately to the disconnected state.
- [x] **Wrong password:** connect to the real share with an incorrect password.
  The attempt fails, the app returns to the disconnected state, and the NTLMSSP
  authentication error appears in the status bar.
- [x] **Cancelled password prompt:** press Cancel (or Esc) in the password
  dialog. No connection attempt starts; the app stays disconnected.
- [x] **Bad URL:** try a URL missing the user (`smb://host/share`) or the share
  name (`smb://user@host`). No password prompt appears; a specific error shows
  in the status bar.

## Milestone 2

Single file browser view.

- [x] **Root listing:** connect to the real share. The view appears and lists
  `/` with folders grouped on top: icon, name, size (blank for folders), date
  modified, and inode columns populated. The Links column shows "…" for files
  (the lazy stat that fills it in is milestone 3).
- [x] **Navigation down:** double-click a folder (or select it and press
  Enter). The view lists that folder and the path box updates.
- [x] **Navigation via path box:** type a known path (e.g. `/some/subdir`) and
  press Enter; the view lists it. Also try `..` segments (e.g.
  `/some/subdir/..`) — the path normalizes and lists the parent.
- [x] **Bad path:** type a nonexistent path and press Enter. The previous
  listing stays, the path box reverts to the current path, and an error shows
  in the status bar.
- [x] **Filter:** type in the filter box. Only case-insensitive substring
  matches remain visible, and the count label switches from "total" to
  "matches / total". Clearing the filter restores the full list and plain
  total.
- [x] **Sorting:** click each column header, both directions. Folders stay
  grouped on top in every case; names sort case-insensitively; size, date,
  and inode sort by value (not as text).
- [x] **Large directory:** open the biggest directory on the share. The
  listing appears without the UI stuttering (if it does stutter, see the
  threading escape hatch in CLAUDE.md).
- [x] **Disconnect/reconnect:** disconnect while browsing, reconnect. A fresh
  view appears at `/`.
- [x] **Parent button:** the up button left of the path box is disabled at
  `/`, enabled after descending into a folder, and navigates to the immediate
  parent when clicked.
- [x] **Folders-first toggle:** the folder-icon toggle starts on (folders
  grouped on top). Turning it off re-sorts folders alphabetically among the
  files; turning it back on regroups them, in both sort directions.
- [x] **Case-sensitivity toggle:** the "Aa" toggle starts off
  (case-insensitive). With mixed-case names, turning it on re-sorts with
  uppercase before lowercase; hover shows the explanatory tooltip.

## Milestone 3

Lazy hard-link-count fill-in.

A fast LAN server answers stats too quickly to observe the fill-in. Set the
debug throttle before launching to simulate a slow server — each stat then
holds its in-flight slot for N ms before being sent (with the 32-slot window,
200 ms ≈ 160 stats/s):

```powershell
$env:HLM_STAT_DELAY_MS = '200'
.\build\windows\bin\Release\hardlinkmgr.exe
```

Unset it (`Remove-Item Env:HLM_STAT_DELAY_MS`) for normal behavior.

On Linux:

```bash
HLM_STAT_DELAY_MS=200 ./build/linux-release/bin/hardlinkmgr
```

- [x] **Counts populate:** open a directory with files. The Links column
  starts as "…" and fills in with real numbers; known hard-linked files show
  their correct count (e.g. 2), everything else 1.
- [x] **Visible rows first:** open a directory too big to fit on screen. The
  rows on screen fill in first; after scrolling to the bottom, the newly
  visible rows fill in ahead of the untouched middle.
- [x] **Large directory drains:** leave the biggest directory open. All "…"
  eventually become numbers, the UI stays responsive throughout, and scrolling
  during the fill stays smooth.
- [x] **Navigation cancels:** open a big directory, then navigate away while
  Links is still filling. The new directory lists and fills normally; no stale
  counts appear in it.
- [x] **Sort by Links while filling:** sort on the Links column during a fill.
  Rows re-order live as counts arrive ("…" and "?" sort below real counts).
- [x] **Disconnect mid-fill:** disconnect while a big directory is filling.
  No crash, no errors beyond the normal disconnect; reconnecting works.

## Milestone 4

Selection + Hard Link dialog + link execution. **Use throwaway copies of files
in a test directory** — the run replaces file contents by design.

- [x] **Link enablement:** the Link button is disabled with 0 or 1 files
  selected, enabled at 2+, and folders don't count (a folder + one file stays
  disabled). It disables again after navigation (selection clears) and on
  disconnect.
- [x] **Dialog basics:** with 2+ files selected, Link opens the dialog listing
  each file's path, size / links / inode. "Hard Link" is disabled until a
  primary radio is chosen. Cancel before running changes nothing on the share.
- [x] **Happy path:** select two throwaway copies, pick a primary, run. The
  victim's status walks through the steps to "replaced with hard link"; after
  Close, the view refreshes and both names show the same inode and Links = 2;
  the victim's content now matches the primary (check over the share).
- [x] **Three files:** select three copies, keep one primary. Both victims are
  replaced sequentially; Links shows 3 on all three names after refresh.
- [x] **No leftover tmp:** after a successful run, no `*.hlmgr-tmp` files
  remain in the directory.
- [ ] **Link failure restores:** cause the link step to fail (e.g. make the
  victim's folder read-only for the user, if possible). The status reports the
  failure with "(original restored)", and the victim is back under its
  original name with its original content.
- [x] **Already-linked pair:** run the dialog on two names that are already
  hard links of each other. No data is lost (the result is the same pair,
  link count unchanged).
- [x] **Disconnect mid-run:** with the stat throttle off but on a slower run
  (several victims), disconnect mid-sequence if you can catch it. Remaining
  steps fail with "Disconnected.", no crash; check the share for a stranded
  `*.hlmgr-tmp` and restore it manually if present (this is the documented
  cost of aborting mid-sequence).

## Milestone 5

Multiple views + polish.

- [x] **Add view:** connect, then click "Add View". A second view appears
  below the first in a resizable splitter, starting at `/`, with its own
  path, filter, and sort toggles working independently.
- [x] **Close view:** with 2+ views, each shows a close button at the right
  of its toolbar; with only one view the button is hidden. Closing removes
  just that view.
- [x] **Cross-view selection:** select one file in one view and one in
  another — Link enables. The same file selected in both views counts once
  (Link stays disabled until a second distinct file is selected).
- [x] **Cross-view link run:** link a file from view A with a file from
  view B. After Close, *all* views refresh (both show updated link counts).
- [x] **No console window:** launching the exe (double-click from Explorer)
  opens only the app window, no console.
- [x] **Remembered URL:** connect successfully, quit, relaunch. The URL box
  is pre-filled with the last share URL; only the password is asked for
  again (never stored).

## Milestone 6

Linux build (Ubuntu 26.04, setup per the Linux section of `README.md`).

- [x] **Clean build:** on a fresh system with only the documented apt
  packages, `linux-debug` and `linux-release` both configure and build with
  no errors (no source changes were needed over the Windows build).
- [x] **Launch:** the built binary starts under the default Wayland session
  and shows the main window.
- [x] **Connect + browse:** connect to the real share, browse and filter a
  directory (milestone 1/2 spot check on this platform).
- [x] **Link counts:** the Links column fills in against known hard-linked
  files (milestone 3 spot check).
- [x] **Link run:** end-to-end hard-link run on throwaway copies
  (milestone 4 spot check).
- [x] **Remembered URL:** the last URL persists across relaunch
  (`QSettings` lands in `~/.config` on Linux).
