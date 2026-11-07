// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phegdkwin.cpp -- the WSERV half of the gdk-symbian skeleton (docs/phorepoc/hildon-port.md 7.8): a full-screen RWindow showing a CFbsBitmap whose pixel memory is
// the cairo surface of a PheGdkWindow. Exposes a plain C ABI (phegdk.h: phe_ws_init / phe_ws_poll / phe_ws_present) to the C half (phegdk_display.c: the GLib
// main-loop source; phegdkwin_demo.c). The request statuses of RWsSession::EventReady / RedrawReady stay armed here; phe_ws_poll fetches what completed.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>
#include <stdlib.h>

extern "C" {
typedef struct { int type; int x, y, w, h; int code, scancode, ptype; } PheWsEvent;
void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }
int phe_ws_init(unsigned char** aPixels, int* aW, int* aH, int* aStride);
int phe_ws_poll(PheWsEvent* aEv);
void phe_ws_present(int aX, int aY, int aW, int aH);
struct PheGdkDisplay;
struct PheGdkWindow;
PheGdkDisplay* phe_gdk_display_open(void);
PheGdkWindow* phe_gdk_window_new_toplevel(PheGdkDisplay* aDisplay);
void phe_gdk_demo_run(PheGdkDisplay* aDisplay, PheGdkWindow* aWin);
}

static RWsSession gWs;
static CWsScreenDevice* gScreen;
static CWindowGc* gGc;
static RWindowGroup* gGroup;
static RWindow* gWin;
static CFbsBitmap* gBmp;
static TRequestStatus gRedrawSt, gEventSt;

extern "C" int phe_ws_init(unsigned char** aPixels, int* aW, int* aH, int* aStride)
	{
	if (gWs.Connect() != KErrNone) return -1;
	gScreen = new CWsScreenDevice(gWs);
	if (!gScreen || gScreen->Construct() != KErrNone) return -2;
	if (gScreen->CreateContext(gGc) != KErrNone) return -3;
	gGroup = new RWindowGroup(gWs);
	if (gGroup->Construct(0x50484333, ETrue) != KErrNone) return -4;
	gWin = new RWindow(gWs);
	if (gWin->Construct(*gGroup, 0x50484334) != KErrNone) return -5;
	const TSize size = gScreen->SizeInPixels();
	gWin->SetExtent(TPoint(0, 0), size);
	gWin->SetBackgroundColor(KRgbBlack);
	gWin->Activate();
	gBmp = new CFbsBitmap;
	if (!gBmp || gBmp->Create(size, EColor16MU) != KErrNone) return -6;
	gBmp->LockHeap();                       // the pixel memory stays at this address for the cairo surface (the bitmap is never compressed or moved here)
	*aPixels = (unsigned char*)gBmp->DataAddress();
	*aW = size.iWidth;
	*aH = size.iHeight;
	*aStride = gBmp->DataStride();
	gWs.RedrawReady(&gRedrawSt);
	gWs.EventReady(&gEventSt);
	return 0;
	}

extern "C" void phe_ws_present(int aX, int aY, int aW, int aH)
	{
	const TRect r(TPoint(aX, aY), TSize(aW, aH));
	gGc->Activate(*gWin);
	gWin->BeginRedraw(r);
	gGc->BitBlt(TPoint(aX, aY), gBmp, r);
	gWin->EndRedraw();
	gGc->Deactivate();
	gWs.Flush();
	}

extern "C" int phe_ws_poll(PheWsEvent* aEv)
	{
	aEv->type = 0;
	if (gRedrawSt != KRequestPending)
		{
		TWsRedrawEvent rev;
		gWs.GetRedraw(rev);
		gWs.RedrawReady(&gRedrawSt);
		const TRect r = rev.Rect();
		aEv->type = 3;
		aEv->x = r.iTl.iX; aEv->y = r.iTl.iY; aEv->w = r.Width(); aEv->h = r.Height();
		return 1;
		}
	if (gEventSt != KRequestPending)
		{
		TWsEvent ev;
		gWs.GetEvent(ev);
		gWs.EventReady(&gEventSt);
		switch (ev.Type())
			{
		case EEventKey:
			aEv->type = 1; aEv->code = ev.Key()->iCode; aEv->scancode = ev.Key()->iScanCode;
			return 1;
		case EEventPointer:
			aEv->type = 2; aEv->x = ev.Pointer()->iPosition.iX; aEv->y = ev.Pointer()->iPosition.iY; aEv->ptype = ev.Pointer()->iType;
			return 1;
		default:
			return phe_ws_poll(aEv);        // not interesting: look at the next completed request
			}
		}
	return 0;
	}

static void MainL()
	{
	setenv("FONTCONFIG_FILE", "z:/resource/phegdkwin/fonts.conf", 1);
	PheGdkDisplay* d = phe_gdk_display_open();
	User::LeaveIfNull(d);
	PheGdkWindow* w = phe_gdk_window_new_toplevel(d);
	User::LeaveIfNull(w);
	phe_gdk_demo_run(d, w);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	RDebug::Printf("[phegdkwin] exit err=%d", err);
	delete cleanup;
	return err;
	}
