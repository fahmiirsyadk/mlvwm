/****************************************************************************/
/* This module is based on fvwm, but has been siginificantly modified       */
/* by TakaC Hasegawa (tac.hasegawa@gmail.com)                               */
/****************************************************************************/
/****************************************************************************
 * This module is based on Twm, but has been siginificantly modified 
 * by Rob Nation (nation@rocket.sanders.lockheed.com)
 ****************************************************************************/
/*****************************************************************************/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**  Portions Copyright 1989 by the Massachusetts Institute of Technology   **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    names of Evans & Sutherland and M.I.T. not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND AND M.I.T. DISCLAIM ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND OR    **/
/**    M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "mlvwm.h"
#include "screen.h"
#include "event.h"
#include "add_window.h"
#include "config.h"
#include "functions.h"
#include "misc.h"
#include "desktop.h"

#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/extensions/shape.h>
#include <X11/xpm.h>
#ifdef USE_LOCALE
#include <X11/Xlocale.h>
#endif

static unsigned char start_mesh[] = { 0x03, 0x03, 0x0c, 0x0c};

ScreenInfo Scr;
Display *dpy;
int xfd;
XContext MlvwmContext;
XContext MenuContext;
XClassHint NoClass;

static char **g_argv;
int ShapeEventBase, ShapeErrorBase;

void CreateCursors( void )
{
	Scr.MlvwmCursors[DEFAULT] = XCreateFontCursor( dpy, XC_left_ptr );
	Scr.MlvwmCursors[SYS] = XCreateFontCursor( dpy, XC_X_cursor );
	Scr.MlvwmCursors[TITLE_CURSOR] = XCreateFontCursor( dpy, XC_hand2 );
	Scr.MlvwmCursors[RESIZE] =
		XCreateFontCursor( dpy, XC_bottom_right_corner );
	Scr.MlvwmCursors[MOVE] = XCreateFontCursor( dpy, XC_fleur );
	Scr.MlvwmCursors[MENU] = XCreateFontCursor( dpy, XC_sb_left_arrow );
	Scr.MlvwmCursors[WAIT] = XCreateFontCursor( dpy, XC_watch );
	Scr.MlvwmCursors[SELECT] = XCreateFontCursor( dpy, XC_dot );
	Scr.MlvwmCursors[DESTROY] = XCreateFontCursor( dpy, XC_pirate );
	Scr.MlvwmCursors[SBARH_CURSOR] =
		XCreateFontCursor( dpy, XC_sb_h_double_arrow );
	Scr.MlvwmCursors[SBARV_CURSOR] =
		XCreateFontCursor( dpy, XC_sb_v_double_arrow );
	Scr.MlvwmCursors[MINMAX_CURSOR] = XCreateFontCursor( dpy, XC_sizing );
	Scr.MlvwmCursors[SHADER_UP_CURSOR] =
		XCreateFontCursor( dpy, XC_based_arrow_up );
	Scr.MlvwmCursors[SHADER_DOWN_CURSOR] =
		XCreateFontCursor( dpy, XC_based_arrow_down );
}

void LoadDefaultFonts( void )
{
#ifdef USE_LOCALE
	char **miss, *def;
	int n_miss, lp;

	if( Scr.flags & DEBUGOUT )
		fprintf( stderr, "Locale: %s\n", setlocale(LC_ALL, NULL)); 

	Scr.MenuBarFs =	XCreateFontSet( dpy, DEFAULTFS, &miss, &n_miss, &def );
	if( n_miss>0 ){
		for( lp=0; lp<n_miss; lp++ )
			DrawErrMsgOnMenu( "Can't load default font ", miss[lp] );
		XFreeStringList( miss );
	}
	Scr.MenuFs = XCreateFontSet( dpy, DEFAULTFS, &miss, &n_miss, &def );
	if( n_miss>0 ){
		for( lp=0; lp<n_miss; lp++ )
			DrawErrMsgOnMenu( "Can't load default font ", miss[lp] );
		XFreeStringList( miss );
	}
	Scr.WindowFs = XCreateFontSet( dpy, DEFAULTFS, &miss, &n_miss, &def );
	if( n_miss>0 ){
		for( lp=0; lp<n_miss; lp++ )
			DrawErrMsgOnMenu( "Can't load default font ", miss[lp] );
		XFreeStringList( miss );
	}
	Scr.BalloonFs = XCreateFontSet( dpy, DEFAULTFS, &miss, &n_miss, &def );
	if( n_miss>0 ){
		for( lp=0; lp<n_miss; lp++ )
			DrawErrMsgOnMenu( "Can't load default font ", miss[lp] );
		XFreeStringList( miss );
	}
#else
	if(( Scr.MenuBarFont = XLoadQueryFont( dpy, DEFAULTFONT )) == NULL )
		DrawErrMsgOnMenu( "Can't load default font ", "MenuBar" );
	if(( Scr.MenuFont =	XLoadQueryFont( dpy, DEFAULTFONT )) == NULL )
		DrawErrMsgOnMenu( "Can't load default font ", "Menu" );
	if(( Scr.WindowFont = XLoadQueryFont( dpy, DEFAULTFONT )) == NULL )
		DrawErrMsgOnMenu( "Can't load default font ", "Window" );
	if(( Scr.BalloonFont = XLoadQueryFont( dpy, DEFAULTFONT )) == NULL )
		DrawErrMsgOnMenu( "Can't load default font ", "Balloon" );
#endif
}

void FreeFont( void )
{
#ifdef USE_LOCALE
	XFreeFontSet( dpy, Scr.MenuBarFs );
	XFreeFontSet( dpy, Scr.MenuFs );
	XFreeFontSet( dpy, Scr.WindowFs );
#else
	XFreeFont( dpy, Scr.MenuBarFont );
	XFreeFont( dpy, Scr.MenuFont );
	XFreeFont( dpy, Scr.WindowFont );
#endif
}

void InitScrParams( void )
{
	unsigned char mask[] = {0x01, 0x02};
	
	Scr.d_depth = DefaultDepth( dpy, Scr.screen );
	Scr.n_desktop = 1;
	Scr.Restarting = False;
	{
		Atom atype;
		int aformat;
		unsigned long nitems, bytes_remain;
		unsigned char *prop;
    
		Scr.currentdesk = 0;
		if ((XGetWindowProperty(dpy, Scr.Root, _XA_WM_DESKTOP,
			 0L, 1L, True, _XA_WM_DESKTOP, &atype, &aformat,
			 &nitems, &bytes_remain, &prop))==Success){
			if(prop != NULL){
				Scr.Restarting = True;
				Scr.currentdesk = *(unsigned long *)prop;
			}
		}
	}
	Scr.MlvwmRoot.w = Scr.Root;
	Scr.MlvwmRoot.prev = NULL;
	Scr.MlvwmRoot.next = NULL;
	Scr.LastActive = calloc( 1, sizeof(MlvwmWindow));
	Scr.MyDisplayWidth = DisplayWidth( dpy, Scr.screen );
	Scr.MyDisplayHeight = DisplayHeight( dpy, Scr.screen );
	Scr.MenuLabelRoot = NULL;
	Scr.MenuRoot = NULL;
	Scr.ActiveMenu = NULL;
	Scr.IconMenu.m_item = NULL;
	Scr.ActiveWin = NULL;
	Scr.root_pushes = 0;
	Scr.pushed_window = &Scr.MlvwmRoot;
	Scr.style_list = NULL;
	Scr.ShortCutRoot = NULL;
	Scr.double_click_time = 300;
	Scr.bar_width = 16;
	Scr.flash_time = 100000;
	Scr.flash_times = 2;
	Scr.zoom_wait = 10000;
	Scr.IconPath = NULL;
	Scr.BalloonOffStr = NULL;
	Scr.BalloonOnStr = NULL;
	Scr.mask = XCreatePixmapFromBitmapData( dpy, Scr.Root, mask, 2, 2, 
										   WhitePixel( dpy, Scr.screen ),
										   BlackPixel( dpy, Scr.screen ),
										   Scr.d_depth );
	Scr.StartFunc = NULL;
	Scr.flags |= STARTING;
	Scr.resist_x = 0;
	Scr.resist_y = 0;
}

void InitGCs( void )
{
	XGCValues gcv;
	unsigned long gcm;

	gcm = GCFunction|GCForeground|GCSubwindowMode|GCLineWidth|GCLineStyle;
	gcv.function = GXxor;
	gcv.subwindow_mode = IncludeInferiors;
	gcv.line_width = 1;

	if( Scr.d_depth>1 )
		gcv.foreground = GetColor( "#777777" );
	else
		gcv.foreground = WhitePixel( dpy, Scr.screen );
	gcv.line_style = FillSolid;

	Scr.RobberGC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	gcm = GCFunction | GCPlaneMask | GCForeground | GCBackground | GCTile;
	gcv.function = GXcopy;
	gcv.plane_mask = AllPlanes;
	gcv.line_width = 0;
	gcv.fill_style = FillSolid;
	gcv.tile = Scr.mask;

	gcv.foreground = BlackPixel( dpy, Scr.screen );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.BlackGC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	gcv.foreground = WhitePixel( dpy, Scr.screen );
	gcv.background = BlackPixel( dpy, Scr.screen );
	Scr.WhiteGC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )	gcv.foreground = GetColor( "#444444" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.Gray1GC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )	gcv.foreground = GetColor( "#777777" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.Gray2GC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )		gcv.foreground = GetColor( "#bbbbbb" );
	if( Scr.d_depth>1 )		gcv.foreground = GetColor( "#aaaaaa" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.Gray3GC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )	gcv.foreground = GetColor( "#e0e0e0" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.Gray4GC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )	gcv.foreground = GetColor( "#3333ff" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.MenuSelectBlueGC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )	gcv.foreground = GetColor( "#dddddd" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.MenuBlueGC = XCreateGC( dpy, Scr.Root, gcm, &gcv );

	if( Scr.d_depth>1 )	gcv.foreground = GetColor( "#ccccff" );
	gcv.background = WhitePixel( dpy, Scr.screen );
	Scr.ScrollBlueGC = XCreateGC( dpy, Scr.Root, gcm, &gcv );
}

void InitVariables( void )
{
	MlvwmContext = XUniqueContext();

	InitScrParams();
	InitGCs();

	NoClass.res_name = NoName;
	NoClass.res_class = NoName;

	AddMenuItem( &(Scr.IconMenu), "Hide Active", "HideActive",
				NULL, NULL, NULL, STRGRAY );
	AddMenuItem( &(Scr.IconMenu), "Hide Others", "HideOthers",
				NULL, NULL, NULL, STRGRAY );
	AddMenuItem( &(Scr.IconMenu), "Show All", "ShowAll",
				NULL, NULL, NULL, STRGRAY );
	AddMenuItem( &(Scr.IconMenu), "", "Nop", NULL, NULL, NULL, STRGRAY );
}

Atom _XA_MIT_PRIORITY_COLORS;
Atom _XA_WM_CHANGE_STATE;
Atom _XA_WM_STATE;
Atom _XA_WM_COLORMAP_WINDOWS;
Atom _XA_WM_PROTOCOLS;
Atom _XA_WM_TAKE_FOCUS;
Atom _XA_WM_DELETE_WINDOW;
Atom _XA_WM_DESKTOP;
Atom _XA_NET_WM_WINDOW_TYPE;
Atom _XA_NET_WM_WINDOW_TYPE_DESKTOP;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_NUMBER_OF_DESKTOPS;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_DESKTOP_NAMES;
Atom _XA_NET_DESKTOP_GEOMETRY;
Atom _XA_NET_DESKTOP_VIEWPORT;
Atom _XA_NET_WORKAREA;
Atom _XA_NET_WM_DESKTOP;
Atom _XA_NET_CLIENT_LIST;
Atom _XA_NET_ACTIVE_WINDOW;
Atom _XA_NET_WM_NAME;
Atom _XA_UTF8_STRING;
Atom _XA_XdndAware;
Atom _XA_XdndProxy;

void InternUsefulAtoms (void)
{
	/* 
	 * Create priority colors if necessary.
	 */
	_XA_MIT_PRIORITY_COLORS = XInternAtom(dpy, "_MIT_PRIORITY_COLORS", False);   
	_XA_WM_CHANGE_STATE = XInternAtom (dpy, "WM_CHANGE_STATE", False);
	_XA_WM_STATE = XInternAtom (dpy, "WM_STATE", False);
	_XA_WM_COLORMAP_WINDOWS = XInternAtom (dpy, "WM_COLORMAP_WINDOWS", False);
	_XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
	_XA_WM_TAKE_FOCUS = XInternAtom (dpy, "WM_TAKE_FOCUS", False);
	_XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);
	_XA_WM_DESKTOP = XInternAtom (dpy, "WM_DESKTOP", False);
	_XA_NET_WM_WINDOW_TYPE = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
	_XA_NET_WM_WINDOW_TYPE_DESKTOP = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	_XA_NET_SUPPORTED = XInternAtom (dpy, "_NET_SUPPORTED", False);
	_XA_NET_SUPPORTING_WM_CHECK = XInternAtom (dpy, "_NET_SUPPORTING_WM_CHECK", False);
	_XA_NET_NUMBER_OF_DESKTOPS = XInternAtom (dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	_XA_NET_CURRENT_DESKTOP = XInternAtom (dpy, "_NET_CURRENT_DESKTOP", False);
	_XA_NET_DESKTOP_NAMES = XInternAtom (dpy, "_NET_DESKTOP_NAMES", False);
	_XA_NET_DESKTOP_GEOMETRY = XInternAtom (dpy, "_NET_DESKTOP_GEOMETRY", False);
	_XA_NET_DESKTOP_VIEWPORT = XInternAtom (dpy, "_NET_DESKTOP_VIEWPORT", False);
	_XA_NET_WORKAREA = XInternAtom (dpy, "_NET_WORKAREA", False);
	_XA_NET_WM_DESKTOP = XInternAtom (dpy, "_NET_WM_DESKTOP", False);
	_XA_NET_CLIENT_LIST = XInternAtom (dpy, "_NET_CLIENT_LIST", False);
	_XA_NET_ACTIVE_WINDOW = XInternAtom (dpy, "_NET_ACTIVE_WINDOW", False);
	_XA_NET_WM_NAME = XInternAtom (dpy, "_NET_WM_NAME", False);
	_XA_UTF8_STRING = XInternAtom (dpy, "UTF8_STRING", False);
	_XA_XdndAware = XInternAtom (dpy, "XdndAware", False);
	_XA_XdndProxy = XInternAtom (dpy, "XdndProxy", False);

	return;
}

