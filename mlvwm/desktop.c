/*
 * mlvwm/desktop.c - Integrated desktop icon manager
 *
 * Architecture based on OXWM's desktop.c:
 * - Desktop window is created and managed by the WM
 * - Events on the desktop are handled directly by the WM (no focus conflict)
 * - Built-in context menu, wallpaper, icons
 *
 * Config format: idesk-compatible (.idesktop/*.lnk files)
 *   table Icon
 *     Icon: /path/to/icon.xpm
 *     Caption: Name
 *     Command: command
 *     X: 50
 *     Y: 50
 *   end
 *
 * Rendering: X11 core fonts (XLoadQueryFont) + Xpm for icons
 * (no Xft/Imlib2 needed)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/xpm.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "mlvwm.h"
#include "screen.h"
#include "desktop.h"
#include "functions.h"
#include "add_window.h"
#include "menus.h"
#include "borders.h"

#define MAX_DESKTOP_ICONS 64
#define ICON_SIZE 48
#define DRAG_THRESHOLD 5
#define CLICK_DELAY 500
#define GRID_W 90
#define GRID_H 80
#define GRID_PAD_X 10
#define GRID_PAD_Y 10

static const char *home_dir(void) {
    static const char *h = NULL;
    if (!h) h = getenv("HOME");
    if (!h) h = "/tmp";
    return h;
}

typedef struct {
    char caption[64];
    char icon[256];
    char command[256];
    char lnk_path[512];
    int x, y;
    Pixmap pixmap;
    Pixmap mask;
    int pixmap_w, pixmap_h;
    int selected;
} DeskIcon;

static DeskIcon icons[MAX_DESKTOP_ICONS];
static int n_icons = 0;
static Window desktop_win = None;
static GC gc = None;
static XFontStruct *font = NULL;
static Pixmap wallpaper_pm = None;
static Atom a_xrootpmap, a_esetroot;
static Atom a_net_wm_window_type, a_net_wm_window_type_desktop;
static Atom a_net_wm_state, a_net_wm_state_below, a_net_wm_state_sticky;
static Atom a_net_wm_state_skip_pager, a_net_wm_state_skip_taskbar;
static int sw, sh;

/* Drag state */
static int drag_idx = -1;
static int drag_x, drag_y;
static int drag_moved = 0;
static int drag_orig_x, drag_orig_y;

/* Click tracking */
static Time last_click_time = 0;
static int last_click_x = -1, last_click_y = -1;
static int click_count = 0;

/* Context menu (native mlvwm menu engine) */
static MenuLabel desk_menu;
static MenuLabel wall_menu;          /* wallpaper picker popup */
static MenuLabel shot_menu;          /* screenshot submenu */
static int menu_open_x, menu_open_y; /* where the context menu was opened */
static int pending_new = 0;          /* deferred: open New Launcher dialog */
static int pending_bg = 0;           /* deferred: open wallpaper picker */
static int pending_rename = -1;      /* deferred: rename icon at this index */

enum { WP_SCALE = 0, WP_CENTER, WP_TILE, WP_FIT };
static int wallpaper_mode = WP_SCALE;
static char current_wallpaper[1024];

static const char *wallpaper_conf_path(void) {
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/.mlvwm/wallpaper.conf", home_dir());
    return buf;
}

static const char **wall_dir_list(void) {
    static char b0[512], b1[512], b2[512];
    static const char *dirs[4];
    static int built = 0;
    if (!built) {
        snprintf(b0, sizeof(b0), "%s/.mlvwm/wallpapers", home_dir());
        snprintf(b1, sizeof(b1), "%s/Pictures", home_dir());
        snprintf(b2, sizeof(b2), "%s/.mlvwm/patterns", home_dir());
        dirs[0] = b0; dirs[1] = b1; dirs[2] = b2; dirs[3] = NULL;
        built = 1;
    }
    return dirs;
}

/* Forward declarations for helpers referenced before their definition */
static void redraw_icons(void);

/* Trim whitespace in-place */
static char *trim(char *s) {
    char *e;
    while (*s && isspace(*s)) s++;
    e = s + strlen(s) - 1;
    while (e > s && isspace(*e)) { *e = '\0'; e--; }
    return s;
}

/* Quote a string for safe inclusion in a /bin/sh command line.  Wraps the
 * whole string in single quotes, turning each embedded ' into the sequence
 * '\''.  This neutralises shell metacharacters in file/icon paths that the
 * user controls (.lnk files, the Browse dialog, $HOME), so they can never
 * break out of the quoting and inject commands into system()/sh -c.
 *
 * Writes a NUL-terminated result into out (capacity outsz) and returns it.
 * If the quoted form would not fit, out is set to "''" (an empty argument)
 * so the surrounding command fails to find its file rather than running an
 * attacker-controlled fragment. */
static const char *shell_quote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    if (outsz < 3) { if (outsz) out[0] = '\0'; return out; }
    out[o++] = '\'';
    for (; *in; in++) {
        if (*in == '\'') {
            if (o + 5 >= outsz) goto overflow;  /* '\'' (4) + closing ' (1) */
            out[o++] = '\''; out[o++] = '\\'; out[o++] = '\''; out[o++] = '\'';
        } else {
            if (o + 2 >= outsz) goto overflow;  /* char + closing ' */
            out[o++] = *in;
        }
    }
    out[o++] = '\'';
    out[o] = '\0';
    return out;
overflow:
    out[0] = '\''; out[1] = '\''; out[2] = '\0';
    return out;
}

