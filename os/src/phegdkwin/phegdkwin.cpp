// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phegdkwin.cpp -- the WSERV half of the gdk-symbian skeleton (docs/phorepoc/hildon-port.md 7.8-7.11): a full-screen RWindow showing a CFbsBitmap whose pixel memory is
// the cairo surface of a PheGdkWindow. Plain C ABI to the C half (phegdk.h: phe_ws_init / phe_ws_poll / phe_ws_pending / phe_ws_wake_fd / phe_ws_present).
//
// Threads. An RWsSession is NOT shareable between threads (CreateSession is unsharable; ShareAuto panics KERN-EXEC 23), so the WSERV objects (session, screen, window, gc,
// the bitmap handle) live in ONE helper thread, "WSERV": it waits (User::WaitForAnyRequest) for EventReady, RedrawReady and a command request. Events go into a ring queue plus
// one byte in a pipe, whose read end the GLib main loop select()s on, so an idle loop blocks with NO timeout and a WSERV event wakes it exactly when it arrives. The main
// thread's phe_ws_present() queues a BitBlt command and wakes the helper with RThread::RequestComplete on the command request.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

extern "C" {
typedef struct { int type; int x, y, w, h; int code, scancode, ptype; } PheWsEvent;
void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }
int phe_ws_init(unsigned char** aPixels, int* aW, int* aH, int* aStride);
int phe_ws_poll(PheWsEvent* aEv);
void phe_ws_present(int aX, int aY, int aW, int aH);
int phe_ws_pending(void);
int phe_ws_wake_fd(void);
struct PheGdkDisplay;
struct PheGdkWindow;
PheGdkDisplay* phe_gdk_display_open(void);
PheGdkWindow* phe_gdk_window_new_toplevel(PheGdkDisplay* aDisplay);
void phe_gdk_demo_run(PheGdkDisplay* aDisplay, PheGdkWindow* aWin);
}

// ---- shared state (all under gMutex) ----
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static const int KQ = 256;
static PheWsEvent gEvQ[256];          // events for the main thread
static int gEvHead, gEvTail;
struct TCmd { int x, y, w, h; };
static TCmd gCmdQ[64];                // BitBlt commands for the helper
static int gCmdHead, gCmdTail;
static TBool gHelperArmed;            // the helper's command request is pending (main must RequestComplete it to wake it)
static TRequestStatus* gCmdStPtr;
static TThreadId gHelperId;
static int gPipe[2] = { -1, -1 };

// ---- helper-thread-owned WSERV objects ----
static RWsSession gWs;
static CWsScreenDevice* gScreen;
static CWindowGc* gGc;
static RWindowGroup* gGroup;
static RWindow* gWin;
static CFbsBitmap* gBmp;

// init hand-over
static pthread_mutex_t gInitMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gInitCond = PTHREAD_COND_INITIALIZER;
static int gInitDone, gInitErr, gRedraws;
static unsigned char* gPixels;
static int gW, gH, gStride;

static void PushEv(const PheWsEvent& aEv)
	{
	pthread_mutex_lock(&gMutex);
	const int next = (gEvTail + 1) % KQ;
	if (next != gEvHead)
		{
		gEvQ[gEvTail] = aEv;
		gEvTail = next;
		}
	pthread_mutex_unlock(&gMutex);
	}

static TBool Translate(const TWsEvent& aWs, PheWsEvent& aEv)
	{
	memset(&aEv, 0, sizeof(aEv));
	switch (aWs.Type())
		{
	case EEventKey:
		aEv.type = 1; aEv.ptype = 0; aEv.code = aWs.Key()->iCode; aEv.scancode = aWs.Key()->iScanCode;
		return ETrue;
	case EEventKeyDown:
		aEv.type = 1; aEv.ptype = 1; aEv.scancode = aWs.Key()->iScanCode;
		return ETrue;
	case EEventKeyUp:
		aEv.type = 1; aEv.ptype = 2; aEv.scancode = aWs.Key()->iScanCode;
		return ETrue;
	case EEventPointer:
		aEv.type = 2; aEv.x = aWs.Pointer()->iPosition.iX; aEv.y = aWs.Pointer()->iPosition.iY; aEv.ptype = aWs.Pointer()->iType;
		return ETrue;
	default:
		return EFalse;
		}
	}

static void Blit(int aX, int aY, int aW, int aH)
	{
	const TRect r(TPoint(aX, aY), TSize(aW, aH));
	gGc->Activate(*gWin);
	gWin->BeginRedraw(r);
	gGc->BitBlt(TPoint(aX, aY), gBmp, r);
	gWin->EndRedraw();
	gGc->Deactivate();
	gWs.Flush();
	}

static int SetupWs()
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
	gPixels = (unsigned char*)gBmp->DataAddress();
	gW = size.iWidth;
	gH = size.iHeight;
	gStride = gBmp->DataStride();
	return 0;
	}