/* ------------------------------------------------------------------ *
 *  EWMH (freedesktop _NET_*) desktop / workspace hints.
 *
 *  mlvwm has always tracked virtual desktops through its private
 *  WM_DESKTOP atom; these helpers additionally publish the standard
 *  _NET_* properties so external pagers, taskbars and tools such as
 *  wmctrl can see and drive the existing workspaces.  They are pure
 *  publishers over the existing Scr.* state - no behaviour changes.
 * ------------------------------------------------------------------ */

static Window ewmh_check_win = None;

void EwmhPublishDesktops( void )
{
	long n;
	char names[16*999], *p;
	int len, lp;

	n = Scr.n_desktop < 1 ? 1 : Scr.n_desktop;
	XChangeProperty( dpy, Scr.Root, _XA_NET_NUMBER_OF_DESKTOPS, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char *)&n, 1 );

	/* NUL-separated UTF8_STRING list: "Desk 0\0Desk 1\0..." - 0-based to
	   mirror mlvwm's own icon/window-list desktop labels. */
	p = names;
	for( lp=0; lp<n && (size_t)(p-names) < sizeof(names)-16; lp++ )
		p += snprintf( p, 16, "Desk %d", lp ) + 1;
	len = p - names;
	XChangeProperty( dpy, Scr.Root, _XA_NET_DESKTOP_NAMES, _XA_UTF8_STRING,
					 8, PropModeReplace, (unsigned char *)names, len );
}

