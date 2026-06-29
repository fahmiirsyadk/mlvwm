<h1 align="center">MLVWM+</h1>

<img width="1920" height="1200" alt="screenshot-20260630-005935" src="https://github.com/user-attachments/assets/4462a660-4635-4df2-85f4-b415bc34ee4e" />


(note: this is fork, and it might be break on some machine. so DWYOR)

MLVWM (Macintosh-Like Virtual Window Manager) is an X11 window manager with a classic Mac OS 7/8/9 appearance. It provides a global menu bar, window decorations, virtual desktops, desktop icons, wallpaper management, and balloon help.

## Installation

### 1. Install Dependencies

#### Void Linux

```bash
sudo xbps-install -S gcc make imake libX11-devel libXext-devel libXpm-devel ImageMagick
```

#### Debian / Ubuntu

```bash
sudo apt install gcc make xutils-dev libx11-dev libxext-dev libxpm-dev imagemagick
```

#### Arch Linux

```bash
sudo pacman -S gcc make imake libx11 libxext libxpm imagemagick
```

### 2. Build & Install

```bash
cd mlvwm && xmkmf && make && sudo make install
```

### 3. Install the Session Entry

This registers MLVWM as a session option in your display manager (LightDM, GDM, etc.):

```bash
sudo cp install/mlvwm.desktop /usr/share/xsessions/
```

> **Note:** The desktop entry points to `/usr/local/bin/mlvwm`. If your `make install` placed the binary in `/usr/bin/`, edit the `Exec=` line accordingly.

### 4. Install the Configuration (mlvwmrc)

The bundled `mlvwmrc/` directory contains a complete, ready-to-use configuration with themes, app menus, icons, wallpapers, and helper scripts. This is the recommended setup.

```bash
cd mlvwmrc && make && make install
```

This does the following:

- Downloads menu bar mini-icons and converts them to XPM
- Downloads Mac OS wallpaper patterns
- Copies `.mlvwm/` to `$HOME/.mlvwm/`
- Symlinks `$HOME/.mlvwmrc` to `$HOME/.mlvwm/.mlvwmrc`
- Installs helper scripts (`mlvwm-powerdown`, `mlvwm-screenshot`, etc.) to `$HOME/bin/`

> **Requires:** `curl`, `convert` (ImageMagick), `unzip`

## Post-Install Setup

### Recommended Packages

| Package | Purpose |
|---------|---------|
| `pcmanfm` | File manager (has full MLVWM menu bar integration) |
| `xterm` | Terminal emulator (default terminal in config) |
| `xclock` | Swallowed into the menu bar as a clock widget |
| `xcalc` | Calculator, accessible from Apple menu |
| `xeyes` | Classic X11 toy |
| `ImageMagick` | Required for PNG/JPG desktop icons and wallpapers (provides `convert`) |
| `maim` | Screenshot tool (used by context menu and keyboard shortcut) |
| `scrot` | Fallback screenshot tool if `maim` is not installed |
| `feh` | Lightweight image viewer, useful for wallpaper testing |
| `xsetroot` | Sets root window color (used by `.initrc`) |
| `xclip` | Clipboard utility (useful with screenshots) |

Void Linux:
```bash
sudo xbps-install -S pcmanfm xterm xclock xcalc xeyes ImageMagick maim scrot feh xsetroot xclip
```

Debian / Ubuntu:
```bash
sudo apt install pcmanfm xterm x11-apps imagemagick maim scrot feh x11-xserver-utils xclip
```

Arch Linux:
```bash
sudo pacman -S pcmanfm xterm xorg-xclock xorg-xcalc xorg-xeyes imagemagick maim scrot feh xorg-xsetroot xclip
```

### Installing an Icon Theme (MoNine)

