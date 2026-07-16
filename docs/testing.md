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
