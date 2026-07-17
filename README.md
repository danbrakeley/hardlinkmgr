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

- [milestones.md](./docs/milestones.md) - The milestones to get the POC behavior up and running
- [roadmap.md](./docs/roadmap.md) - Where this app is heading
- [ADRs](./docs/decisions/) - Architectural Decision Records
- [testing.md](./docs/testing.md) - List of intended behavior, in checklist form

## Build

On every platform, libsmb2 is pulled from the patched fork via CMake
`FetchContent`, so no manual checkout is needed.

### Windows

Uses the MSVC Qt kit (see `docs/decisions/0001-choose-project-language.md`).
Qt is found from the installed kit via the `windows` preset.

First-time build (or any time after editing `CMakeLists.txt`), configure then build:

```powershell
cmake --preset windows
cmake --build --preset windows-release
# -> build\windows\bin\Release\hardlinkmgr.exe
```

For day-to-day builds where only source has changed, just rebuild:

```powershell
cmake --build --preset windows-release
```

Use `windows-debug` in place of `windows-release` for a debug build.

The exe runs standalone: a post-build `windeployqt` step stages Qt's runtime DLLs
and plugins next to it (it re-runs each build but no-ops when they're current), so
no need to have Qt on `PATH`.

### Linux

Tested on Ubuntu 26.04. One-time setup on a fresh system (on top of `git`,
`curl`, and `build-essential`):

```bash
sudo apt install cmake ninja-build qt6-base-dev qt6-wayland libgl1-mesa-dev
```

- `cmake` + `ninja-build` — the `linux-*` presets use the Ninja generator.
- `qt6-base-dev` — Qt Widgets/Network development files (Qt 6.10 on 26.04).
- `qt6-wayland` — Qt's Wayland platform plugin, so the app runs natively on
  Ubuntu's default Wayland session.
- `libgl1-mesa-dev` — OpenGL headers, required when linking against Qt6::Gui.

Configure then build (reconfigure only after `CMakeLists.txt` edits):

```bash
cmake --preset linux-release
cmake --build --preset linux-release
# -> build/linux-release/bin/hardlinkmgr
```

Use `linux-debug` in place of `linux-release` for a debug build; unlike the
multi-config Windows preset, each Linux preset is single-config with its own
build directory, so both coexist. Qt links dynamically against the system
packages and the binary runs in place — no deploy step.
