# Hard Link Manager

## Original Problem

I ended up in the position where I had a remote SMB file share with multiple copies of the same large files. The file names didn't match, and in some cases the file sizes didn't match either, and I wanted a way to identify two (or more) files by hand, then [hard link](https://en.wikipedia.org/wiki/Hard_link) them to each other to avoid wasting space.

While there are existing applications to identify duplicates and replace them with hard links (i.e. [jdupes](https://codeberg.org/jbruchon/jdupes)), my situation involved files that didn't always have exactly the same size, but I generally knew where to find the two files, and just wanted a way to help me locate known duplicates and then explicitly hard link them to the same bytes.

## Constraints

- GUI application
- App starts instantly (lightweight)
- Low resource usage
- Cross platform (Windows & Linux required, macOS is nice-to-have)
- Looks and feels like a native app on each platform (OS has good solutions for hotkeys/UX, don't re-invent the wheel)

## UI/UX

TODO: add some screen shots, maybe an animation, and maybe some short descriptions.

## Documents

- [ADRs](./docs/decisions/) - Architectural Decision Records
- [roadmap.md](./docs/roadmap.md) - Where this app is heading
- [testing.md](./docs/testing.md) - List of intended behavior, in checklist form

## Build

The included [`Makefile`](./Makefile) handles most common operations in a cross-platform way.

```text
$ make help
Targets:
  configure  - regenerate CMake's build files (run after editing CMakeLists.txt)
  release    - build hardlinkmgr (Release, app only)
  debug      - build hardlinkmgr (Debug, app only)
  test-unit  - build + run the serverless unit suite
  test-all   - build + run every suite (needs Docker)
  clean      - remove the build/ directory
```

| command     | notes                                                                                  |
| ----------- | -------------------------------------------------------------------------------------- |
| `configure` | Run this on a fresh sync or after a `clean`, or whenever `CMakeLists.txt` has changed. |
| `release`   | Generates a release executable. If it fails, try `configure release`.                  |
| `debug`     | Generates a debug executable. If it fails, try `configure debug`.                      |
| `test-unit` | Builds and runs unit tests. Does not require Docker.                                   |
| `test-all`  | Builds and runs unit tests + integration tests. Requires Docker.                       |
| `clean`     | `rm -rf build`. You'll need to re-run `configure` after.                               |

### Windows

Requires you to manually install Qt with the binaries compiled with MSVC. When I last did this, there was no preset I could use, I had to customize the install to get the MSVC binaries.

Builds are found in `build\windows\bin\{Release|Debug}\hardlinkmgr.exe`. Required Qt .dlls are in the same folder.

### Linux

Installing dependencies depends on your linux flavor, but for Ubuntu 26.04, I did this:

```bash
sudo apt install git curl build-essential cmake ninja-build qt6-base-dev qt6-svg-dev qt6-wayland libgl1-mesa-dev
```

- `cmake` + `ninja-build` — the `linux-*` presets use the Ninja generator.
- `qt6-base-dev` — Qt Widgets/Network/Test development files (Qt 6.10 on 26.04); the Test module's CMake config ships in this package too, so no separate package is needed to build the `tests/` suites.
- `qt6-svg-dev` — Qt6::Svg development files (headers + CMake config), needed for the toolbar/action icons. `qt6-base-dev` only pulls in the runtime library (`libqt6svg6`), not this, so it must be listed explicitly.
- `qt6-wayland` — Qt's Wayland platform plugin, so the app runs natively on Ubuntu's default Wayland session.
- `libgl1-mesa-dev` — OpenGL headers, required when linking against Qt6::Gui.

Additionally, you'll need **Docker** with Compose v2 to run all the tests.

## Tests

The `tst_*` suites are excluded from the default `ALL` target (so an everyday `cmake --build` only builds the app), so build them explicitly before running `ctest`. Two build targets: `hlm_tests_unit` (just the serverless "unit"-labeled suites, matching `ctest ... -unit`) and `hlm_tests` (everything, matching `ctest ... -all`) — build the smaller one if that's all you're about to run, it skips compiling the docker-fixture suites:

```powershell
cmake --build --preset windows-debug --target tests/hlm_tests_unit --parallel
ctest --preset windows-unit    # serverless unit tests (< 1 s)

cmake --build --preset windows-debug --target tests/hlm_tests --parallel
ctest --preset windows-all     # everything, incl. integration/widget suites
```

(On Linux, drop the `tests/` prefix — e.g. `cmake --build --preset linux-debug --target hlm_tests --parallel`, then `linux-unit` / `linux-all`. Windows needs that prefix because CMake's Visual Studio generator can't resolve a bare target name for a target defined in a subdirectory; Linux's Ninja generator doesn't need it.)

The full run needs **Docker** with Compose v2: ctest builds and starts a Samba container (port 10445, share on a named volume), runs the SMB-backed suites against it, and tears it down. Without Docker on PATH those suites aren't registered and the unit tier still runs. See [`docs/testing.md`](./docs/testing.md) and [ADR 4](./docs/decisions/0004-automated-test-architecture.md).

## Cutting a release

`.github/workflows/release.yml` builds Windows + Linux artifacts and, when triggered by a tag, attaches them to a GitHub Release. There's no separate version file — the version lives in one place, and the release is just a tag that matches it:

1. Bump `VERSION` in the top-level `CMakeLists.txt`'s `project()` call (e.g. `VERSION 0.1.0` → `0.2.0`). This is the only place the version is defined — it flows into the Linux `.deb`'s filename via CPack (`CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT"`); the Windows zip's name (`hardlinkmgr-windows-x64.zip`) is unversioned by design, so nothing there needs touching.

2. Commit that change and push it to `main` (or merge it in) — the tag in the next step must point at a commit that already has the bumped version, or the built `.deb` will carry the old version number.

3. Tag the commit `vMAJOR.MINOR.PATCH`, matching the `CMakeLists.txt` value exactly (the workflow only triggers on a `v*` tag push; its `check-version` job compares the tag against `CMakeLists.txt`'s `VERSION` and fails fast — before the windows/linux jobs spend runner time — if they don't match):

   ```bash
   git tag v0.2.0
   git push origin v0.2.0
   ```

4. Pushing the tag triggers the `windows` and `linux` jobs, then (only for a `v*` tag, not `workflow_dispatch`) the `release` job, which downloads both artifacts and publishes a GitHub Release with the zip and `.deb` attached. Watch it under the repo's **Actions** tab.

5. If something's wrong with the build, delete the tag (locally and on the remote), fix it, and re-tag rather than reusing the same tag name:

   ```bash
   git tag -d v0.2.0
   git push origin :refs/tags/v0.2.0
   ```

To test the build+package steps without creating a release (e.g. to check CI still passes before tagging), run the workflow manually from the Actions tab (`workflow_dispatch`) — same jobs, but the `release` job is skipped since there's no `v*` tag.