/* Find icon at point */
static int hit_test(int x, int y) {
    int i;
    for (i = n_icons - 1; i >= 0; i--) {
        int w = icons[i].pixmap_w > 0 ? icons[i].pixmap_w : ICON_SIZE;
        int h = icons[i].pixmap_h > 0 ? icons[i].pixmap_h : ICON_SIZE;
        if (x >= icons[i].x && x < icons[i].x + w &&
            y >= icons[i].y && y < icons[i].y + h)
            return i;
    }
    return -1;
}

/* Load a single .lnk file (idesk format) */
static int load_lnk(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (n_icons >= MAX_DESKTOP_ICONS) { fclose(f); return -1; }
    DeskIcon *ic = &icons[n_icons];
    memset(ic, 0, sizeof(*ic));
    strncpy(ic->lnk_path, path, sizeof(ic->lnk_path) - 1);
    ic->x = 50; ic->y = 50;

    char line[512];
    int in_table = 0;
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (strncmp(s, "table Icon", 10) == 0) { in_table = 1; continue; }
        if (strncmp(s, "end", 3) == 0) break;
        if (!in_table) continue;
        if (strncmp(s, "Icon:", 5) == 0) {
            sscanf(s + 5, " %[^\n]", ic->icon);
        } else if (strncmp(s, "Caption:", 8) == 0) {
            sscanf(s + 8, " %[^\n]", ic->caption);
        } else if (strncmp(s, "Command:", 8) == 0) {
            sscanf(s + 8, " %[^\n]", ic->command);
        } else if (strncmp(s, "X:", 2) == 0) {
            sscanf(s + 2, " %d", &ic->x);
        } else if (strncmp(s, "Y:", 2) == 0) {
            sscanf(s + 2, " %d", &ic->y);
        }
    }
    fclose(f);
    if (ic->caption[0] == '\0') return -1;
    return n_icons++;
}

/* Save .lnk file */
static void save_lnk(int idx) {
    if (idx < 0 || idx >= n_icons) return;
    DeskIcon *ic = &icons[idx];
    FILE *f = fopen(ic->lnk_path, "w");
    if (!f) return;
    fprintf(f, "table Icon\n");
    fprintf(f, "  Icon: %s\n", ic->icon);
    fprintf(f, "  Caption: %s\n", ic->caption);
    fprintf(f, "  Command: %s\n", ic->command);
    fprintf(f, "  X: %d\n", ic->x);
    fprintf(f, "  Y: %d\n", ic->y);
    fprintf(f, "end\n");
    fclose(f);
}

/* Load all icons from ~/.idesktop/ */
static void load_icons(void) {
    char d0[512], d1[512];
    const char *dirs[3];
    snprintf(d0, sizeof(d0), "%s/.idesktop", home_dir());
    snprintf(d1, sizeof(d1), "%s/.config/idesktop", home_dir());
    dirs[0] = d0; dirs[1] = d1; dirs[2] = NULL;
    int i;
    for (i = 0; dirs[i]; i++) {
        DIR *d = opendir(dirs[i]);
        if (!d) continue;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL && n_icons < MAX_DESKTOP_ICONS) {
            const char *dot = strrchr(ent->d_name, '.');
            if (!dot || strcmp(dot, ".lnk") != 0) continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dirs[i], ent->d_name);
            load_lnk(path);
        }
        closedir(d);
    }
}

/* Convert any image to XPM using ImageMagick and cache it.
 * wp_mode: -1 = icon (preserve aspect), WP_SCALE..WP_FIT for wallpaper. */
static char *convert_to_xpm(const char *src, int w, int h, int wp_mode) {
    static char xpm_path[1024];
    static int have_convert = -1;
    if (have_convert == -1)
        have_convert = (system("command -v convert >/dev/null 2>&1") == 0);
    if (!have_convert) return NULL;
    mkdir("/tmp/mlvwm-desktop-cache", 0755);
    char hash[64];
    unsigned long hh = 0;
    int i;
    for (i = 0; src[i]; i++) hh = hh * 31 + (unsigned char)src[i];
    snprintf(hash, sizeof(hash), "%lu_%dx%d_%d.xpm", hh, w, h, wp_mode);
    snprintf(xpm_path, sizeof(xpm_path),
             "/tmp/mlvwm-desktop-cache/%s", hash);
    struct stat st_src, st_xpm;
    if (stat(xpm_path, &st_xpm) == 0 && stat(src, &st_src) == 0 &&
        st_xpm.st_mtime >= st_src.st_mtime) {
        return xpm_path;
    }
    char cmd[4096];
    char qsrc[2048];
    shell_quote(src, qsrc, sizeof(qsrc));
    switch (wp_mode) {
    case WP_SCALE:
        snprintf(cmd, sizeof(cmd),
                 "convert %s -resize %dx%d! '%s' 2>/dev/null",
                 qsrc, w, h, xpm_path);
        break;
    case WP_FIT:
        snprintf(cmd, sizeof(cmd),
                 "convert %s -resize %dx%d -background '#D4D0C8' "
                 "-gravity center -extent %dx%d '%s' 2>/dev/null",
                 qsrc, w, h, w, h, xpm_path);
        break;
    case WP_CENTER:
        snprintf(cmd, sizeof(cmd),
                 "convert %s -background '#D4D0C8' "
                 "-gravity center -extent %dx%d '%s' 2>/dev/null",
                 qsrc, w, h, xpm_path);
        break;
    case WP_TILE:
        snprintf(cmd, sizeof(cmd),
                 "convert %s '%s' 2>/dev/null",
                 qsrc, xpm_path);
        break;
    default:
        snprintf(cmd, sizeof(cmd),
                 "convert %s -resize %dx%d '%s' 2>/dev/null",
                 qsrc, w, h, xpm_path);
        break;
    }
    if (system(cmd) != 0) return NULL;
    if (access(xpm_path, R_OK) != 0) return NULL;
    return xpm_path;
}