void EwmhSetCurrentDesktop( void )
{
	long d = Scr.currentdesk;
	XChangeProperty( dpy, Scr.Root, _XA_NET_CURRENT_DESKTOP, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char *)&d, 1 );
}

void EwmhSetWindowDesktop( MlvwmWindow *t )
{
	long d;
	if( !t )		return;
	d = t->Desk;
	XChangeProperty( dpy, t->w, _XA_NET_WM_DESKTOP, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char *)&d, 1 );
}

void EwmhUpdateClientList( void )
{
	MlvwmWindow *t;
	Window *list;
	int n = 0, i = 0;

	for( t = Scr.MlvwmRoot.next; t != NULL; t = t->next )		n++;
	if( n == 0 ){
		XChangeProperty( dpy, Scr.Root, _XA_NET_CLIENT_LIST, XA_WINDOW,
						 32, PropModeReplace, (unsigned char *)&i, 0 );
		return;
	}
	list = calloc( n, sizeof(Window) );
	if( !list )		return;
	for( t = Scr.MlvwmRoot.next; t != NULL && i < n; t = t->next )
		list[i++] = t->w;
	XChangeProperty( dpy, Scr.Root, _XA_NET_CLIENT_LIST, XA_WINDOW,
					 32, PropModeReplace, (unsigned char *)list, i );
	free( list );
}

