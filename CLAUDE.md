# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project state

Pre-implementation. The repo currently contains only `README.md` (the product vision) — no code, no build system, and **no language chosen yet** (see the README's TODO). Any technology decision below is still open; when one is made, replace this section with real build/test/run commands.

## What this app is

A GUI desktop app for manually deduplicating files on an SMB share by replacing near-duplicate files with hard links. The key difference from tools like `jdupes`: matches are **not** decided automatically. The user manually selects which files are "the same," picks a primary to keep, and the app replaces the others with hard links to it. This exists because the target files are often *slightly* different (including different sizes), so content-hashing tools miss them.

## Hard constraints (these drive the tech-stack choice — treat as requirements)

- **Instant startup** — no splash screen or loading bar.
- **Small binary + low memory footprint** — the README explicitly rules out Electron / a web rendering stack for this reason. Prefer native/lightweight GUI toolkits.
- **Cross-platform, in priority order:** Windows/amd64 and Linux/amd64 are required; macOS/arm is a nice-to-have, low priority.
- **Native look-and-feel** on each platform is a goal.
- Must operate over **SMB** (connect to `host_or_ip:port`) and manipulate **hard links + inode metadata** (hard link count, inode number are shown in the UI and are core to the feature). Verify the chosen SMB access approach actually exposes inode/link-count info and can create hard links on the share.

## UI model (from the README — the intended structure)

- **Main window toolbar:** SMB connect/disconnect control (connect → spinner/abort → disconnect states) and a "Link" button (enabled only when ≥2 files are selected across any file list).
- **One or more filesystem views**, each with its own path box, case-insensitive plain-text search filter, and a "matches / total" count label. Each view keeps the full file list in memory but only displays entries matching the filter. Folders sort to the top. Columns: icon, name, size, date modified, hard-link count, inode number.
- **Hard Link dialog:** lists the selected files, requires choosing the primary to keep, and on confirm replaces the non-primary files with hard links to the primary.