static void load_pixmap(int idx) {
    DeskIcon *ic = &icons[idx];
    if (ic->icon[0] == '\0') return;
    /* Try direct XPM load first */
    XpmAttributes attr;
    attr.valuemask = XpmSize;
    if (XpmReadFileToPixmap(dpy, RootWindow(dpy, Scr.screen), ic->icon,
                            &ic->pixmap, &ic->mask, &attr) == XpmSuccess) {
        ic->pixmap_w = attr.width;
        ic->pixmap_h = attr.height;
        return;
    }
    /* Convert via ImageMagick and try again */
    char *xpm = convert_to_xpm(ic->icon, ICON_SIZE, ICON_SIZE, -1);
    if (xpm) {
        if (XpmReadFileToPixmap(dpy, RootWindow(dpy, Scr.screen), xpm,
                                &ic->pixmap, &ic->mask, &attr) == XpmSuccess) {
            ic->pixmap_w = attr.width;
            ic->pixmap_h = attr.height;
            return;
        }
    }
    /* Failed - use placeholder */
    ic->pixmap = None;
    ic->mask = None;
    ic->pixmap_w = ICON_SIZE;
    ic->pixmap_h = ICON_SIZE;
}

/* Load a single image as the desktop wallpaper (PNG/JPG via convert, or XPM
 * directly), set it as the window background and publish it on the root for
 * pseudo-transparency tools.  Returns 1 on success, 0 on failure. */
static int apply_wallpaper(const char *path) {
    Pixmap pm = None;
    XpmAttributes attr;
    attr.valuemask = XpmSize;
    if (!path || !path[0] || access(path, R_OK) != 0) return 0;
    if (strstr(path, ".xpm") != NULL) {
        if (XpmReadFileToPixmap(dpy, RootWindow(dpy, Scr.screen), path,
                                &pm, NULL, &attr) != XpmSuccess)
            return 0;
    } else {
        char *xpm = convert_to_xpm(path, sw, sh, wallpaper_mode);
        if (!xpm) return 0;
        if (XpmReadFileToPixmap(dpy, RootWindow(dpy, Scr.screen), xpm,
                                &pm, NULL, &attr) != XpmSuccess)
            return 0;
    }
    if (wallpaper_pm) XFreePixmap(dpy, wallpaper_pm);
    wallpaper_pm = pm;
    XSetWindowBackgroundPixmap(dpy, desktop_win, wallpaper_pm);
    XChangeProperty(dpy, Scr.Root, a_xrootpmap, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char *)&wallpaper_pm, 1);
    XChangeProperty(dpy, Scr.Root, a_esetroot, XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char *)&wallpaper_pm, 1);
    return 1;
}

/* Load the wallpaper: the user-selected one if present,
 * else the first available built-in default, else a solid Mac OS 9 beige. */
static void load_wallpaper(void) {
    char line[1024];
    FILE *f;
    int done = 0;

    f = fopen(wallpaper_conf_path(), "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            char *s = trim(line);
            strncpy(current_wallpaper, s, sizeof(current_wallpaper) - 1);
            current_wallpaper[sizeof(current_wallpaper) - 1] = '\0';
            if (fgets(line, sizeof(line), f)) {
                char *m = trim(line);
                if (strcmp(m, "center") == 0) wallpaper_mode = WP_CENTER;
                else if (strcmp(m, "tile") == 0) wallpaper_mode = WP_TILE;
                else if (strcmp(m, "fit") == 0) wallpaper_mode = WP_FIT;
                else wallpaper_mode = WP_SCALE;
            }
            /* Apply via current_wallpaper, NOT s: s aliases the `line` buffer
             * which the mode-line fgets() above has since overwritten.  Using
             * the stable copy makes the saved path the single source of truth
             * for both persisting and applying. */
            if (apply_wallpaper(current_wallpaper)) done = 1;
        }
        fclose(f);
    }
    if (!done) {
        char p0[512], p1[512], p2[512];
        const char *paths[4];
        snprintf(p0, sizeof(p0), "%s/Downloads/wallpaper.jpg", home_dir());
        snprintf(p1, sizeof(p1), "%s/.idesktop/wallpaper.xpm", home_dir());
        snprintf(p2, sizeof(p2), "%s/.mlvwm/patterns/mac-os-background-hi-res.png", home_dir());
        paths[0] = p0; paths[1] = p1; paths[2] = p2; paths[3] = NULL;
        int i;
        for (i = 0; paths[i]; i++)
            if (apply_wallpaper(paths[i])) { done = 1; break; }
    }
    if (!done) {
        /* Default to a beige color like Mac OS 9 */
        XColor scol;
        if (wallpaper_pm) { XFreePixmap(dpy, wallpaper_pm); wallpaper_pm = None; }
        if (XAllocNamedColor(dpy, DefaultColormap(dpy, Scr.screen),
                             "#D4D0C8", &scol, &scol))
            XSetWindowBackground(dpy, desktop_win, scol.pixel);
        else
            XSetWindowBackground(dpy, desktop_win, WhitePixel(dpy, Scr.screen));
    }
    XClearWindow(dpy, desktop_win);
}