void EwmhSetActiveWindow( Window w )
{
	XChangeProperty( dpy, Scr.Root, _XA_NET_ACTIVE_WINDOW, XA_WINDOW,
					 32, PropModeReplace, (unsigned char *)&w, 1 );
}

void EwmhInit( void )
{
	long geom[2], view[2];
	long *work;
	int nd, lp;
	XSetWindowAttributes attr;

	/* _NET_SUPPORTING_WM_CHECK: a stable child window carrying _NET_WM_NAME,
	   referenced from both the root and itself, so clients can confirm an
	   EWMH-compliant WM is running. */
	attr.override_redirect = True;
	ewmh_check_win = XCreateWindow( dpy, Scr.Root, -100, -100, 1, 1, 0,
									CopyFromParent, InputOnly, CopyFromParent,
									CWOverrideRedirect, &attr );
	XChangeProperty( dpy, Scr.Root, _XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW,
					 32, PropModeReplace, (unsigned char *)&ewmh_check_win, 1 );
	XChangeProperty( dpy, ewmh_check_win, _XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW,
					 32, PropModeReplace, (unsigned char *)&ewmh_check_win, 1 );
	XChangeProperty( dpy, ewmh_check_win, _XA_NET_WM_NAME, _XA_UTF8_STRING,
					 8, PropModeReplace, (unsigned char *)"mlvwm", 5 );

	{
		Atom supported[] = {
			_XA_NET_SUPPORTED, _XA_NET_SUPPORTING_WM_CHECK,
			_XA_NET_NUMBER_OF_DESKTOPS, _XA_NET_CURRENT_DESKTOP,
			_XA_NET_DESKTOP_NAMES, _XA_NET_DESKTOP_GEOMETRY,
			_XA_NET_DESKTOP_VIEWPORT, _XA_NET_WORKAREA,
			_XA_NET_WM_DESKTOP, _XA_NET_CLIENT_LIST,
			_XA_NET_ACTIVE_WINDOW, _XA_NET_WM_NAME,
			_XA_NET_WM_WINDOW_TYPE, _XA_NET_WM_WINDOW_TYPE_DESKTOP,
		};
		XChangeProperty( dpy, Scr.Root, _XA_NET_SUPPORTED, XA_ATOM, 32,
						 PropModeReplace, (unsigned char *)supported,
						 sizeof(supported)/sizeof(supported[0]) );
	}

	geom[0] = Scr.MyDisplayWidth;	geom[1] = Scr.MyDisplayHeight;
	XChangeProperty( dpy, Scr.Root, _XA_NET_DESKTOP_GEOMETRY, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char *)geom, 2 );
	view[0] = 0;	view[1] = 0;
	XChangeProperty( dpy, Scr.Root, _XA_NET_DESKTOP_VIEWPORT, XA_CARDINAL,
					 32, PropModeReplace, (unsigned char *)view, 2 );
	/* Work area (excludes the top menu bar), one set of 4 CARDINALs per
	   desktop as required by the spec - identical for every desktop here. */
	nd = Scr.n_desktop < 1 ? 1 : Scr.n_desktop;
	work = calloc( nd * 4, sizeof(long) );
	if( work ){
		for( lp=0; lp<nd; lp++ ){
			work[lp*4+0] = 0;
			work[lp*4+1] = Scr.bar_width;
			work[lp*4+2] = Scr.MyDisplayWidth;
			work[lp*4+3] = Scr.MyDisplayHeight - Scr.bar_width;
		}
		XChangeProperty( dpy, Scr.Root, _XA_NET_WORKAREA, XA_CARDINAL,
						 32, PropModeReplace, (unsigned char *)work, nd*4 );
		free( work );
	}

	EwmhPublishDesktops();
	EwmhSetCurrentDesktop();
	EwmhUpdateClientList();
	EwmhSetActiveWindow( None );
}

