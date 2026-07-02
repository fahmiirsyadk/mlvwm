# PRD — mlvwm+ Modernization

**Project:** mlvwm+ (fork of mlvwm 0.9.4)
**Author:** fahmiirsyadk
**Status:** Draft
**Last updated:** 2026-07-02

---

## 1. Overview

mlvwm+ revives and modernizes MLVWM (Macintosh-Like Virtual Window Manager, 1997), preserving its authentic classic Mac OS look while making it a practical daily-driver window manager on modern Linux/BSD systems. The fork already added a desktop icon manager with wallpaper support, EWMH desktop/workspace basics, crash logging, and a menu-bar workspace pager (in progress). This PRD defines the next stage: hardening the 1997 core, making modern applications behave correctly, and shipping the signature Mac OS 9 features that define the project's identity.

## 2. Vision & Goals

**Vision:** the most faithful classic Mac OS desktop experience available on X11 — not a theme on top of a generic WM, but a window manager whose behaviors (menu bar, desktop, Control Strip, Trash) are the real thing.

**Goals**
- G1: No crashes or memory-unsafe behavior from config files or client-supplied X properties.
- G2: Modern applications (GTK/Qt, video players, panels) work correctly without workarounds.
- G3: Ship signature classic Mac features: Trash, Control Strip, window snapping, keyboard app switching.
- G4: Anyone can build and install with `make` on any mainstream distro; CI guards every commit.

**Non-goals**
- Wayland support.
- Built-in compositing/transparency (external compositors like picom must work, and do).
- A file manager (desktop shows launchers/Trash; full file management delegates to `xdg-open`).
- Pixel-perfect recreation of every OS 9 control panel.

## 3. Users

- **Retro-computing enthusiasts** who want the classic Mac experience as a daily desktop.
- **Minimalist WM users** on older/low-power hardware (the WM is ~11k lines of C, Xlib + Xpm only).
- **Distro packagers** (Void, Arch, NetBSD, NixOS already package upstream) who need a sane build.

## 4. Current State (audit summary, July 2026)

| Area | State |
|---|---|
| Codebase | ~11k lines C, 11 files; core from 1997, `desktop.c` (2025) is modern quality |
| Safety | 1 client-triggerable stack overflow (`event.c:1650`); `calloc(strlen-N)` underflows in config parser; ~44 unchecked allocations; ~28 unbounded `sprintf` |
| EWMH | Desktops, client list, active window, supporting-WM-check ✔ — `_NET_WM_STATE` (fullscreen/maximize), struts, UTF-8 titles, `WM_TAKE_FOCUS` ✘ |
| Input | Shortcuts break with NumLock/CapsLock active |
| Build | Imake-only; `install.sh` broken (references missing files, Void-only); build artifacts committed; version drift (0.9.3/0.9.4/0.9.x); no CI |
| Uncommitted | Workspace pager feature complete in working tree, needs testing + commit |

## 5. Requirements

Priorities: **P0** = must ship next release; **P1** = should ship; **P2** = nice to have.

### 5.1 Stability & safety (P0)

| ID | Requirement | Acceptance criteria |
|---|---|---|
| S1 | No client-controlled buffer overflows | Setting a huge `_XA_WM_DESKTOP` value via `xprop` does not crash; all `sprintf` replaced with bounded `snprintf` |
| S2 | Config parser survives malformed input | Empty lines, 1-char lines, and lines shorter than their keyword parse or are rejected without crash/UB (`fgetline`, `calloc(strlen-N)` sites) |
| S3 | Allocation failures are fatal-with-message, not NULL-deref | `xmalloc`/`xcalloc` wrappers used at all allocation sites |
| S4 | Desktop cache is per-user and unpredictable | Cache moves from `/tmp/mlvwm-desktop-cache` to `$XDG_CACHE_HOME/mlvwm/` |
| S5 | Workspace pager lands cleanly | Pager clicks switch desktops; disabled/single-desktop configs unaffected; committed to master |

### 5.2 Signature features

#### F1 — Trash can (P0)
The classic Mac Trash on the desktop, integrated with the freedesktop trash spec.
- Trash icon anchored bottom-right of the desktop; distinct empty/full icon states.
- Dragging a desktop icon over the Trash highlights it; dropping a launcher deletes its `.lnk` (with the file going to trash, not `unlink`).
- Trashed files follow the freedesktop spec (`~/.local/share/Trash/files/` + `.trashinfo`) so `gio trash --list` and file managers see them.
- Context menu: **Open Trash** (opens trash dir via `xdg-open`), **Empty Trash** (confirmation dialog; permanently deletes).
- *Acceptance:* drag a launcher onto Trash → icon disappears, `gio trash --list` shows the file, Trash icon shows "full"; Empty Trash → icon returns to empty state.

#### F2 — Control Strip (P1)
OS 9's collapsible module strip, docked at the bottom-left screen edge.
- Platinum-styled strip; tab handle collapses to a nub / expands on click; draggable vertically along the left edge.
- Built-in modules: **desktop switcher** (reuses workspace-pager logic), **clock** (updates ≤1 s drift using the existing event-loop timeout), **volume** (via `amixer`/`pactl`).
- User modules: `ControlStripModule "label" "command"` config lines add clickable command cells.
- Config: `ControlStrip` keyword enables it; documented in `CONFIGURATION` and sample rc.
- *Acceptance:* strip renders, collapses/expands, survives desk switches and restarts; each module responds to click.

#### F3 — Alt-Tab application switcher (P1)
- Alt+Tab / Alt+Shift+Tab cycles windows forward/back; a centered platinum panel shows mini-icons + window names with the current selection highlighted.
- Selection activates (focus + raise + desk switch if needed) when the modifier is released.
- Works regardless of NumLock/CapsLock state (depends on requirement C5).
- *Acceptance:* cycling works across desktops with 0, 1, and many windows; no stuck grabs.

