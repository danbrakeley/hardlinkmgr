---
date: 2026-07-28
---

# Fix the File Browser to Two Views, Remove Add/Remove-View

## Context and Problem Statement

Milestone 5 gave the file browser arbitrary add/remove views on a splitter: an "Add View" toolbar action, and a per-view close button, so the user could stack as many views as they wanted and manually navigate each one to compare candidate files. That was the only tool in the app for finding matches at the time.

Post-POC, the Match Finder (`docs/decisions/0002-metadata-only-match-finder.md`) replaced that manual workflow: it searches a primary and secondary path and hands back a reviewable candidate list, and selecting a result reveals the primary file in the first view and the secondary in the second (`FileBrowserView::navigateToAndReveal`, wired from `MainWindow::onRevealRequested`). In practice, a third or fourth view never got used — every real session is "one primary tree, one secondary tree," and the Match Finder now drives both views directly. Should the app keep supporting arbitrary add/remove views?

## Decision Drivers

- **Match the actual workflow.** The Match Finder is now how matches get found; it only ever addresses two views (primary/secondary). Arbitrary extra views serve the old manual-comparison workflow the Match Finder was built to replace.
- **Less UI, less code to keep correct.** Add/remove-view UI means splitter child-count bookkeeping, per-view close-button visibility toggling, and `MainWindow::m_views` needing to stay in sync with a mutable splitter — all in service of a capability nobody was reaching for.
- **`onRevealRequested` already assumes two.** It reveals into `m_views[0]`/`m_views[1]` unconditionally; the only add/remove interaction it needs is "make sure a second view exists," not "manage N views."

## Considered Options

- **Fix the file browser to exactly two views, remove add/remove-view UI** (chosen)
- **Keep arbitrary views** (status quo from milestone 5)
- **Keep add/remove UI but default to two** (compromise)

## Decision Outcome

Chosen option: **fix to two views**, because the Match Finder's two-tree model is now the app's actual usage pattern, and every observed use of the add/remove-view UI was either "get back to two" or unused entirely — keeping the general mechanism only pays for complexity with no matching benefit.

`MainWindow` now creates exactly two `FileBrowserView`s on connect and never adds or removes any afterward; `FileBrowserView` no longer has a close button. `docs/milestones.md`'s milestone 5 description and `CLAUDE.md`'s UI-model notes should be read as historical for the "add/remove views" phrase — the current behavior is a fixed pair.

## More Information

- Supersedes the "add/remove views" part of milestone 5 (`docs/milestones.md`) — the remembered-URL and GUI-executable parts of that milestone are unaffected.
- Revisit if: a future workflow needs more than two trees open at once