int MappedNotOverride( Window w )
{
    XWindowAttributes wa;

    XGetWindowAttributes(dpy, w, &wa);
    return ((wa.map_state != IsUnmapped) && (wa.override_redirect != True));
}

void RepaintAllWindows( Window w )
{
	Window parent, *children;
	XWindowAttributes attributes;
	unsigned nchildren, lp;
	MlvwmWindow *t;

	XQueryTree( dpy, Scr.Root, &Scr.Root, &parent, &children, &nchildren );
	for( lp=0; nchildren > lp; lp++ ){
		if( children[lp]==w || children[lp]==Scr.MenuBar )	continue;
		XGetWindowAttributes( dpy, children[lp], &attributes );
		if( IsUnmapped ==attributes.map_state )	continue;
		if( children[lp] && MappedNotOverride(children[lp]) ){
			HandleMapRequest( children[lp] );
			if( XFindContext( dpy, children[lp],
							 MlvwmContext, (caddr_t *)&t)!=XCNOENT)
				DrawStringMenuBar( t->name );
		}
		XLowerWindow( dpy, children[lp] );
	}
	XFree( children );
}

void Reborder( Bool restart )
{
	MlvwmWindow *tmp;

	XGrabServer (dpy);
	InstallWindowColormaps( &Scr.MlvwmRoot );	/* force reinstall */
	for (tmp = (MlvwmWindow *)Scr.MlvwmRoot.next; tmp != NULL;
		 tmp = (MlvwmWindow *)tmp->next){
		XUnmapWindow(dpy,tmp->frame);
		RestoreWithdrawnLocation( tmp, restart );
    }
	XUngrabServer (dpy);
	XSetInputFocus (dpy, PointerRoot, RevertToPointerRoot,CurrentTime);
}

void SaveDesktopState( void )
{
	MlvwmWindow *t;
	unsigned long data[1];

	for (t = Scr.MlvwmRoot.next; t != NULL; t = t->next){
		data[0] = (unsigned long) t->Desk;
		XChangeProperty (dpy, t->w, _XA_WM_DESKTOP, _XA_WM_DESKTOP, 32,
						 PropModeReplace, (unsigned char *) data, 1);
		XChangeProperty (dpy, t->w, _XA_NET_WM_DESKTOP, XA_CARDINAL, 32,
						 PropModeReplace, (unsigned char *) data, 1);
    }

	data[0] = (unsigned long) Scr.currentdesk;
	XChangeProperty (dpy, Scr.Root, _XA_WM_DESKTOP, _XA_WM_DESKTOP, 32,
					 PropModeReplace, (unsigned char *) data, 1);
	XChangeProperty (dpy, Scr.Root, _XA_NET_CURRENT_DESKTOP, XA_CARDINAL, 32,
					 PropModeReplace, (unsigned char *) data, 1);

	XSync(dpy, 0);
}

