// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phegdkwin.cpp -- the WSERV half of the gdk-symbian skeleton (docs/phorepoc/hildon-port.md 3, step "GDK backend"): a full-screen RWindow that shows a CFbsBitmap
// whose pixel memory is the cairo surface of a PheGdkWindow (phegdk.h). The drawing (Pango layouts, a gdk-pixbuf PNG) is C, in phegdkwin_paint.c.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>
#include <stdlib.h>

extern "C" {
void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }
struct PheGdkWindow;
PheGdkWindow* phe_gdk_window_new(unsigned char* aPixels, int aW, int aH, int aStride);
int phe_gdk_demo_paint(PheGdkWindow* aWin);
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
	setenv("FONTCONFIG_FILE", "z:/resource/phegdkwin/fonts.conf", 1);
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	User::LeaveIfError(screen->Construct());
	CWindowGc* gc = NULL;
	User::LeaveIfError(screen->CreateContext(gc));
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x50484333, ETrue));
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x50484334));
	const TSize size = screen->SizeInPixels();
	win.SetExtent(TPoint(0, 0), size);
	win.SetBackgroundColor(KRgbBlack);
	win.Activate();

	CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
	User::LeaveIfError(bmp->Create(size, EColor16MU));
	bmp->LockHeap();
	PheGdkWindow* gw = phe_gdk_window_new((unsigned char*)bmp->DataAddress(), size.iWidth, size.iHeight, bmp->DataStride());
	const TInt rc = phe_gdk_demo_paint(gw);
	bmp->UnlockHeap();
	RDebug::Printf("[phegdkwin] demo painted rc=%d", rc);
	Blit(*gc, win, *bmp);
	ws.Flush();
	RDebug::Printf("[phegdkwin] blitted %dx%d into the window", size.iWidth, size.iHeight);

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
	RDebug::Printf("[phegdkwin] exit err=%d", err);
	delete cleanup;
	return err;
	}