/* Persist and apply a new wallpaper chosen by the user */
static const char *wp_mode_str(int m) {
    switch (m) {
    case WP_CENTER: return "center";
    case WP_TILE:   return "tile";
    case WP_FIT:    return "fit";
    default:        return "scale";
    }
}

static void save_wallpaper_conf(void) {
    FILE *f = fopen(wallpaper_conf_path(), "w");
    if (f) {
        fprintf(f, "%s\n%s\n", current_wallpaper, wp_mode_str(wallpaper_mode));
        fclose(f);
    }
}

static void set_wallpaper(const char *path) {
    strncpy(current_wallpaper, path, sizeof(current_wallpaper) - 1);
    current_wallpaper[sizeof(current_wallpaper) - 1] = '\0';
    save_wallpaper_conf();
    if (apply_wallpaper(path)) {
        XClearWindow(dpy, desktop_win);
        redraw_icons();
    }
}

/* Draw caption */
static void draw_caption(int idx) {
    DeskIcon *ic = &icons[idx];
    if (ic->caption[0] == '\0' || !font) return;
    int text_y = ic->y + ICON_SIZE + 12;
    int text_w = XTextWidth(font, ic->caption, strlen(ic->caption));
    int text_x = ic->x + ICON_SIZE / 2 - text_w / 2;
    if (text_x < 0) text_x = 0;
    /* Background sized to text width + 4px padding on each side.
     * This prevents ghost trails when the text is shorter or longer
     * than the icon width. */
    XSetForeground(dpy, gc, WhitePixel(dpy, Scr.screen));
    XFillRectangle(dpy, desktop_win, gc, text_x - 2, text_y - 10, text_w + 4, 14);
    /* Text */
    XSetForeground(dpy, gc, BlackPixel(dpy, Scr.screen));
    XDrawString(dpy, desktop_win, gc, text_x, text_y, ic->caption, strlen(ic->caption));
}

/* Compute the union bounding box of an icon and its caption.
 * The caption is centered on the icon and may be wider than ICON_SIZE,
 * extending to the left of ic->x.  Both regions must be cleared together
 * to avoid leaving ghost trails when a wide caption is dragged. */
static void icon_bbox(int idx, int *bx, int *by, int *bw, int *bh) {
    DeskIcon *ic = &icons[idx];
    int left   = ic->x - 2;
    int top    = ic->y - 2;
    int right  = ic->x + ICON_SIZE + 2;
    int bottom = ic->y + ICON_SIZE + 2;
    if (font && ic->caption[0]) {
        int text_y = ic->y + ICON_SIZE + 12;
        int text_w = XTextWidth(font, ic->caption, strlen(ic->caption));
        int text_x = ic->x + ICON_SIZE / 2 - text_w / 2;
        if (text_x < 0) text_x = 0;
        if (text_x - 2 < left)            left = text_x - 2;
        if (text_x + text_w + 2 > right)  right = text_x + text_w + 2;
        if (text_y + 4 > bottom)          bottom = text_y + 4;
    }
    *bx = left;
    *by = top;
    *bw = right - left;
    *bh = bottom - top;
}

/* Draw a single icon */
static void draw_icon(int idx) {
    DeskIcon *ic = &icons[idx];
    if (ic->selected) {
        XSetForeground(dpy, gc, BlackPixel(dpy, Scr.screen));
        XDrawRectangle(dpy, desktop_win, gc, ic->x - 1, ic->y - 1,
                       ICON_SIZE + 2, ICON_SIZE + 2);
    }
    if (ic->pixmap != None) {
        if (ic->mask != None) {
            XSetClipMask(dpy, gc, ic->mask);
            XSetClipOrigin(dpy, gc, ic->x, ic->y);
        }
        XCopyArea(dpy, ic->pixmap, desktop_win, gc, 0, 0,
                  ic->pixmap_w, ic->pixmap_h, ic->x, ic->y);
        if (ic->mask != None) XSetClipMask(dpy, gc, None);
    } else {
        /* Placeholder */
        XSetForeground(dpy, gc, BlackPixel(dpy, Scr.screen));
        XDrawRectangle(dpy, desktop_win, gc, ic->x, ic->y, ICON_SIZE - 1, ICON_SIZE - 1);
    }
    draw_caption(idx);
}

/* Redraw all icons */
static void redraw_icons(void) {
    int i;
    for (i = 0; i < n_icons; i++) draw_icon(i);
}