Desktop icons support XPM natively and PNG/JPG via ImageMagick conversion. The [MoNine](https://github.com/BlissThatMiss/MoNine) icon theme provides clean 32x32 icons that work well.

1. Download MoNine and extract it:
   ```bash
   mkdir -p ~/.icons
   # Extract MoNine to ~/.icons/MoNine/
   ```

2. Reference icons in your `.lnk` files:
   ```
   table Icon
     Icon: /home/youruser/.icons/MoNine/apps/32/firefox.png
     Caption: Firefox
     Command: firefox
     X: 10
     Y: 90
   end
   ```

3. Or use them when creating launchers via the desktop right-click menu (New Launcher prompts for an icon path).

### Installing Fonts

MLVWM uses X11 core fonts. The defaults work, but you can improve the look:

```bash
# Void Linux
sudo xbps-install -S font-misc-misc xorg-fonts

# Debian/Ubuntu
sudo apt install xfonts-base xfonts-100dpi xfonts-75dpi

# After installing, rebuild the font cache
sudo mkfontdir /usr/share/fonts/misc
xset fp rehash
```

To change fonts, edit `~/.mlvwm/.mlvwmrc` and uncomment/modify the font lines:

```
MenuBarFont -*-helvetica-medium-r-*-*-12-*-*-*-*-*-iso8859-*
MenuFont -*-helvetica-medium-r-*-*-12-*-*-*-*-*-iso8859-*
TitleBarFont -*-helvetica-bold-r-*-*-12-*-*-*-*-*-iso8859-*
```

### Menu Bar Widgets (Menu Extras)

The clock in the menu bar is powered by `xclock` swallowed into the bar. Enable or disable widgets in `~/.mlvwm/MenuBar`:

```
# Uncomment to add widgets
Read .mlvwm/MenuExtras/xload
Read .mlvwm/MenuExtras/xmem
```

Available menu extras: `X11` (window controls), `mlclock`, `oclock`, `xclock`, `xload`, `xmem`.

## Configuration Reference

All configuration lives under `~/.mlvwm/`. The main entry point is `~/.mlvwmrc` (symlinked to `~/.mlvwm/.mlvwmrc`).

### File Structure

```
~/.mlvwmrc                  -> ~/.mlvwm/.mlvwmrc (symlink)
~/.mlvwm/
  .mlvwmrc                  Main config (loads everything else via Read)
  .initrc                   Commands run on startup (set background, launch apps)
  .restartrc                Commands run on restart
  .Xdefaults                X resource imports (clock styling, etc.)
  MenuBar                   Global menu bar definition (File, Edit, View, Special)
  VirtualDesktops           Number of virtual desktops
  themes/
    System7                 Classic Mac System 7 look
    MacOS8                  Mac OS 8 look
    MacOS9                  Mac OS 8/9 look (recommended)
  apps/
    .AppsManifest           Loads all app-specific configs
    pcmanfm                 PCManFM menu bar and style
    firefox                 Firefox menu bar and style
    xterm                   XTerm menu bar and style
    ...                     (40+ app configs included)
  MenuExtras/
    X11                     Window Move/Resize/Kill menu
    xclock                  Digital clock widget
    xload                   CPU load widget
    ...
  pixmap/                   XPM icons for menu bar items
  patterns/                 Wallpaper images
  Xresources/               X resource overrides per app
```

### Themes

Change the theme in `~/.mlvwm/.mlvwmrc`:

```
# Choose one:
Read .mlvwm/themes/System7
Read .mlvwm/themes/MacOS8
Read .mlvwm/themes/MacOS9
```

The `MacOS9` theme enables `System8` mode (raised title bars, color decorations, opaque move/resize, one-click menus) and defines the Apple menu.

### Virtual Desktops

Edit `~/.mlvwm/VirtualDesktops`:

```
Desktopnum 4
```

Switch desktops with `Ctrl+Up` / `Ctrl+Down`.

### Startup Applications

Edit `~/.mlvwm/.initrc` to launch apps on login:

```
InitFunction
Exec "xsetroot" xsetroot -grey &
Desk 0
Exec "xterm" xterm -ls -geometry 80x25 &
Wait xterm
Exec "pcmanfm" pcmanfm &
END
```

### Keyboard Shortcuts

Default shortcuts (defined in the `ShortCut` block in `.mlvwmrc`):

| Shortcut | Action |
|----------|--------|
| `Ctrl+Up` | Previous desktop |
| `Ctrl+Down` | Next desktop |
| `Ctrl+Left` | Previous window |
| `Ctrl+Right` | Next window |
| `Alt+Left` | Previous window (same desktop) |
| `Alt+Right` | Next window (same desktop) |
| `Alt+Escape` | Restart MLVWM |
| `Shift+Alt+Escape` | Log out (exit) |
| `Shift+Ctrl+3` | Screenshot (full screen, saved to `~/Pictures/`) |
| `Shift+Ctrl+4` | Screenshot (selection, saved to `~/Pictures/`) |

Shortcut modifiers: `C` = Ctrl, `M` = Alt, `S` = Shift, `N` = None. You can add custom shortcuts in `~/.mlvwm/.mlvwmrc`:

```
ShortCut
Up    C    Desk -
Down  C    Desk +
Left  C    PreviousWindow
Right C    NextWindow
Left  M    PreviousSameDeskWindow
Right M    NextSameDeskWindow
Escape M   Restart mlvwm
Escape SM  Exit
3     SC   Exec "mlvwm-screenshot" exec ./bin/mlvwm-screenshot
4     SC   Exec "mlvwm-screenshot" exec ./bin/mlvwm-screenshot -s
END
```

Available shortcut actions: `Desk +/-`, `NextWindow`, `PreviousWindow`, `NextSameDeskWindow`, `PreviousSameDeskWindow`, `Restart`, `Exit`, `Exec`, `MoveWindow`, `ResizeWindow`, `Refresh`, `KillWindow`.

### Adding an Application Menu

Each app can have its own menu bar. Create a file in `~/.mlvwm/apps/` and add it to `.AppsManifest`.

Example (`~/.mlvwm/apps/myapp`):

```
Menu MyApp-File, Label "File", Left
"New" Action SendMessage C+N
"Quit" Action SendMessage C+Q
END

MenuBar MyApp
MyApp-File
Default-Edit
END

Style
"*MyApp*" MiniIcon mini-x.xpm, MenuBar MyApp
END
```

Then add to `~/.mlvwm/apps/.AppsManifest`:

```
Read .mlvwm/apps/myapp
```

### Window Styles

Control per-window behavior in app config files or `.mlvwmrc`:

```
Style
"*Firefox*" MaxmizeScale 90, MiniIcon firefox.xpm, MenuBar Firefox
"*XTerm*" NoSBarH, NoSBarV, MiniIcon mini-sh.xpm, MenuBar xterm
END
```

Available style flags: `NoTitle`, `NoCloseR`, `NoMinMaxR`, `NoResizeR`, `NoSBarH`, `NoSBarV`, `NoWinList`, `Sticky`, `SkipSelect`, `MaxmizeScale`, `MiniIcon`, `MenuBar`, `EnableScroll`, `IconifyShade`.

## Desktop Icons

The desktop supports idesk-compatible `.lnk` files in `~/.idesktop/`.

### .lnk File Format

```
table Icon
  Icon: /path/to/icon.png
  Caption: My App
  Command: myapp
  X: 100
  Y: 200
end
```

- **Icon** accepts XPM files natively, or PNG/JPG if ImageMagick is installed
- **X/Y** is the pixel position on the desktop
- Icons can also be created via right-click -> "New Launcher"

### Desktop Right-Click Menu

**On empty area:**

- **New Launcher** -- prompts for name, command, and icon path
- **Change Background** -- submenu listing images from wallpaper directories, a "Browse..." option for manual path entry, and aspect mode selectors (Scale, Center, Tile, Fit)
- **Take Screenshot** -- submenu with "Full Screen" and "Selection" (uses `maim`, falls back to `scrot`; saves to `~/Pictures/`)
- **Arrange Icons** -- auto-layouts all icons into a grid (columns, top-to-bottom)
- **Refresh Desktop** -- reloads all icons and wallpaper

**On an icon:**

- **Open** -- launches the icon's command
- **Rename** -- prompts for a new caption (pre-filled with current name)
- **Delete Icon** -- removes the `.lnk` file and icon from the desktop

### Wallpaper

Wallpapers are scanned from `~/.mlvwm/wallpapers/`, `~/Pictures/`, and `~/.mlvwm/patterns/`. Selection persists in `~/.mlvwm/wallpaper.conf`.

Aspect modes (selectable from the Change Background submenu):

| Mode | Behavior |
|------|----------|
| Scale (stretch) | Stretches to fill screen, ignores aspect ratio |
| Center | Places image at center, no scaling |
| Tile | Repeats image across the screen |
| Fit | Scales to fit within screen, preserves aspect ratio |

## Testing with Xephyr

Test MLVWM without logging out of your current session:

```bash
Xephyr :32 -screen 1024x768 &
DISPLAY=:32 mlvwm
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-f config_file` | Use specified configuration file |
| `-d display_name` | Run on specified display |
| `-debug` | Enable debug output |

## Documentation

- Manual page: `man mlvwm`
- Configuration reference: [CONFIGURATION](CONFIGURATION) ([Japanese](CONFIGURATION.jp))
- Changelog: [CHANGELOG](CHANGELOG)
- Original docs: [README](README) ([Japanese](README.jp))

## History

MLVWM was originally developed in 1997 by Takashi HASEGAWA, based on FVWM, while studying at Nagoya University. In 2020, Morgan Aldridge obtained permission to continue maintenance and development.

## License

Distributed as freeware. Original copyright must remain in source code and documentation. Some files retain their original MIT license.

Macintosh and MacOS are registered trademarks of Apple, Inc.
