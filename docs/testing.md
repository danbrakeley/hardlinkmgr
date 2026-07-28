# Testing

Most of this file is the per-milestone checklist that was originally verified
by hand against the real target server. The automated suite (see ADR 0004)
now covers the majority of it against a local Samba fixture: each item below
is tagged **(automated: tst_xxx)** when a suite exercises it, or **(manual)**
when only a human can judge it (perceptual items, and anything that only the
real server can prove). Checkbox state tracks the original manual runs.

## Running the automated tests

Build as usual, then:

```powershell
ctest --preset windows-unit    # serverless suites only (< 1 s)
ctest --preset windows-all     # everything; starts/stops the Samba fixture itself
```

On Linux use `linux-unit` / `linux-all` with the `linux-debug` build.

The `docker`-labeled suites need Docker (Desktop) with Compose v2. ctest
builds and starts the fixture container (`samba_fixture_up`), runs the
suites, and tears it down volume-and-all (`samba_fixture_down`). For a faster
edit-run loop, keep a long-lived fixture up and run suites directly:

```powershell
docker compose -f tests/docker/docker-compose.yml up -d --build --wait
ctest --test-dir build\windows -C Debug -R tst_linkrunner --output-on-failure
```

Suites `QSKIP` when the server is unreachable (set `HLM_TEST_SMB_STRICT=1` to
fail loudly instead). Defaults — URL `smb://hlmtest@localhost:10445/share`,
password `hlmtest` — can be overridden with `HLM_TEST_SMB_URL` /
`HLM_TEST_SMB_PASSWORD` / `HLM_TEST_SMB_CONTAINER` (e.g. if port 10445
collides with something local; change the compose file to match). Widget
suites run on the offscreen platform; to watch one on screen:
`$env:QT_QPA_PLATFORM='windows'; .\build\windows\bin\Debug\tst_mainwindow.exe`.

What stays manual by nature: UI smoothness/stutter, spinner rendering, the
no-console-window check, splitter drag feel, the Wayland launch, and a slim
spot check against the real NAS whenever libsmb2 or the server changes —
Samba passing doesn't prove the target server's quirks haven't.

## Milestone 1

SMB session + connect toolbar.

- [x] **Connect:** enter `smb://user@host:port/share` for the real share, click
  Connect, enter the password in the prompt. The button shows a spinner labeled
  "Abort" while connecting, then a "Disconnect" state on success; the status bar
  reports the connection. (automated: tst_smbsession, tst_mainwindow —
  spinner rendering manual)
- [x] **Disconnect:** click Disconnect on an established connection. The app
  returns to the disconnected state and the URL box becomes editable again.
  (automated: tst_mainwindow)
- [x] **Unresolvable host:** connect to a bogus hostname (e.g.
  `smb://user@foo/bar`). The UI stays responsive with the spinner showing (no
  freeze), Abort works during the lookup, and if left alone the attempt fails
  with a "could not resolve host" error in the status bar.
  (automated: tst_smbsession_offline — responsiveness/spinner manual)
- [x] **Abort mid-attempt:** connect to an unroutable IP (e.g.
  `smb://user@192.0.2.1/share`) so the attempt hangs, then click Abort while the
  spinner is showing. The app returns immediately to the disconnected state.
  (automated: tst_smbsession_offline)
- [x] **Wrong password:** connect to the real share with an incorrect password.
  The attempt fails, the app returns to the disconnected state, and the NTLMSSP
  authentication error appears in the status bar. (automated: tst_smbsession)
- [x] **Cancelled password prompt:** press Cancel (or Esc) in the password
  dialog. No connection attempt starts; the app stays disconnected.
  (automated: tst_mainwindow)
- [x] **Bad URL:** try a URL missing the user (`smb://host/share`) or the share
  name (`smb://user@host`). No password prompt appears; a specific error shows
  in the status bar. (automated: tst_smbsharespec, tst_mainwindow)

## Milestone 2

Single file browser view.

- [x] **Root listing:** connect to the real share. The view appears and lists
  `/` with folders grouped on top: icon, name, size (blank for folders), date
  modified, and inode columns populated. The Links column shows "…" for files
  (the lazy stat that fills it in is milestone 3).
  (automated: tst_smbsession, tst_filebrowserview, tst_filelistmodel)
- [x] **Navigation down:** double-click a folder (or select it and press
  Enter). The view lists that folder and the path box updates.
  (automated: tst_filebrowserview)
- [x] **Navigation via path box:** type a known path (e.g. `/some/subdir`) and
  press Enter; the view lists it. Also try `..` segments (e.g.
  `/some/subdir/..`) — the path normalizes and lists the parent.
  (automated: tst_filebrowserview, tst_pathutil)
- [x] **Bad path:** type a nonexistent path and press Enter. The previous
  listing stays, the path box reverts to the current path, and an error shows
  in the status bar. (automated: tst_filebrowserview)
- [x] **Filter:** type in the filter box. Only case-insensitive substring
  matches remain visible, and the count label switches from "total" to
  "matches / total". Clearing the filter restores the full list and plain
  total. (automated: tst_filebrowserview, tst_filefilterproxymodel)