static int rects_overlap(int ax, int ay, int aw, int ah,
                         int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

/* Live icon drag: erase the icon (and caption) at its current position,
 * revealing the static wallpaper background that the X server repaints for
 * free, then move it to (nx,ny) and draw the real icon + caption there.
 * This replaces the old XOR rubber-band outline so the whole icon follows
 * the cursor.  It stays flicker-free because the only thing cleared is the
 * icon's own bounding box (wallpaper shows through), and the only icons
 * repainted are the dragged one plus any neighbour that shared that box. */
static void drag_icon_to(int idx, int nx, int ny) {
    int ox, oy, ow, oh, i;
    icon_bbox(idx, &ox, &oy, &ow, &oh);
    XClearArea(dpy, desktop_win, ox, oy, ow, oh, False);
    icons[idx].x = nx;
    icons[idx].y = ny;
    for (i = 0; i < n_icons; i++) {
        int bx, by, bw, bh;
        if (i == idx) continue;
        icon_bbox(i, &bx, &by, &bw, &bh);
        if (rects_overlap(ox, oy, ow, oh, bx, by, bw, bh))
            draw_icon(i);
    }
    draw_icon(idx);
}

static void snap_to_grid(DeskIcon *ic) {
    int col = (ic->x - GRID_PAD_X + GRID_W / 2) / GRID_W;
    int row = (ic->y - GRID_PAD_Y + GRID_H / 2) / GRID_H;
    if (col < 0) col = 0;
    if (row < 0) row = 0;
    ic->x = GRID_PAD_X + col * GRID_W;
    ic->y = GRID_PAD_Y + row * GRID_H;
}

static void arrange_icons(void) {
    int max_rows = (sh - GRID_PAD_Y * 2) / GRID_H;
    int i;
    if (max_rows < 1) max_rows = 1;
    for (i = 0; i < n_icons; i++) {
        int col = i / max_rows;
        int row = i % max_rows;
        icons[i].x = GRID_PAD_X + col * GRID_W;
        icons[i].y = GRID_PAD_Y + row * GRID_H;
        save_lnk(i);
    }
    XClearWindow(dpy, desktop_win);
    redraw_icons();
}

/* Launch command */
static void launch_cmd(const char *cmd) {
    if (fork() == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(1);
    }
}

/* Free loaded icon pixmaps and reset the list */
static void free_icons(void) {
    int i;
    for (i = 0; i < n_icons; i++) {
        if (icons[i].pixmap != None) XFreePixmap(dpy, icons[i].pixmap);
        if (icons[i].mask != None) XFreePixmap(dpy, icons[i].mask);
    }
    n_icons = 0;
}

/* Reload icons (and optionally the wallpaper) and repaint the desktop */
static void reload_desktop(int reload_wallpaper) {
    int i;
    free_icons();
    load_icons();
    for (i = 0; i < n_icons; i++)
        load_pixmap(i);
    if (reload_wallpaper)
        load_wallpaper();
    XClearWindow(dpy, desktop_win);
    redraw_icons();
}

/* ---- Inline modal text-input dialog --------------------------------- */

#define PROMPT_W 380
#define PROMPT_H 78

static void draw_prompt(Window win, const char *title, const char *text) {
    XClearWindow(dpy, win);
    if (!font) return;
    XSetForeground(dpy, gc, BlackPixel(dpy, Scr.screen));
    XDrawString(dpy, win, gc, 12, 22, title, strlen(title));
    /* input field */
    XDrawRectangle(dpy, win, gc, 12, 32, PROMPT_W - 24, 22);
    XDrawString(dpy, win, gc, 16, 48, text, strlen(text));
    /* text cursor */
    int tw = XTextWidth(font, text, strlen(text));
    XDrawLine(dpy, win, gc, 16 + tw + 1, 35, 16 + tw + 1, 51);
    /* hint */
    XDrawString(dpy, win, gc, 12, 70,
                "Enter = OK   Esc = Cancel", 25);
}

/* Show a modal single-line prompt.  buf holds an optional default on entry
 * and the result on exit.  Returns 1 if accepted (Return), 0 if cancelled. */
static int prompt_text(const char *title, char *buf, int buflen) {
    XSetWindowAttributes wa;
    int px = (sw - PROMPT_W) / 2;
    int py = sh / 3;
    int len = strlen(buf);
    int result = -1;

    Window win = XCreateSimpleWindow(dpy, Scr.Root, px, py, PROMPT_W, PROMPT_H,
                                     2, BlackPixel(dpy, Scr.screen),
                                     WhitePixel(dpy, Scr.screen));
    wa.override_redirect = True;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &wa);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapRaised(dpy, win);
    if (XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync,
                      CurrentTime) != GrabSuccess) {
        XDestroyWindow(dpy, win);
        return 0;
    }

    while (result < 0) {
        XEvent ev;
        XMaskEvent(dpy, ExposureMask | KeyPressMask, &ev);
        if (ev.type == Expose) {
            if (ev.xexpose.count == 0) draw_prompt(win, title, buf);
        } else if (ev.type == KeyPress) {
            char kb[32];
            KeySym ks;
            int n = XLookupString(&ev.xkey, kb, sizeof(kb) - 1, &ks, NULL);
            if (ks == XK_Return || ks == XK_KP_Enter) {
                result = 1;
            } else if (ks == XK_Escape) {
                result = 0;
            } else if (ks == XK_BackSpace) {
                if (len > 0) buf[--len] = '\0';
                draw_prompt(win, title, buf);
            } else {
                int i;
                for (i = 0; i < n && len < buflen - 1; i++)
                    if (isprint((unsigned char)kb[i])) buf[len++] = kb[i];
                buf[len] = '\0';
                draw_prompt(win, title, buf);
            }
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XDestroyWindow(dpy, win);
    return result;
}

/* ---- New launcher --------------------------------------------------- */

static void do_rename_icon(int idx) {
    char name[64];
    int bx, by, bw, bh;
    if (idx < 0 || idx >= n_icons) return;
    strncpy(name, icons[idx].caption, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (prompt_text("Rename icon:", name, sizeof(name)) && name[0] != '\0') {
        icon_bbox(idx, &bx, &by, &bw, &bh);
        XClearArea(dpy, desktop_win, bx, by, bw, bh, False);
        strncpy(icons[idx].caption, name, sizeof(icons[idx].caption) - 1);
        icons[idx].caption[sizeof(icons[idx].caption) - 1] = '\0';
        save_lnk(idx);
        redraw_icons();
    }
}

/* Prompt for a name + command and create a launcher .lnk at (x,y). */
static void do_new_launcher(int x, int y) {
    char name[64] = "";
    char cmd[256] = "";
    char icon_path[256] = "";
    char path[1024];
    int n;
    FILE *f;

    if (!prompt_text("Launcher name:", name, sizeof(name)) || name[0] == '\0')
        return;
    if (!prompt_text("Command to run:", cmd, sizeof(cmd)) || cmd[0] == '\0')
        return;
    prompt_text("Icon path (optional):", icon_path, sizeof(icon_path));

    {
        char idir[512];
        snprintf(idir, sizeof(idir), "%s/.idesktop", home_dir());
        mkdir(idir, 0755);
    }
    for (n = 0; n < 1000; n++) {
        snprintf(path, sizeof(path),
                 "%s/.idesktop/launcher%d.lnk", home_dir(), n);
        if (access(path, F_OK) != 0) break;
    }
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "table Icon\n");
        fprintf(f, "  Icon: %s\n", icon_path);
        fprintf(f, "  Caption: %s\n", name);
        fprintf(f, "  Command: %s\n", cmd);
        fprintf(f, "  X: %d\n", x);
        fprintf(f, "  Y: %d\n", y);
        fprintf(f, "end\n");
        fclose(f);
    }
    reload_desktop(0);
}

