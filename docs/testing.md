# Manual Testing

Checklists for verifying each milestone against the real target server (see the
milestone list in the project `README.md`). Automated tests can't cover these:
they depend on a live SMB server's behavior.

## Milestone 1

SMB session + connect toolbar.

- [ ] **Connect:** enter `smb://user@host:port/share` for the real share, click
  Connect, enter the password in the prompt. The button shows a spinner labeled
  "Abort" while connecting, then a "Disconnect" state on success; the status bar
  reports the connection.
- [ ] **Disconnect:** click Disconnect on an established connection. The app
  returns to the disconnected state and the URL box becomes editable again.
- [ ] **Unresolvable host:** connect to a bogus hostname (e.g.
  `smb://user@foo/bar`). The UI stays responsive with the spinner showing (no
  freeze), Abort works during the lookup, and if left alone the attempt fails
  with a "could not resolve host" error in the status bar.
- [ ] **Abort mid-attempt:** connect to an unroutable IP (e.g.
  `smb://user@192.0.2.1/share`) so the attempt hangs, then click Abort while the
  spinner is showing. The app returns immediately to the disconnected state.
- [ ] **Wrong password:** connect to the real share with an incorrect password.
  The attempt fails, the app returns to the disconnected state, and the NTLMSSP
  authentication error appears in the status bar.
- [ ] **Cancelled password prompt:** press Cancel (or Esc) in the password
  dialog. No connection attempt starts; the app stays disconnected.
- [ ] **Bad URL:** try a URL missing the user (`smb://host/share`) or the share
  name (`smb://user@host`). No password prompt appears; a specific error shows
  in the status bar.