- [x] **Sorting:** click each column header, both directions. Folders stay
  grouped on top in every case; names sort case-insensitively; size, date,
  and inode sort by value (not as text).
  (automated: tst_filefilterproxymodel, tst_filelistmodel)
- [x] **Large directory:** open the biggest directory on the share. The
  listing appears without the UI stuttering (if it does stutter, see the
  threading escape hatch in CLAUDE.md). (manual — perceptual)
- [x] **Disconnect/reconnect:** disconnect while browsing, reconnect. A fresh
  view appears at `/`. (automated: tst_mainwindow)
- [x] **Parent button:** the up button left of the path box is disabled at
  `/`, enabled after descending into a folder, and navigates to the immediate
  parent when clicked. (automated: tst_filebrowserview)
- [x] **Folders-first toggle:** the folder-icon toggle starts on (folders
  grouped on top). Turning it off re-sorts folders alphabetically among the
  files; turning it back on regroups them, in both sort directions.
  (automated: tst_filefilterproxymodel — menu wiring manual)
- [x] **Case-sensitivity toggle:** the "Aa" toggle starts off
  (case-insensitive). With mixed-case names, turning it on re-sorts with
  uppercase before lowercase; hover shows the explanatory tooltip.
  (automated: tst_filefilterproxymodel — menu wiring/tooltip manual)

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

The automated suites use the same throttle to make fill-in states observable.

- [x] **Counts populate:** open a directory with files. The Links column
  starts as "…" and fills in with real numbers; known hard-linked files show
  their correct count (e.g. 2), everything else 1.
  (automated: tst_filebrowserview, tst_smbsession)
- [x] **Visible rows first:** open a directory too big to fit on screen. The
  rows on screen fill in first; after scrolling to the bottom, the newly
  visible rows fill in ahead of the untouched middle. (manual — perceptual)
- [x] **Large directory drains:** leave the biggest directory open. All "…"
  eventually become numbers, the UI stays responsive throughout, and scrolling
  during the fill stays smooth.
  (automated: tst_filebrowserview — responsiveness/smoothness manual)
- [x] **Navigation cancels:** open a big directory, then navigate away while
  Links is still filling. The new directory lists and fills normally; no stale
  counts appear in it. (automated: tst_filebrowserview)
- [x] **Sort by Links while filling:** sort on the Links column during a fill.
  Rows re-order live as counts arrive ("…" and "?" sort below real counts).
  (automated: tst_filelistmodel for the sort values — live re-order manual)
- [x] **Disconnect mid-fill:** disconnect while a big directory is filling.
  No crash, no errors beyond the normal disconnect; reconnecting works.
  (automated: tst_smbsession, tst_filebrowserview)

## Milestone 4

Selection + Hard Link dialog + link execution. **Use throwaway copies of files
in a test directory** — the run replaces file contents by design.

- [x] **Link enablement:** the Link button is disabled with 0 or 1 files
  selected, enabled at 2+, and folders don't count (a folder + one file stays
  disabled). It disables again after navigation (selection clears) and on
  disconnect. (automated: tst_mainwindow)
- [x] **Dialog basics:** with 2+ files selected, Link opens the dialog listing
  each file's path, size / links / inode. "Hard Link" is disabled until a
  primary radio is chosen. Cancel before running changes nothing on the share.
  (automated: tst_hardlinkdialog)
- [x] **Happy path:** select two throwaway copies, pick a primary, run. The
  victim's status walks through the steps to "replaced with hard link"; after
  Close, the view refreshes and both names show the same inode and Links = 2;
  the victim's content now matches the primary (check over the share).
  (automated: tst_linkrunner, tst_hardlinkdialog)
- [x] **Three files:** select three copies, keep one primary. Both victims are
  replaced sequentially; Links shows 3 on all three names after refresh.
  (automated: tst_linkrunner)
- [x] **No leftover tmp:** after a successful run, no `*.hlmgr-tmp` files
  remain in the directory. (automated: tst_linkrunner, tst_hardlinkdialog)