/* ---- Wallpaper picker ----------------------------------------------- */

static int is_image(const char *name) {
    const char *exts[] = { ".xpm", ".png", ".jpg", ".jpeg",
                           ".bmp", ".gif", ".tif", ".tiff", NULL };
    const char *dot = strrchr(name, '.');
    int i;
    if (!dot) return 0;
    for (i = 0; exts[i]; i++)
        if (strcasecmp(dot, exts[i]) == 0) return 1;
    return 0;
}

#define MAX_WALL_ITEMS 24

/* Populate the wall_menu with image files from wall_dirs, plus a "Browse..."
 * fallback.  Called before showing the context menu so wall_menu is ready to
 * appear as a submenu of "Change Background...". */
static void build_wallpaper_menu(void) {
    char act[1024];
    int count = 0, i;

    FreeMenuItems(&wall_menu, False);
    wall_menu.m_item = NULL;
    const char **wdirs = wall_dir_list();
    for (i = 0; wdirs[i] && count < MAX_WALL_ITEMS; i++) {
        DIR *d = opendir(wdirs[i]);
        struct dirent *e;
        if (!d) continue;
        while ((e = readdir(d)) != NULL && count < MAX_WALL_ITEMS) {
            if (!is_image(e->d_name)) continue;
            snprintf(act, sizeof(act), "IDeskWall %s/%s",
                     wdirs[i], e->d_name);
            AddMenuItem(&wall_menu, e->d_name, act, NULL, NULL, NULL, SELECTON);
            count++;
        }
        closedir(d);
    }
    if (count > 0)
        AddMenuItem(&wall_menu, "", NULL, NULL, NULL, NULL, 0);
    AddMenuItem(&wall_menu, "Browse...", "IDeskBg", NULL, NULL, NULL, SELECTON);
    AddMenuItem(&wall_menu, "", NULL, NULL, NULL, NULL, 0);
    AddMenuItem(&wall_menu, "Scale (stretch)", "IDeskWpMode 0",
                NULL, NULL, NULL, SELECTON);
    AddMenuItem(&wall_menu, "Center", "IDeskWpMode 1",
                NULL, NULL, NULL, SELECTON);
    AddMenuItem(&wall_menu, "Tile", "IDeskWpMode 2",
                NULL, NULL, NULL, SELECTON);
    AddMenuItem(&wall_menu, "Fit", "IDeskWpMode 3",
                NULL, NULL, NULL, SELECTON);
}

/* Build the desktop context menu items and pop it up at (x,y).
 * target is an icon index, or -1 for the empty-area menu. */
static void show_desktop_menu(int x, int y, int target) {
    char act[64];
    menu_open_x = x;
    menu_open_y = y;
    FreeMenuItems(&desk_menu, False);
    desk_menu.m_item = NULL;
    if (target >= 0) {
        snprintf(act, sizeof(act), "IDeskOpen %d", target);
        AddMenuItem(&desk_menu, "Open", act, NULL, NULL, NULL, SELECTON);
        snprintf(act, sizeof(act), "IDeskRename %d", target);
        AddMenuItem(&desk_menu, "Rename", act, NULL, NULL, NULL, SELECTON);
        snprintf(act, sizeof(act), "IDeskDelete %d", target);
        AddMenuItem(&desk_menu, "Delete Icon", act, NULL, NULL, NULL, SELECTON);
    } else {
        build_wallpaper_menu();
        AddMenuItem(&desk_menu, "New Launcher", "IDeskNew",
                    NULL, NULL, NULL, SELECTON);
        AddMenuItem(&desk_menu, "Change Background", NULL,
                    NULL, NULL, &wall_menu, SELECTON);
        FreeMenuItems(&shot_menu, False);
        shot_menu.m_item = NULL;
        AddMenuItem(&shot_menu, "Full Screen", "IDeskShot 0",
                    NULL, NULL, NULL, SELECTON);
        AddMenuItem(&shot_menu, "Selection", "IDeskShot 1",
                    NULL, NULL, NULL, SELECTON);
        AddMenuItem(&desk_menu, "Take Screenshot", NULL,
                    NULL, NULL, &shot_menu, SELECTON);
        AddMenuItem(&desk_menu, "Arrange Icons", "IDeskArrange",
                    NULL, NULL, NULL, SELECTON);
        AddMenuItem(&desk_menu, "Refresh Desktop", "IDeskRefresh",
                    NULL, NULL, NULL, SELECTON);
    }
    PopupMenu(&desk_menu, x, y, 1);
}