void Done( int restart, char *command )
{
	char *my_argv[10];
	int i,done;

	strcpy( Scr.ErrorFunc, "Start Done" );

	FreeMenu();
	strcpy( Scr.ErrorFunc, "FreeMenu Done" );

	FreeShortCut();
	strcpy( Scr.ErrorFunc, "FreeShortCut Done" );

	FreeStyles();
	strcpy( Scr.ErrorFunc, "FreeStyles Done" );

	FreeFont();
	strcpy( Scr.ErrorFunc, "FreeFont Done" );

	if( Scr.LastActive )		free( Scr.LastActive );
	if( Scr.Balloon!=None )		XDestroyWindow( dpy, Scr.Balloon );

	Reborder( restart==0?False:True );
	strcpy( Scr.ErrorFunc, "Reborder Done" );

	if(restart){
		i=0;
		done = 0;
		while((g_argv[i] != NULL)&&(i<8)){
			if(strcmp(g_argv[i],"-s")==0)
				done = 1;
			my_argv[i] = g_argv[i];
			i++;
        }
		if(!done)
			my_argv[i++] = "-s";
		while(i<10)
			my_argv[i++] = NULL;
		SaveDesktopState();
		XSelectInput(dpy, Scr.Root, 0 );
		XSync(dpy, 0);
		XCloseDisplay(dpy);

		sleep( 1 );
		ReapChildren();

		if( command != NULL )
			execvp(command,g_argv);
		else
			execvp( *g_argv,g_argv);

		fprintf( stderr, "Call of '%s' failed!!!!", command);
		execvp( *g_argv, g_argv);
		fprintf( stderr, "Call of '%s' failed!!!!", g_argv[0]);
	}
	else{
		XCloseDisplay( dpy );
		exit(0);
	}
}

void LogFatal( const char *what );

void SigDone(int nonsense)
{
	fprintf( stderr, "Catch Signal in [%s]\n", Scr.ErrorFunc );
	LogFatal( "Terminating signal (SIGTERM/SIGINT/SIGHUP/SIGQUIT)" );
	Done(0, NULL);
	return;
}

void setsighandle( int sig )
{
	if( signal( sig, SIG_IGN ) != SIG_IGN )
		signal( sig, SigDone );
}

void usage( void )
{
	fprintf( stderr, "Mlvwm Ver %s\n\n", VERSION );
	fprintf( stderr, "mlvwm [-d display] [-f config_file]");
	fprintf( stderr, " [-debug]\n" );
}

XErrorHandler CatchRedirectError(Display *err_dpy, XErrorEvent *event)
{
	fprintf( stderr, "MLVWM : another WM may be running.\n" );
	exit(1);
}

XErrorHandler MlvwmErrorHandler(Display *err_dpy, XErrorEvent *event)
{
	char err_msg[80];

	/* some errors are acceptable, mostly they're caused by 
	 * trying to update a lost  window */
	if((event->error_code == BadWindow) ||
	   (event->request_code == X_GetGeometry) ||
	   (event->error_code==BadDrawable) ||
	   (event->request_code==X_SetInputFocus) ||
	   (event->request_code == X_InstallColormap))
		return 0 ;

	XGetErrorText( err_dpy, event->error_code, err_msg, 80 );
	fprintf( stderr, "MLVWM : X Protocol error\n" );
	fprintf( stderr, "   Error detected : %s\n", err_msg );
	fprintf( stderr, "      Protocol Request : %d\n", event->request_code );
	fprintf( stderr, "      Error            : %d\n", event->error_code);
	fprintf( stderr, "      Resource ID      : 0x%x\n", 
			(unsigned int)event->resourceid );
	fprintf( stderr,"\n");
	return 0;
}

Window CreateStartWindow( void )
{
	Pixmap p_map;
	unsigned long valuemask;
	Window StartWin;
	XSetWindowAttributes attributes;

	if( Scr.flags & DEBUGOUT )
		fprintf( stderr, "Display Startup Screen !\n" ); 
	p_map = XCreatePixmapFromBitmapData( dpy, Scr.Root, start_mesh, 4, 4, 
								WhitePixel( dpy, Scr.screen ),
								BlackPixel( dpy, Scr.screen ), Scr.d_depth );
	if( Scr.flags & DEBUGOUT )
		fprintf( stderr, "Pixmap Create !\n" ); 
	valuemask = CWBackPixmap | CWBackingStore | CWCursor;
	attributes.background_pixmap = p_map;
	attributes.cursor = Scr.MlvwmCursors[WAIT];
	attributes.backing_store = NotUseful;
	StartWin = XCreateWindow (dpy, Scr.Root, 0, 0,
							  (unsigned int) Scr.MyDisplayWidth,
							  (unsigned int) Scr.MyDisplayHeight,
							  (unsigned int) 0,
							  CopyFromParent, (unsigned int) CopyFromParent,
							  (Visual *) CopyFromParent, valuemask,
							  &attributes);
	XMapRaised (dpy, StartWin);
	XFreePixmap( dpy, p_map );
	XFlush (dpy);

	return StartWin;
}