static void* WsThread(void*)
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	gHelperId = RThread().Id();
	const int err = SetupWs();
	pthread_mutex_lock(&gInitMutex);
	gInitErr = err;
	gInitDone = 1;
	pthread_cond_signal(&gInitCond);
	pthread_mutex_unlock(&gInitMutex);
	if (err != 0)
		return NULL;

	TRequestStatus redrawSt, eventSt, cmdSt;
	gWs.RedrawReady(&redrawSt);
	gWs.EventReady(&eventSt);
	pthread_mutex_lock(&gMutex);
	cmdSt = KRequestPending;
	gCmdStPtr = &cmdSt;
	gHelperArmed = ETrue;
	pthread_mutex_unlock(&gMutex);
	for (;;)
		{
		User::WaitForAnyRequest();
		TBool any = EFalse;
		if (redrawSt != KRequestPending)
			{
			TWsRedrawEvent rev;
			gWs.GetRedraw(rev);
			const TRect r = rev.Rect();
			// a redraw request is answered right here, in the thread that owns the window, from the backing bitmap (so the region is valid again before we re-ask)
			if (r.Width() > 0 && r.Height() > 0)
				{
				Blit(r.iTl.iX, r.iTl.iY, r.Width(), r.Height());
				if (gRedraws++ < 3)
					RDebug::Printf("[phegdkwin] redraw request answered from the backing bitmap: %d,%d %dx%d", r.iTl.iX, r.iTl.iY, r.Width(), r.Height());
				}
			gWs.RedrawReady(&redrawSt);
			}
		if (eventSt != KRequestPending)
			{
			TWsEvent ev;
			gWs.GetEvent(ev);
			gWs.EventReady(&eventSt);
			PheWsEvent e;
			if (Translate(ev, e))
				{
				PushEv(e);
				any = ETrue;
				}
			}
		if (any)
			{
			const char c = 1;
			write(gPipe[1], &c, 1);
			}
		if (cmdSt != KRequestPending)
			{
			// the main thread completed our command request: run every queued BitBlt, then re-arm (under the mutex, so no wake-up is lost)
			for (;;)
				{
				pthread_mutex_lock(&gMutex);
				if (gCmdHead == gCmdTail)
					{
					cmdSt = KRequestPending;
					gHelperArmed = ETrue;
					pthread_mutex_unlock(&gMutex);
					break;
					}
				const TCmd c = gCmdQ[gCmdHead];
				gCmdHead = (gCmdHead + 1) % 64;
				pthread_mutex_unlock(&gMutex);
				Blit(c.x, c.y, c.w, c.h);
				}
			}
		}
	return NULL;
	}

extern "C" int phe_ws_init(unsigned char** aPixels, int* aW, int* aH, int* aStride)
	{
	if (pipe(gPipe) != 0) return -8;
	fcntl(gPipe[0], F_SETFL, O_NONBLOCK);
	pthread_t th;
	pthread_attr_t at;
	pthread_attr_init(&at);
	pthread_attr_setstacksize(&at, 0x20000);
	if (pthread_create(&th, &at, WsThread, NULL) != 0) return -9;
	pthread_mutex_lock(&gInitMutex);
	while (!gInitDone)
		pthread_cond_wait(&gInitCond, &gInitMutex);
	pthread_mutex_unlock(&gInitMutex);
	if (gInitErr != 0) return gInitErr;
	*aPixels = gPixels; *aW = gW; *aH = gH; *aStride = gStride;
	return 0;
	}

// main thread: queue a BitBlt of the rectangle and wake the WSERV thread
extern "C" void phe_ws_present(int aX, int aY, int aW, int aH)
	{
	TRequestStatus* wake = NULL;
	pthread_mutex_lock(&gMutex);
	const int next = (gCmdTail + 1) % 64;
	if (next != gCmdHead)
		{
		gCmdQ[gCmdTail].x = aX; gCmdQ[gCmdTail].y = aY; gCmdQ[gCmdTail].w = aW; gCmdQ[gCmdTail].h = aH;
		gCmdTail = next;
		}
	if (gHelperArmed)
		{
		gHelperArmed = EFalse;
		wake = gCmdStPtr;
		}
	pthread_mutex_unlock(&gMutex);
	if (wake)
		{
		RThread helper;
		if (helper.Open(gHelperId) == KErrNone)
			{
			helper.RequestComplete(wake, KErrNone);
			helper.Close();
			}
		}
	}

extern "C" int phe_ws_poll(PheWsEvent* aEv)
	{
	int got = 0;
	pthread_mutex_lock(&gMutex);
	if (gEvHead != gEvTail)
		{
		*aEv = gEvQ[gEvHead];
		gEvHead = (gEvHead + 1) % KQ;
		got = 1;
		}
	pthread_mutex_unlock(&gMutex);
	return got;
	}

extern "C" int phe_ws_pending(void)
	{
	pthread_mutex_lock(&gMutex);
	const int p = gEvHead != gEvTail;
	pthread_mutex_unlock(&gMutex);
	return p;
	}

extern "C" int phe_ws_wake_fd(void)
	{
	return gPipe[0];
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