/* Context-menu action handlers, dispatched from ExecuteFunction()
 * via the IDesk* entries in the builtin table (functions.c). */
void DesktopMenuOpen(char *act) {
    int idx = -1;
    sscanf(act, "%*s %d", &idx);
    if (idx >= 0 && idx < n_icons)
        launch_cmd(icons[idx].command);
}

void DesktopMenuDelete(char *act) {
    int idx = -1, j;
    sscanf(act, "%*s %d", &idx);
    if (idx < 0 || idx >= n_icons) return;
    unlink(icons[idx].lnk_path);
    if (icons[idx].pixmap != None) XFreePixmap(dpy, icons[idx].pixmap);
    if (icons[idx].mask != None) XFreePixmap(dpy, icons[idx].mask);
    for (j = idx; j < n_icons - 1; j++)
        icons[j] = icons[j + 1];
    n_icons--;
    XClearWindow(dpy, desktop_win);
    redraw_icons();
}

void DesktopMenuRefresh(char *act) {
    (void)act;
    reload_desktop(1);
}

void DesktopMenuArrange(char *act) {
    (void)act;
    arrange_icons();
}

/* These only flag the request; the modal dialog / picker is opened after
 * the context menu has fully closed (see HandleDesktopEvent), so we never run
 * a nested grab inside the menu's own modal loop. */
void DesktopMenuRename(char *act) {
    int idx = -1;
    sscanf(act, "%*s %d", &idx);
    if (idx >= 0 && idx < n_icons) pending_rename = idx;
}

void DesktopMenuNewLauncher(char *act) {
    (void)act;
    pending_new = 1;
}

void DesktopMenuWallpaper(char *act) {
    (void)act;
    pending_bg = 1;
}

/* Apply a wallpaper chosen from the picker.  Action is "IDeskWall <path>";
 * the path may contain spaces, so take everything after the first space. */
void DesktopMenuWallpaperSet(char *act) {
    char *p = strchr(act, ' ');
    if (!p) return;
    p++;
    if (*p) set_wallpaper(p);
}

void DesktopMenuWpMode(char *act) {
    int mode = WP_SCALE;
    sscanf(act, "%*s %d", &mode);
    if (mode < WP_SCALE || mode > WP_FIT) mode = WP_SCALE;
    wallpaper_mode = mode;
    save_wallpaper_conf();
    if (current_wallpaper[0] && apply_wallpaper(current_wallpaper)) {
        XClearWindow(dpy, desktop_win);
        redraw_icons();
    }
}

void DesktopMenuScreenshot(char *act) {
    int sel = 0;
    char cmd[3072];
    char dir[512];
    char file[640];
    char qfile[1300];
    char ts[64];
    time_t now;
    struct tm *tm;
    sscanf(act, "%*s %d", &sel);
    snprintf(dir, sizeof(dir), "%s/Pictures", home_dir());
    mkdir(dir, 0755);
    now = time(NULL);
    tm = localtime(&now);
    strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", tm);
    snprintf(file, sizeof(file), "%s/screenshot-%s.png", dir, ts);
    shell_quote(file, qfile, sizeof(qfile));
    if (sel)
        snprintf(cmd, sizeof(cmd),
                 "maim -s %s 2>/dev/null || "
                 "scrot -s %s 2>/dev/null",
                 qfile, qfile);
    else
        snprintf(cmd, sizeof(cmd),
                 "maim %s 2>/dev/null || "
                 "scrot %s 2>/dev/null",
                 qfile, qfile);
    if (fork() == 0) { setsid(); execl("/bin/sh", "sh", "-c", cmd, NULL); _exit(1); }
}

void InitDesktop(void) {
    sw = DisplayWidth(dpy, Scr.screen);
    sh = DisplayHeight(dpy, Scr.screen);

    /* Atoms */
    a_net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    a_net_wm_window_type_desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    a_net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    a_net_wm_state_below = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
    a_net_wm_state_sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    a_net_wm_state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    a_net_wm_state_skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    a_xrootpmap = XInternAtom(dpy, "_XROOTPMAP_ID", False);
    a_esetroot = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);

    /* Create desktop window */
    XSetWindowAttributes wa;
    wa.event_mask = ButtonPressMask | ButtonReleaseMask |
                   Button1MotionMask | ExposureMask | StructureNotifyMask |
                   FocusChangeMask;
    wa.bit_gravity = NorthWestGravity;
    wa.backing_store = WhenMapped;
    unsigned long vm = CWEventMask | CWBitGravity | CWBackingStore;
    desktop_win = XCreateWindow(dpy, Scr.Root, 0, 0, sw, sh, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                vm, &wa);

    /* Set EWMH window type to DESKTOP */
    XChangeProperty(dpy, desktop_win, a_net_wm_window_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&a_net_wm_window_type_desktop, 1);
    Atom states[] = { a_net_wm_state_below, a_net_wm_state_sticky,
                     a_net_wm_state_skip_pager, a_net_wm_state_skip_taskbar };
    XChangeProperty(dpy, desktop_win, a_net_wm_state, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)states, 4);

    /* Load wallpaper (apply_wallpaper also publishes it on the root) */
    load_wallpaper();

    /* Load icons */
    load_icons();
    int i;
    for (i = 0; i < n_icons; i++)
        load_pixmap(i);

    /* Create GC */
    gc = XCreateGC(dpy, desktop_win, 0, NULL);
    font = XLoadQueryFont(dpy, "-*-fixed-medium-r-*-*-12-*-*-*-*-*-iso8859-*");
    if (font) XSetFont(dpy, gc, font->fid);

    /* Create the native context-menu popups (items added on demand) */
    memset(&desk_menu, 0, sizeof(desk_menu));
    desk_menu.name = strdup("DESKTOP");
    desk_menu.flags = ACTIVE;
    desk_menu.LabelStr = NULL;
    desk_menu.m_item = NULL;
    CreatePopupMenuLabel(&desk_menu);

    memset(&wall_menu, 0, sizeof(wall_menu));
    wall_menu.name = strdup("WALLPAPER");
    wall_menu.flags = ACTIVE;
    wall_menu.LabelStr = NULL;
    wall_menu.m_item = NULL;
    CreatePopupMenuLabel(&wall_menu);

    memset(&shot_menu, 0, sizeof(shot_menu));
    shot_menu.name = strdup("SCREENSHOT");
    shot_menu.flags = ACTIVE;
    shot_menu.LabelStr = NULL;
    shot_menu.m_item = NULL;
    CreatePopupMenuLabel(&shot_menu);

    /* Show desktop window */
    XMapWindow(dpy, desktop_win);
    XLowerWindow(dpy, desktop_win);
}