/* Record a fatal event (signal or X error) plus a backtrace to stderr and to
   ~/.mlvwm-crash.log, since at real login mlvwm's stderr is discarded by the
   display manager.  Async-signal-safety is best-effort: backtrace_symbols_fd
   and write are safe; the snprintf header is a pragmatic compromise. */
void LogFatal( const char *what )
{
	void *bt[64];
	int n;
	const char *home;
	char path[512];
	int fd;

	fprintf( stderr, "MLVWM fatal: %s (in %s)\n", what, Scr.ErrorFunc );
	n = backtrace( bt, 64 );
	backtrace_symbols_fd( bt, n, 2 );

	home = getenv( "HOME" );
	if( home ){
		time_t t = time( NULL );
		snprintf( path, sizeof(path), "%s/.mlvwm-crash.log", home );
		fd = open( path, O_WRONLY|O_CREAT|O_APPEND, 0644 );
		if( fd >= 0 ){
			char hdr[256];
			int len = snprintf( hdr, sizeof(hdr),
				"\n--- %s at %.24s in '%s' ---\n",
				what, ctime(&t), Scr.ErrorFunc );
			(void)write( fd, hdr, len );
			backtrace_symbols_fd( bt, n, fd );
			close( fd );
		}
	}
}

void SegFault( int sig )
{
	const char *name;
	switch( sig ){
		case SIGSEGV: name = "Segmentation Fault (SIGSEGV)"; break;
		case SIGABRT: name = "Abort (SIGABRT)";              break;
		case SIGBUS:  name = "Bus error (SIGBUS)";           break;
		case SIGFPE:  name = "Arithmetic error (SIGFPE)";    break;
		case SIGILL:  name = "Illegal instruction (SIGILL)"; break;
		default:      name = "Fatal signal";                 break;
	}
	LogFatal( name );
	exit( -1 );
}

/* The Xlib default I/O-error handler just exits silently when the X server
   connection dies, which made an early session teardown indistinguishable
   from a clean exit.  Log it instead. */
int MlvwmIOErrorHandler( Display *err_dpy )
{
	LogFatal( "X I/O error (server connection lost)" );
	exit( 1 );
	return 0;
}

void DoStartFunc( void )
{
	ShortCut *now, *prev;

	now = Scr.StartFunc;
	while( now ){
		ExecuteFunction( now->action );
		prev = now;
		now = now->next;
		free( prev->action );
		free( prev );
	}
}

