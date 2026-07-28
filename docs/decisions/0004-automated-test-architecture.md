---
date: 2026-07-28
---

# Automated Tests: Qt Test In-Process, Samba-in-Docker Fixture

## Context and Problem Statement

`docs/testing.md` began as a purely manual checklist run against the real
target server. Most of its items verify app logic (models, the link engine,
the search pipeline, widget wiring), not target-server quirks — so they can be
automated, if two problems are solved: the app needs a real SMB server that
supports the patched libsmb2's hard-link op (`FILE_LINK_INFORMATION`), and the
Qt widgets need to be driven without a human. How should the automated suite
be built?

## Decision Drivers

- **A real SMB server, reproducibly, on any dev machine** (Windows today,
  Linux/macOS possible) — the SMB layer is the heart of the app and mocks
  would test nothing.
- **Hard links must actually work** on the server's filesystem, and tests must
  be able to verify inode/nlink ground truth out-of-band.
- **Headless and deterministic**: suites run under ctest with no visible
  windows, no modal dialogs blocking, no reliance on timing luck.
- **Minimal app-code changes**: test seams should not reshape the app.

## Considered Options

- **Qt Test in-process + Samba in Docker** (chosen)
- External UI automation (accessibility APIs / Squish) against the built exe
- Mocked SMB layer (abstract SmbSession behind an interface)

## Decision Outcome

Chosen option: **Qt Test in-process + Samba in Docker**. External automation
is brittle and slow; a mocked session would skip the layer most worth testing
and force an interface onto a concrete class for no product benefit.

The architecture, and the reasoning baked into each piece:

- **Static-library split.** All app code lives in `hardlinkmgr_core`; the exe
  is just `main.cpp`. Test executables link the same objects. Because the
  icons `.qrc` moved into the static lib, every executable (app and tests)
  calls `Q_INIT_RESOURCE(icons)` or the linker may drop the resources.
- **One test executable per suite** (`tests/CMakeLists.txt`,
  `hlm_add_test()`): Qt Test runs one QObject class per `qExec()`; separate
  exes give crash isolation and ctest-level parallelism. All suites use
  `HLM_TEST_MAIN` (`tests/common/TestMain.h`), which defaults
  `QT_QPA_PLATFORM=offscreen` (overridable), initializes Winsock on Windows,
  registers resources, suppresses the debug-CRT abort dialog (a hidden modal
  dialog hangs headless runs), and enables `QStandardPaths` test mode so
  QSettings never touches the developer's real configuration.
- **ctest labels partition the suites**: `unit` (serverless) vs `docker`
  (needs the fixture), with `testPresets` in `CMakePresets.json`
  (`windows-unit`/`windows-all`, `linux-unit`/`linux-all`). If docker isn't on
  PATH, docker suites aren't registered at all.
- **Samba fixture** (`tests/docker/`): an in-repo ~20-line Dockerfile (public
  Samba images are unmaintained/opaque) from a **digest-pinned**
  `debian:trixie-slim`, one test user `hlmtest`, minimal `smb.conf`. To bump
  the base image: `docker pull debian:trixie-slim`, copy the new digest into
  the Dockerfile, rebuild, re-run the `docker` label.
  - **Named volume, never a bind mount**: on Docker Desktop (Windows/macOS)
    bind mounts cross a filesystem bridge where hard links don't behave like
    a real Linux filesystem. Verified: enumeration inode == stat inode ==
    `stat` in the container on the named volume.
  - **Host port 10445**: Windows owns 445; the app's URL syntax carries the
    port (`smb://hlmtest@localhost:10445/share`).
  - **Lifecycle via ctest fixtures**: `samba_fixture_up` (`docker compose up
    -d --build --wait`, blocking on the container healthcheck) and
    `samba_fixture_down` (`down -v`, wiping the volume) wrap the `docker`
    suites via `FIXTURES_SETUP`/`FIXTURES_CLEANUP`. A developer can also keep
    a long-lived fixture up manually; suites only talk SMB and `docker exec`.
  - **Per-test-case unique directories**: each fixture instance namespaces
    everything under `/hlm-<uuid>/…`, so suites are order-independent and
    parallel-safe; nothing is wiped mid-run.
  - **Out-of-band seeding/verification** (`tests/common/SmbFixture.h`):
    files are seeded and inode/nlink ground truth read via `docker exec`
    (`stat -c '%h %i'`). Everything seeded is `chown`ed to `hlmtest` — the
    SMB user needs directory write permission for rename/link/unlink.
    `chmod` in the container provides the fault injections (unlistable
    subfolder, read-only directory) that were never testable against the NAS.
  - Config via env vars (`HLM_TEST_SMB_URL`, `HLM_TEST_SMB_PASSWORD`,
    `HLM_TEST_SMB_CONTAINER`); suites `QSKIP` when the server is unreachable
    unless `HLM_TEST_SMB_STRICT=1`.
- **Seam policy — smallest that works**: one behavioral seam
  (`MainWindow::setPasswordPrompt` replaces the modal `QInputDialog`;
  `std::nullopt` = cancelled), `objectName`s on test-relevant widgets
  (`mw.*`, `fbv.*`, `mfp.*`, `hld.*`) for `findChild`, and a
  poll-and-click helper (`tests/common/TestSupport.h`) for the remaining
  static `QMessageBox`es. No friend classes, no interfaces. Pure logic worth
  serverless testing was extracted, not mocked: `core/MatchPairing.h` (the
  pairing math, with the truncation valves parameterized) and
  `core/MatchConflicts.h` (the checked-rows conflict scan).

### Consequences

- The suite already paid for itself twice during construction: it caught a
  latent use-after-free (`disconnectFromShare()` during a completion-signal
  handler destroyed the libsmb2 context from inside its own callback — fixed
  with the `m_inService` deferral in `SmbSession`) and two `pathutil::
  normalize` edge cases (`QDir::cleanPath` preserves a leading `//` and a
  root-level `..`).
- `docs/testing.md` now marks which checklist items are automated (and by
  which suite); the manual residue is perceptual items plus a slim
  spot check against the real NAS — Samba passing doesn't prove the target
  server's quirks haven't changed.
- Revisit if: suites need to run on a machine that cannot run Docker
  (a WSL2/remote Samba reachable via `HLM_TEST_SMB_URL` already works), or
  the fixture's ~3 s up/down overhead starts to matter.