void HandleDesktopEvent(XEvent *ev) {
    switch (ev->type) {
        case Expose:
            if (ev->xexpose.count == 0) redraw_icons();
            break;
        case ButtonPress:
            /* Any click on the desktop deactivates the current window
             * (mirrors the ISDESKTOP path in event.c) and resets the
             * menu bar to its default state. */
            if (Scr.ActiveWin) SetFocus(NULL);
            if (ev->xbutton.button == 1) {
                int idx = hit_test(ev->xbutton.x, ev->xbutton.y);
                if (idx >= 0) {
                    drag_idx = idx;
                    drag_x = ev->xbutton.x;
                    drag_y = ev->xbutton.y;
                    drag_moved = 0;
                    drag_orig_x = icons[idx].x;
                    drag_orig_y = icons[idx].y;
                } else {
                    /* Deselect all */
                    int i;
                    for (i = 0; i < n_icons; i++) {
                        if (icons[i].selected) {
                            icons[i].selected = 0;
                            draw_icon(i);
                        }
                    }
                }
            } else if (ev->xbutton.button == 3) {
                int idx = hit_test(ev->xbutton.x, ev->xbutton.y);
                show_desktop_menu(ev->xbutton.x, ev->xbutton.y, idx);
                /* Open any deferred dialog/picker now that the menu is closed,
                 * so we never grab inside the menu's own modal loop. */
                if (pending_rename >= 0) {
                    int ri = pending_rename;
                    pending_rename = -1;
                    do_rename_icon(ri);
                }
                if (pending_new) {
                    pending_new = 0;
                    do_new_launcher(menu_open_x, menu_open_y);
                }
                if (pending_bg) {
                    char bgpath[512];
                    pending_bg = 0;
                    bgpath[0] = '\0';
                    if (prompt_text("Wallpaper image path:", bgpath,
                                    sizeof(bgpath)) && bgpath[0] != '\0')
                        set_wallpaper(bgpath);
                }
            }
            break;
        case MotionNotify:
            if (drag_idx >= 0) {
                /* Coalesce: jump straight to the most recent queued motion so
                 * the icon tracks the cursor without lagging behind a backlog
                 * of intermediate events. */
                XEvent latest = *ev;
                while (XCheckTypedWindowEvent(dpy, desktop_win,
                                              MotionNotify, &latest))
                    ;
                int dx = latest.xmotion.x - drag_x;
                int dy = latest.xmotion.y - drag_y;
                if (drag_moved ||
                    abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD) {
                    drag_moved = 1;
                    drag_icon_to(drag_idx, drag_orig_x + dx, drag_orig_y + dy);
                }
            }
            break;
        case ButtonRelease:
            if (drag_idx >= 0 && ev->xbutton.button == 1) {
                int idx = drag_idx;
                drag_idx = -1;
                if (!drag_moved) {
                    /* Single click - track double click */
                    if (ev->xbutton.time - last_click_time <= CLICK_DELAY &&
                        abs(ev->xbutton.x - last_click_x) < DRAG_THRESHOLD &&
                        abs(ev->xbutton.y - last_click_y) < DRAG_THRESHOLD) {
                        click_count++;
                        if (click_count == 2) {
                            launch_cmd(icons[idx].command);
                            click_count = 0;
                        }
                    } else {
                        click_count = 1;
                        last_click_time = ev->xbutton.time;
                        last_click_x = ev->xbutton.x;
                        last_click_y = ev->xbutton.y;
                    }
                } else {
                    /* Live drag already moved icons[idx] to its final spot;
                     * just persist the new position and repaint cleanly to
                     * restore z-order for any icons it was dragged across. */
                    save_lnk(idx);
                    click_count = 0;
                    XClearWindow(dpy, desktop_win);
                }
                redraw_icons();
            }
            break;
    }
}

int IsDesktopWindow(Window w) {
    return w == desktop_win;
}

void RedrawDesktopIcons(void) {
    redraw_icons();
}

void DestroyDesktop(void) {
    if (gc) XFreeGC(dpy, gc);
    if (desktop_win) XDestroyWindow(dpy, desktop_win);
    if (wallpaper_pm) XFreePixmap(dpy, wallpaper_pm);
}