#### F4 — Window edge snapping (P2)
- While dragging, nearing the left/right screen edge previews a half-screen snap target and the top edge previews maximize, drawn with the classic XOR outline; geometry applies on release.
- Opt-in via `EdgeSnap <threshold-px>` config; default off (stock behavior preserved).
- *Acceptance:* snap triggers only within threshold; Escape or dragging away cancels; disabled by default.

### 5.3 Modern-app compatibility (P0–P1)

| ID | Requirement | Priority |
|---|---|---|
| C1 | UTF-8 window titles: read `_NET_WM_NAME` with `WM_NAME` fallback, live-update on PropertyNotify | P0 |
| C2 | `_NET_WM_STATE`: FULLSCREEN, MAXIMIZED_VERT/HORZ, ABOVE/BELOW, HIDDEN; plus `_NET_CLOSE_WINDOW` | P0 |
| C3 | `_NET_WM_STRUT(_PARTIAL)`: panels reserve space; maximize/placement/`_NET_WORKAREA` respect it | P1 |
| C4 | Send `WM_TAKE_FOCUS` to clients that request it; react to `WM_HINTS` changes (urgency) | P1 |
| C5 | Keyboard shortcuts work with NumLock/CapsLock active (grab permutations) | P0 |
| C6 | `_NET_WM_WINDOW_TYPE_DOCK` (undecorated, unfocused) and `_DIALOG` (transient treatment) | P1 |

*Acceptance:* `wmctrl -m/-l` report correctly; `mpv --fullscreen` fullscreens and restores; a GTK app shows a non-ASCII title correctly; tint2/polybar reserve screen space; shortcuts fire with NumLock on.

### 5.4 Build, packaging, CI (P1)

| ID | Requirement |
|---|---|
| B1 | Plain hand-written `Makefile` using pkg-config (`x11 xext xpm`); Imake files removed after one transition release |
| B2 | `install.sh` works on non-Void distros; no references to files absent from the repo |
| B3 | GitHub Actions: build with `-Wall -Wextra -Werror` on every push/PR |
| B4 | Single version source (bump to 0.9.5); `*.o`/binary artifacts untracked; `CONFIGURATION` mojibake fixed |
| B5 | The System7/MacOS8/MacOS9 theme bundles advertised in README.md are committed to the repo |

### 5.5 Rendering & platform (P2, optional compile flags)

| ID | Requirement |
|---|---|
| R1 | `USE_XFT`: antialiased text via Xft/fontconfig (text drawing is centralized in the `XDRAWSTRING` macro + one extents helper) |
| R2 | `USE_IMLIB2`: native PNG/JPG icon & wallpaper loading, removing the ImageMagick `convert` dependency |
| R3 | `USE_XRANDR`: multi-monitor awareness — menu bar/Control Strip on primary, per-monitor maximize/snap/placement |

All three must remain optional: a bare Xlib+Xpm build stays supported.

## 6. Backlog (recorded, unscheduled)

Pop-up tabbed windows (drag to bottom edge → title tab); platinum sound effects; startup splash; "Hide Others / Show All" in the application menu; real `~/Desktop` file icons; rubber-band multi-select; volume/disk desktop icons; XDND drop targets; `.desktop` launcher format; icon themes; keyboard desktop navigation; standalone menu-bar clock (superseded by the Control Strip clock module).

## 7. Milestones

| Milestone | Contents | Exit criteria |
|---|---|---|
| M0 | Land workspace pager (S5) | Committed, tested in Xephyr |
| M1 — "Solid" | S1–S4, C1, C2, C5 | Fuzz-ish config/property tests pass; mpv fullscreen works |
| M2 — "Signature" | F1 Trash, F2 Control Strip, F3 Alt-Tab | Acceptance criteria above, demo GIFs for README |
| M3 — "Packaged" | B1–B5, C3, C4, C6, F4 | CI green; fresh-distro install via `make install` |
| M4 — "Polished" | R1–R3 | Optional-flag builds green in CI matrix |
| Release | Tag **v0.9.5** after M3; **v1.0.0** candidate after M4 | |

## 8. Success metrics

- Zero crash reports from config/property handling after M1 (crash log `~/.mlvwm-crash.log` telemetry-by-issue).
- Modern-app checklist (mpv, Firefox, a GTK4 app, tint2, wmctrl) fully green after M1/M3.
- Build-from-source success on Void, Arch, Debian, NetBSD without imake after M3.
- README screenshots/GIFs showing Trash + Control Strip after M2 (the project's marketing moment).

## 9. Risks & mitigations

- **Grab handling regressions (Alt-Tab, NumLock permutations):** X grabs are easy to get stuck; test in Xephyr, always pair grab/ungrab in error paths, keep the existing `Scr.ErrorFunc` breadcrumb pattern.
- **EWMH state interactions with 1997 geometry code:** fullscreen/maximize must round-trip with shade/zoom; add state to `MlvwmWindow.flags` rather than parallel structures.
- **Scope creep on Control Strip:** ship with three built-in modules + command cells; module SDK/plugins are out of scope.
- **Freedesktop trash spec edge cases** (cross-filesystem moves): fall back to copy+unlink; document.
- **Imake removal breaking downstream packagers:** keep Imakefile one release with a deprecation note in CHANGELOG.

## 10. Open questions

- Should `System8` flag be renamed/split into a proper theme selector (`Appearance System7|Platinum`) when theme bundles land (B5)?
- Trash for real files vs launcher-only at M2 — launcher-only is acceptable for v0.9.5 if `~/Desktop` file icons remain backlog.
- Minimum supported X server age (affects how defensively R1–R3 flags must degrade).