int main( int argc, char *argv[] )
{
	char *display_name=NULL;
	char *display_screen;
	char *display_string;
	char *config_file=NULL;
	char message[255];
	char *cp;
	Window StartWin;
	XSetWindowAttributes attributes;
	int len, lp;
	Bool single = False;

	Scr.flags = 0;
	for( lp=1; lp<argc; lp++ ){
		if( !strncmp( argv[lp], "-d", 2 ) && strlen(argv[lp])==2 ){
			if( ++lp>=argc )	usage();
			else				display_name = argv[lp];
			continue;
		}
		if( !strncmp( argv[lp], "-f", 2 ) ){
			if( ++lp>=argc )	usage();
			else				config_file = argv[lp];
			continue;
		}
		if( !strncmp( argv[lp], "-s", 2 ) ){
			single = True;
			continue;
		}
		if( !strncmp( argv[lp], "-debug", 6 )){
			Scr.flags |= DEBUGOUT;
			continue;
		}
		usage();
	}
	if( Scr.flags & DEBUGOUT )
		fprintf( stderr, "Welcome to MLVWM World !\n" );
#ifdef USE_LOCALE
	if( setlocale( LC_CTYPE, "" )==NULL ){
		fprintf( stderr, "Can't figure out your locale.\n" );
		fprintf( stderr, "Check $LANG.\n" );
	}
	if( XSupportsLocale() == False ){
		fprintf( stderr, "Can't support your local.\n" );
		fprintf( stderr, "Use \"C\" locale.\n" );
		setlocale( LC_CTYPE, "C" );
	}
#endif
	if( !(dpy = XOpenDisplay( display_name )) ){
		fprintf( stderr, "can't open display %s\n",XDisplayName(display_name));
		exit( 1 );
	}
    xfd = XConnectionNumber(dpy);
    
    if( fcntl( xfd, F_SETFD, 1 ) == -1){
        fprintf( stderr, "Close-on-exec failed\n" );
        exit (1);
	}

	Scr.screen = DefaultScreen( dpy );

	g_argv = argv;

	setsighandle( SIGINT );
	setsighandle( SIGHUP );
	setsighandle( SIGQUIT );
	setsighandle( SIGTERM );
	signal( SIGSEGV, SegFault );
	signal( SIGABRT, SegFault );
	signal( SIGBUS,  SegFault );
	signal( SIGFPE,  SegFault );
	signal( SIGILL,  SegFault );
	XSetIOErrorHandler( MlvwmIOErrorHandler );
    Scr.NumberOfScreens = ScreenCount(dpy);

    if(!single){
        for(lp=0;lp<Scr.NumberOfScreens;lp++){
            if(lp!= Scr.screen){
                len = strlen(XDisplayString(dpy)) + 10;
                display_screen = calloc(len, sizeof(char));
                snprintf(display_screen, len, "%s", XDisplayString(dpy));
                /*
                 * Truncate the string 'whatever:n.n' to 'whatever:n',
                 * and then append the screen number.
                 */
                cp = strchr(display_screen, ':');
                if (cp != NULL)
                {
                  cp = strchr(cp, '.');
                  if (cp != NULL)
                  *cp = '\0';  /* truncate at display part */
                }
                snprintf(message, sizeof(message), "%s -d %s.%d -s", argv[0], display_screen, lp);
                free(display_screen);

                if( Scr.flags & DEBUGOUT)
                    snprintf(message + strlen(message), sizeof(message) - strlen(message), " -debug");
                if( config_file != NULL )
                    snprintf(message + strlen(message), sizeof(message) - strlen(message) ," -f %s", config_file);
                snprintf(message + strlen(message), sizeof(message) - strlen(message), " &\n");

                system(message);
			}
		}
	}
	len = strlen( XDisplayString( dpy ) );
	display_string = calloc( len+10, sizeof( char ) );
	snprintf( display_string, len+10, "DISPLAY=%s", XDisplayString(dpy) );
	putenv( display_string );

	XShapeQueryExtension( dpy, &ShapeEventBase, &ShapeErrorBase );
	InternUsefulAtoms();

	Scr.Root = RootWindow( dpy, Scr.screen );
	if( Scr.Root == None ){
		fprintf( stderr, "Root window don't exist\n" );
		exit( 1 );
	}
    XChangeProperty (dpy, Scr.Root, _XA_MIT_PRIORITY_COLORS,
                     XA_CARDINAL, 32, PropModeReplace, NULL, 0);
    XSetErrorHandler((XErrorHandler)CatchRedirectError);
	XSelectInput( dpy, Scr.Root,
				 PropertyChangeMask |
				 SubstructureRedirectMask | KeyPressMask |
				 SubstructureNotifyMask |
				 ButtonPressMask | ButtonReleaseMask );
	XSync( dpy, 0 );

    XSetErrorHandler((XErrorHandler)MlvwmErrorHandler);

	CreateCursors();
	InitVariables();
	XGrabServer( dpy );

    attributes.event_mask = KeyPressMask|FocusChangeMask;
    attributes.override_redirect = True;
    Scr.NoFocusWin=XCreateWindow(dpy,Scr.Root,-10, -10, 10, 10, 0, 0,
                                 InputOnly,CopyFromParent,
                                 CWEventMask|CWOverrideRedirect,
                                 &attributes);
    XMapWindow(dpy, Scr.NoFocusWin);
    XSetInputFocus (dpy, Scr.NoFocusWin, RevertToParent, CurrentTime);

	StartWin = CreateStartWindow();
	CreateMenuBar();
	LoadDefaultFonts();

	if( Scr.flags & DEBUGOUT )
		DrawStringMenuBar( "Read Config File !" );
	ReadConfigFile( config_file? config_file : CONFIGNAME );
	if( Scr.flags & DEBUGOUT ){
		DrawStringMenuBar( "Read Config File Success !" );
		XSynchronize(dpy,1);
	}
	XUngrabServer( dpy );
	if( !Scr.MenuLabelRoot )	CreateSimpleMenu();
	CreateMenuItems();
	for( Scr.iconAnchor = Scr.IconMenu.m_item;
		Scr.iconAnchor->next->next != NULL;
		Scr.iconAnchor = Scr.iconAnchor->next );
	RepaintAllWindows( StartWin );
	InitDesktop();
	EwmhInit();
	if( Scr.StartFunc ) DoStartFunc();

	XDestroyWindow (dpy, StartWin);

	sprintf( message, "Desk %d", Scr.currentdesk );
	DrawStringMenuBar( "" );
	ChangeDesk( message );
	Scr.flags &= ~STARTING;
	MapMenuBar( Scr.ActiveWin );

	while( True )		WaitEvents();
	return 0;
}
