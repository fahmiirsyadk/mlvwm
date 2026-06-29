# mlvwm Desktop Integration — Plan

## Completed

- [x] **Ghost trail fix** — `icon_bbox()` computes the union bounding box of icon + caption; drag clears both old and new extents so wide captions don't leave artifacts.
- [x] **Native context menu** — replaced homegrown menu with the native mlvwm menu engine (`menus.c`). Right-click on empty area shows "New Launcher / Change Background / Refresh Desktop"; right-click on icon shows "Open / Delete Icon". Styled automatically via the SYSTEM8 (Mac OS 9) theme flag.
- [x] **Desktop click unfocus** — clicking the desktop deactivates the focused window and resets the menu bar (`SetFocus(NULL)`).
- [x] **Single-click menu** — context menu opens on right-click release, not press-hold-drag. `PopupMenu` `held` parameter controls ignore mode.
- [x] **Configurable New Launcher** — prompts for name + command via inline modal dialog (`prompt_text`), writes `.lnk` file, reloads desktop.
- [x] **Wallpaper changing** — scans `~/.mlvwm/wallpapers`, `~/Pictures`, `~/.mlvwm/patterns` for images. Persists selection in `~/.mlvwm/wallpaper.conf`. Supports XPM natively and PNG/JPG/etc via ImageMagick `convert`.
- [x] **Wallpaper submenu** — "Change Background" is a proper submenu (not a separate popup). Both menus visible side-by-side, like other desktop environments. Fixed `RedrawMenu` `LabelWin == None` guard to enable submenus on popup menus. "Browse..." fallback for manual path entry.
- [x] **Debug logging cleanup** — removed all temporary `dlog()` and `ChoiseMenu` fprintf instrumentation.
- [x] **`$HOME` portability** — replaced all hardcoded `/home/void` paths with `getenv("HOME")` via `home_dir()` helper. Desktop now works for any user.
- [x] **Handle missing ImageMagick** — `convert_to_xpm()` checks for `convert` once at startup; returns NULL gracefully if missing. Non-XPM icons get placeholder, XPM icons still work.
- [x] **Icon arrangement / snap-to-grid** — 90×80 grid cells. Icons snap to nearest cell on drag release. "Arrange Icons" context menu item auto-layouts icons in Mac OS 9-style columns (top-to-bottom, left-to-right).
- [x] **Icon label editing** — right-click icon → "Rename" opens `prompt_text` dialog pre-filled with current caption. Updates `.lnk` file on confirm.
- [x] **Drag-and-drop visual feedback** — XOR outline rectangle follows cursor during drag instead of moving the icon directly. On release, icon snaps to grid at outline position.
- [x] **Wallpaper aspect modes** — four modes: Scale (stretch), Center, Tile, Fit. Selectable from wallpaper submenu. Mode persists in `wallpaper.conf` (second line). ImageMagick commands adapted per mode.
- [x] **Screenshot context menu** — "Take Screenshot" submenu with "Full Screen" and "Selection" options. Uses `maim` with `scrot` fallback. Saves timestamped PNG to `~/Pictures/`.

## Next — Polish & Features

### Medium priority (robustness)

- [ ] **Multi-monitor / Xinerama** — desktop window spans all screens; icons cluster on primary. Should detect per-monitor geometry.
- [ ] **`.desktop` file support** — standard freedesktop `.desktop` files in addition to idesk `.lnk` format.

### Low priority (nice-to-have)

- [ ] **Icon themes** — load icons from a freedesktop icon theme (`/usr/share/icons/`) by name instead of absolute paths.
- [ ] **Desktop file manager integration** — double-clicking empty area or a "Files" icon could open the file manager at `~/Desktop`.
- [ ] **Auto-start / session items** — launch apps listed in a config on desktop init.
- [ ] **Keyboard navigation** — arrow keys to move between icons, Enter to launch, Delete to remove.

## Architecture Notes

- Desktop window: `_NET_WM_WINDOW_TYPE_DESKTOP`, created in `InitDesktop()`, events routed via `HandleDesktopEvent()`.
- Menu actions use `IDesk*` prefix to avoid `ExecuteFunction` prefix-match collision (see `functions.c` `strncmp` with no `break`).
- Modal dialogs (New Launcher prompt, Browse wallpaper prompt) run **after** `PopupMenu` returns via `pending_*` flags — never inside the menu's grab loop.
- Submenus on popup menus work now that `RedrawMenu` has the `LabelWin == None` guard.
- Build: `cd mlvwm && make && sudo make install`.
