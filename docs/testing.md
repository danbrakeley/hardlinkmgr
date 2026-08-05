# Testing

This file originally tracked a list of tests to be performed manually.

As tests are automated (see ADR 4), they are removed from this file.

## Running the automated tests

This is now covered in the [Makefile](../Makefile).

To run all tests, you'll need Docker to be running.

For a faster edit-run loop, keep a long-lived fixture up (`samba_fixture_up` and `samba_fixture_down`) and run suites directly:

```powershell
docker compose -f tests/docker/docker-compose.yml up -d --build --wait
ctest --test-dir build\windows -C Debug -R tst_linkrunner --output-on-failure
```

Suites `QSKIP` when the server is unreachable (set `HLM_TEST_SMB_STRICT=1` to fail loudly instead).

| field    | default                               | env var override         |
| -------- | ------------------------------------- | ------------------------ |
| URL      | `smb://hlmtest@localhost:10445/share` | `HLM_TEST_SMB_URL`       |
| password | `hlmtest`                             | `HLM_TEST_SMB_PASSWORD`  |
|          |                                       | `HLM_TEST_SMB_CONTAINER` |

Widget suites run on the offscreen platform; to watch one on screen:

```powershell
$env:QT_QPA_PLATFORM='windows'; .\build\windows\bin\Debug\tst_mainwindow.exe
```

What stays manual by nature: UI smoothness/stutter, spinner rendering, the no-console-window check, splitter drag feel, the Wayland launch, and a slim spot check against the real NAS whenever libsmb2 or the server changes — Samba passing doesn't prove the target server's quirks haven't.

## Milestone 2

Single file browser view.

- [x] **Large directory:** open the biggest directory on the share. The listing appears without the UI stuttering (if it does stutter, see the threading escape hatch in CLAUDE.md). (manual — perceptual)

## Milestone 3

Lazy hard-link-count fill-in.

A fast LAN server answers stats too quickly to observe the fill-in. Set the debug throttle before launching to simulate a slow server — each stat then holds its in-flight slot for N ms before being sent (with the 32-slot window, 200 ms ≈ 160 stats/s):

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

- [x] **Visible rows first:** open a directory too big to fit on screen. The rows on screen fill in first; after scrolling to the bottom, the newly visible rows fill in ahead of the untouched middle. (manual — perceptual)
- [x] **Large directory drains:** leave the biggest directory open. All "…" eventually become numbers, the UI stays responsive throughout, and scrolling during the fill stays smooth. (automated: tst_filebrowserview — responsiveness/smoothness manual)
- [x] **Sort by Links while filling:** sort on the Links column during a fill. Rows re-order live as counts arrive ("…" and "?" sort below real counts). (automated: tst_filelistmodel for the sort values — live re-order manual)

## Milestone 5

Multiple views + polish.

- [x] **No console window:** launching the exe (double-click from Explorer) opens only the app window, no console. (manual)

## Milestone 6

Linux build (Ubuntu 26.04, setup per the Linux section of `README.md`).

- [x] **Launch:** the built binary starts under the default Wayland session and shows the main window. (manual)
- [x] **Connect + browse:** connect to the real share, browse and filter a directory (milestone 1/2 spot check on this platform). (manual — real NAS)
- [x] **Link counts:** the Links column fills in against known hard-linked files (milestone 3 spot check). (manual — real NAS)
- [x] **Link run:** end-to-end hard-link run on throwaway copies (milestone 4 spot check). (manual — real NAS)

## Milestone 7

Match Finder: the search options + results panel right of the file views (design in `roadmap.md`, "Generate a list of potential matches"). **The link checks replace file contents by design — point the paths at throwaway copies.**

- [ ] **Panel + splitter:** connect. The views sit left, the Match Finder panel right, in a resizable splitter. Drag the divider, disconnect, reconnect, then quit and relaunch — the position is restored each time. (automated: tst_mainwindow for the layout — divider drag/restore manual)

## Audit log

"Keep log/history of actions/errors" (roadmap): a jsonl audit trail of server writes, connects/disconnects, and errors, appended to `log.jsonl` in the app-data directory (`%LOCALAPPDATA%\brakeley\hardlinkmgr` on Windows, `~/.local/share/brakeley/hardlinkmgr` on Linux).

- [x] **Line format:** every line is a single JSON object carrying `level` (`info`/`error` only), RFC 3339 UTC `time`, `msg`, and structured fields; awkward strings stay on one line. (automated: tst_logformat)
- [x] **File behavior:** lines append across sessions, missing directories are created, no file is written until a path is set. (automated: tst_logger)
- [x] **Link-run trail:** a successful link run logs its rename / hard link / unlink lines, in order, with the operations' paths. (automated: tst_linkrunner)
- [ ] **On the real app:** connect, run one link, disconnect. The app-data `log.jsonl` gained connecting/connected, the three write lines, and disconnected — and no line contains the password. (manual)