- [x] **Link failure restores:** cause the link step to fail (e.g. make the
  victim's folder read-only for the user, if possible). The status reports the
  failure with "(original restored)", and the victim is back under its
  original name with its original content. (automated: tst_linkrunner — was
  never verifiable manually; the fixture injects the failure)
- [x] **Already-linked pair:** run the dialog on two names that are already
  hard links of each other. No data is lost (the result is the same pair,
  link count unchanged). (automated: tst_linkrunner)
- [x] **Disconnect mid-run:** with the stat throttle off but on a slower run
  (several victims), disconnect mid-sequence if you can catch it. Remaining
  steps fail with "Disconnected.", no crash; check the share for a stranded
  `*.hlmgr-tmp` and restore it manually if present (this is the documented
  cost of aborting mid-sequence). (automated: tst_linkrunner —
  deterministically, via disconnect inside a jobFinished handler)

## Milestone 5

Multiple views + polish.

- ~~**Add view**~~ / ~~**Close view**~~ — removed by ADR 0003 (fixed two views).
- [x] **Cross-view selection:** select one file in one view and one in
  another — Link enables. The same file selected in both views counts once
  (Link stays disabled until a second distinct file is selected).
  (automated: tst_mainwindow)
- [x] **Cross-view link run:** link a file from view A with a file from
  view B. After Close, *all* views refresh (both show updated link counts).
  (automated: tst_mainwindow + tst_hardlinkdialog cover the pieces; the
  refresh-after-Close wiring is spot-checked manually)
- [x] **No console window:** launching the exe (double-click from Explorer)
  opens only the app window, no console. (manual)
- [x] **Remembered URL:** connect successfully, quit, relaunch. The URL box
  is pre-filled with the last share URL; only the password is asked for
  again (never stored). (automated: tst_mainwindow)

## Milestone 6

Linux build (Ubuntu 26.04, setup per the Linux section of `README.md`).

- [x] **Clean build:** on a fresh system with only the documented apt
  packages, `linux-debug` and `linux-release` both configure and build with
  no errors (no source changes were needed over the Windows build).
  (automated in effect: `ctest --preset linux-all` builds and runs everything)
- [x] **Launch:** the built binary starts under the default Wayland session
  and shows the main window. (manual)
- [x] **Connect + browse:** connect to the real share, browse and filter a
  directory (milestone 1/2 spot check on this platform). (manual — real NAS)
- [x] **Link counts:** the Links column fills in against known hard-linked
  files (milestone 3 spot check). (manual — real NAS)
- [x] **Link run:** end-to-end hard-link run on throwaway copies
  (milestone 4 spot check). (manual — real NAS)
- [x] **Remembered URL:** the last URL persists across relaunch
  (`QSettings` lands in `~/.config` on Linux). (automated: tst_mainwindow)

## Milestone 7

Match Finder: the search options + results panel right of the file views
(design in `roadmap.md`, "Generate a list of potential matches"). **The link
checks replace file contents by design — point the paths at throwaway
copies.**

- [ ] **Panel + splitter:** connect. The views sit left, the Match Finder
  panel right, in a resizable splitter. Drag the divider, disconnect,
  reconnect, then quit and relaunch — the position is restored each time.
  (automated: tst_mainwindow for the layout — divider drag/restore manual)
- [ ] **Options persist:** set both paths, uncheck one "Include Subfolders",
  change both sizes and units, quit, relaunch, connect. Everything is
  restored. (automated: tst_matchfinderpanel)
- [ ] **Saved-path validation:** connect to a server (or share) that lacks a
  saved path. That path field is cleared and the status bar explains why;
  a valid saved path survives untouched. (automated: tst_matchfinderpanel)
- [ ] **Dialog regression:** the toolbar Link flow (select 2+ files across
  views) behaves exactly as before — statuses walk the same steps, failures
  report the same messages (the engine moved into `LinkRunner`).
  (automated: tst_hardlinkdialog, tst_linkrunner)
- [ ] **Basic search:** point Primary and Secondary at two known folders,
  defaults (10 MiB / 0 MiB). Progress text updates while listing; on
  completion the expected same-size pairs appear and pairs that are already
  hard-linked (same inode) are absent.
  (automated: tst_matchsearcher, tst_matchfinderpanel)
- [ ] **Overlapping paths:** set Primary == Secondary — results contain no
  self-pairs and no A/B + B/A duplicate rows. Then nest Secondary inside a
  recursive Primary — the overlap is listed once (watch the folder count in
  the progress text). (automated: tst_matchsearcher, tst_matchpairing)
- [ ] **Include Subfolders off:** uncheck it on one side — only that root's
  direct files are considered. (automated: tst_matchsearcher)
- [ ] **Size options:** Size Min 0 lets small files in; Size Difference 0
  yields exact-size matches only; a small nonzero difference adds near-size
  pairs. (automated: tst_matchsearcher, tst_matchpairing)
- [ ] **Cancel mid-search:** on a big tree, click Cancel Search. The button
  reverts to Start Search, no errors or stale results appear, and the table
  keeps whatever it showed before the search.
  (automated: tst_matchsearcher, tst_matchfinderpanel)
- [ ] **Search errors:** a nonexistent Primary path fails the search with a
  message in the panel; an unlistable *subfolder* (permissions) lets the
  search complete with "N folder(s) could not be listed".
  (automated: tst_matchsearcher — the fixture injects the permission error)
- ~~**Row reveal**~~ — superseded by ADR 0003 (two fixed views); reveal into
  both views still works and is exercised implicitly via `revealRequested`.
- [ ] **Check-all + link run:** the header checkbox checks/unchecks every
  row (tri-state with a partial selection). Link Selected Matches enables at
  ≥1 checked; checking two rows that share a file is blocked with a conflict
  warning; the confirmation shows the right count; during the run each row's
  Status walks the steps, failure text stays in the row, and all file views
  refresh afterward with updated link counts and no leftover `*.hlmgr-tmp`.
  (automated: tst_matchfinderpanel, tst_matchresultsmodel, tst_matchconflicts)
