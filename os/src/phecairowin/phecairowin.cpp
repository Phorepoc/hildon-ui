// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phecairowin.cpp -- cairo into a WSERV window (step 2 of docs/phorepoc/hildon-port.md, the visible end of it). A cairo RGB24 image surface is created over the
// pixel memory of a CFbsBitmap (EColor16MU = 0x00RRGGBB, the same layout), cairo draws, the bitmap is BitBlt'ed into a full-screen RWindow by the normal window gc.
// WSERV skeleton as phedynres (core/os/src/phedynres).
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>
#include <stdio.h>
#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cairo-ft.h>

static const char* KFontFile = "z:/resource/fonts/s60sans_r.ttf";

// draw the picture into an RGB24 surface of w x h (stride in bytes); returns the number of dark/non-background text pixels for the log
static TInt Paint(TUint8* aData, TInt aW, TInt aH, TInt aStride)
	{
	cairo_surface_t* s = cairo_image_surface_create_for_data(aData, CAIRO_FORMAT_RGB24, aW, aH, aStride);
	cairo_t* cr = cairo_create(s);
	const double w = aW, h = aH;
	// background: a vertical gradient
	cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, h);
	cairo_pattern_add_color_stop_rgb(g, 0, 0.10, 0.16, 0.30);
	cairo_pattern_add_color_stop_rgb(g, 1, 0.02, 0.05, 0.10);
	cairo_set_source(cr, g);
	cairo_paint(cr);
	cairo_pattern_destroy(g);
	// a translucent rounded card
	const double m = w * 0.08, cw = w - 2 * m, ch = h * 0.34, cy = h * 0.12, r = w * 0.05;
	cairo_new_sub_path(cr);
	cairo_arc(cr, m + cw - r, cy + r, r, -1.5708, 0);
	cairo_arc(cr, m + cw - r, cy + ch - r, r, 0, 1.5708);
	cairo_arc(cr, m + r, cy + ch - r, r, 1.5708, 3.14159);
	cairo_arc(cr, m + r, cy + r, r, 3.14159, 4.71239);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.16);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.55);
	cairo_set_line_width(cr, 2);
	cairo_stroke(cr);
	// circles and a diagonal stroke below the card
	cairo_set_line_width(cr, 3);
	cairo_set_source_rgb(cr, 0.95, 0.75, 0.15);
	cairo_arc(cr, w * 0.30, h * 0.64, w * 0.14, 0, 6.28318);
	cairo_stroke(cr);
	cairo_set_source_rgba(cr, 0.30, 0.75, 0.95, 0.8);
	cairo_arc(cr, w * 0.62, h * 0.64, w * 0.14, 0, 6.28318);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.9, 0.35, 0.35);
	cairo_move_to(cr, m, h * 0.84);
	cairo_curve_to(cr, w * 0.35, h * 0.74, w * 0.65, h * 0.94, w - m, h * 0.84);
	cairo_stroke(cr);
	// text through FreeType and cairo-ft
	TInt textPixels = -1;
	FT_Library lib;
	FT_Face face;
	if (FT_Init_FreeType(&lib) == 0)
		{
		if (FT_New_Face(lib, KFontFile, 0, &face) == 0)
			{
			cairo_font_face_t* ff = cairo_ft_font_face_create_for_ft_face(face, 0);
			cairo_set_font_face(cr, ff);
			cairo_set_source_rgb(cr, 1, 1, 1);
			cairo_set_font_size(cr, w * 0.095);
			cairo_move_to(cr, m + r, cy + ch * 0.45);
			cairo_show_text(cr, "Hildon stack");
			cairo_set_font_size(cr, w * 0.052);
			cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
			cairo_move_to(cr, m + r, cy + ch * 0.68);
			cairo_show_text(cr, "GLib + cairo + FreeType");
			cairo_move_to(cr, m + r, cy + ch * 0.86);
			cairo_show_text(cr, "on Phorepoc, x32");
			textPixels = 1;
			cairo_font_face_destroy(ff);
			}
		else
			RDebug::Printf("[phecairowin] FT_New_Face failed");
		}
	cairo_surface_flush(s);
	const cairo_status_t st = cairo_status(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(s);
	if (textPixels > 0 && face) { FT_Done_Face(face); FT_Done_FreeType(lib); }
	RDebug::Printf("[phecairowin] cairo %s painted %dx%d status=%d text=%d", cairo_version_string(), aW, aH, (TInt)st, textPixels);
	return textPixels;
	}

static void RenderL(CFbsBitmap& aBmp)
	{
	const TSize sz = aBmp.SizeInPixels();
	aBmp.LockHeap();
	Paint((TUint8*)aBmp.DataAddress(), sz.iWidth, sz.iHeight, aBmp.DataStride());
	aBmp.UnlockHeap();
	}

static void Blit(CWindowGc& aGc, RWindow& aWin, CFbsBitmap& aBmp)
	{
	aGc.Activate(aWin);
	aWin.BeginRedraw();
	aGc.BitBlt(TPoint(0, 0), &aBmp);
	aWin.EndRedraw();
	aGc.Deactivate();
	}

static void MainL()
	{
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	User::LeaveIfError(screen->Construct());
	CWindowGc* gc = NULL;
	User::LeaveIfError(screen->CreateContext(gc));
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x50484331, ETrue));
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x50484332));
	const TSize size = screen->SizeInPixels();
	win.SetExtent(TPoint(0, 0), size);
	win.SetBackgroundColor(KRgbBlack);
	win.Activate();

	CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
	User::LeaveIfError(bmp->Create(size, EColor16MU));
	RenderL(*bmp);
	Blit(*gc, win, *bmp);
	ws.Flush();
	RDebug::Printf("[phecairowin] blitted %dx%d into the window", size.iWidth, size.iHeight);

	TRequestStatus redrawSt, eventSt;
	ws.RedrawReady(&redrawSt);
	ws.EventReady(&eventSt);
	for (;;)
		{
		User::WaitForAnyRequest();
		if (redrawSt != KRequestPending)
			{
			TWsRedrawEvent rev;
			ws.GetRedraw(rev);
			Blit(*gc, win, *bmp);
			ws.Flush();
			ws.RedrawReady(&redrawSt);
			}
		else if (eventSt != KRequestPending)
			{
			TWsEvent ev;
			ws.GetEvent(ev);
			ws.EventReady(&eventSt);
			}
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	RDebug::Printf("[phecairowin] exit err=%d", err);
	delete cleanup;
	return err;
	}
