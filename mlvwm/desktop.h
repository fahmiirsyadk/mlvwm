#ifndef _MLVWM_DESKTOP_
#define _MLVWM_DESKTOP_

#include <X11/Xlib.h>

/* Initialize the desktop (create window, load icons, set wallpaper) */
void InitDesktop(void);

/* Handle events on the desktop window */
void HandleDesktopEvent(XEvent *ev);

/* Check if a window is the desktop window */
int IsDesktopWindow(Window w);

/* Redraw all desktop icons */
void RedrawDesktopIcons(void);

/* Clean up desktop on exit */
void DestroyDesktop(void);

/* Context-menu action handlers, dispatched via ExecuteFunction()
 * from the IDesk* entries in the builtin function table. */
void DesktopMenuOpen(char *act);
void DesktopMenuDelete(char *act);
void DesktopMenuRefresh(char *act);
void DesktopMenuArrange(char *act);
void DesktopMenuRename(char *act);
void DesktopMenuNewLauncher(char *act);
void DesktopMenuWallpaper(char *act);
void DesktopMenuWallpaperSet(char *act);
void DesktopMenuWpMode(char *act);
void DesktopMenuScreenshot(char *act);

#endif
