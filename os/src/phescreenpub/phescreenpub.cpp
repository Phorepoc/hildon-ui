// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phescreenpub -- publish a (virtual) WSERV screen as one window of the window registry (phewinreg), so a compositor shows a
// classic app that runs on its own screen as a decorated window (multi-window stage 2).
//
//   phescreenpub <screen>
//
// The screen's framebuffer is the display driver's RAM buffer (HAL EDisplayMemoryAddress of HAL device <screen>); the driver
// keeps what changed since the last read (display HAL function 0x7103 on that device: x0,y0,x1,y1,generation). Each change is
// copied into a CFbsBitmap that the registry hands to the compositor (RPheWinReg::Content), as every native window does.
// The window's title = the caption of the focused application on that screen. The frame's focus action moves WSERV's key (and
// pointer) focus to this screen (RWsSession::SetFocusScreen, WriteDeviceData).
#include <e32base.h>
#include <e32svr.h>
#include <u32hal.h>
#include <hal.h>
#include <fbs.h>
#include <w32std.h>
#include <apgwgnam.h>
#include "phewinreg.h"

static const TInt KPheDisplayHalDamage = 0x7103;		// the x86 display driver, virtual screens (xk_display.cpp)
static const TInt KPollUs = 20000;						// 50 Hz, the display flusher's own rate

static void Log(TRefByValue<const TDesC> aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	TBuf<256> b(_L("[phescreenpub] "));
	b.AppendFormatList(aFmt, list);
	RDebug::Print(_L("%S"), &b);
	}

class CScreenPub : public CBase
	{
public:
	~CScreenPub();
	void ConstructL(TInt aScreen);
	void RunL();
private:
	TBool CopyDamage(TRect& aRect);
	void UpdateTitle();
	void InjectPointer(const TPheWinPointer& aEv);
	void AnswerPing();
	TInt iScreen;
	TUint32* iFb;
	TInt iStride;					// words per line
	TSize iSize;
	RPheWinReg iReg;
	RWsSession iWs;
	CWsScreenDevice* iDevice;		// this screen (the session's primary device: SetPointerCursorArea acts on it)
	CFbsBitmap* iBitmap;
	TPheWinInfo iInfo;
	TUint iGen;
	};

CScreenPub::~CScreenPub()
	{
	if (iInfo.iId)
		iReg.Remove(iInfo.iId);
	iReg.Close();
	delete iBitmap;
	delete iDevice;
	iWs.Close();
	}

void CScreenPub::ConstructL(TInt aScreen)
	{
	iScreen = aScreen;
	TInt addr = 0, w = 0, h = 0, pitch = 0, mode = 0;
	User::LeaveIfError(HAL::Get(aScreen, HALData::EDisplayMode, mode));
	User::LeaveIfError(HAL::Get(aScreen, HALData::EDisplayMemoryAddress, addr));
	User::LeaveIfError(HAL::Get(aScreen, HALData::EDisplayXPixels, w));
	User::LeaveIfError(HAL::Get(aScreen, HALData::EDisplayYPixels, h));
	pitch = mode;													// EDisplayOffsetBetweenLines takes the mode as its in-value
	User::LeaveIfError(HAL::Get(aScreen, HALData::EDisplayOffsetBetweenLines, pitch));
	iFb = (TUint32*)addr;
	iStride = pitch / 4;
	iSize.SetSize(w, h);
	User::LeaveIfError(iWs.Connect());
	// the screen's pointer area. WSERV gives only screen 0 a digitiser area (CWsTop::InitScreenL): every other screen's pointer
	// cursor area is empty, and RestrictPos clamps any pointer position there to (-1,-1). A frame's click for this screen needs the
	// whole screen; the 2010 API for it is SetPointerCursorArea (WriteDeviceData), which acts on this session's primary screen
	// device -- hence a device on this screen, made first (CWsClient: iScreen = the primary device's screen)
	iDevice = new(ELeave) CWsScreenDevice(iWs);
	User::LeaveIfError(iDevice->Construct(aScreen));
	iWs.SetPointerCursorArea(iDevice->CurrentScreenMode(), TRect(TPoint(0, 0), iDevice->SizeInPixels()));
	User::LeaveIfError(RFbsSession::Connect());
	iBitmap = new(ELeave) CFbsBitmap;
	User::LeaveIfError(iBitmap->Create(iSize, EColor16MU));
	User::LeaveIfError(iReg.Connect());
	iInfo.iType = EPheWinNormal;
	iInfo.iGeometry = TRect(TPoint(0, 0), iSize);
	iInfo.iTitle.Format(_L("Screen %d"), aScreen);
	User::LeaveIfError(iReg.Register(iInfo));
	Log(_L("screen %d: %dx%d at %08x, stride %d -> window %d"), aScreen, w, h, addr, pitch, iInfo.iId);
	}

// the changed rectangle since the last call, copied into the bitmap; EFalse = nothing changed
TBool CScreenPub::CopyDamage(TRect& aRect)
	{
	TInt d[5];
	if (UserSvr::HalFunction(EHalGroupDisplay, KPheDisplayHalDamage, d, NULL, iScreen) != KErrNone)
		return EFalse;
	if (iInfo.iBitmap == 0)											// the first frame: all of it
		{
		d[0] = 0;  d[1] = 0;  d[2] = iSize.iWidth;  d[3] = iSize.iHeight;
		}
	else if (d[0] >= d[2] || d[1] >= d[3])
		return EFalse;
	aRect.SetRect(d[0], d[1], d[2], d[3]);
	iGen = (TUint)d[4];
	iBitmap->LockHeap();
	TUint32* dst = iBitmap->DataAddress();
	const TInt dstride = CFbsBitmap::ScanLineLength(iSize.iWidth, EColor16MU) / 4;
	for (TInt y = aRect.iTl.iY; y < aRect.iBr.iY; ++y)
		Mem::Copy(dst + y*dstride + aRect.iTl.iX, iFb + y*iStride + aRect.iTl.iX, aRect.Width() * 4);
	iBitmap->UnlockHeap();
	return ETrue;
	}

void CScreenPub::UpdateTitle()
	{
	const TInt wg = iWs.GetFocusWindowGroup(iScreen);
	if (wg <= 0)
		return;
	CApaWindowGroupName* name = NULL;
	TRAPD(r, name = CApaWindowGroupName::NewL(iWs, wg));
	if (r != KErrNone)
		return;
	TPtrC caption = name->Caption();
	if (caption.Length() && caption.Left(iInfo.iTitle.MaxLength()) != iInfo.iTitle)
		{
		iInfo.iTitle = caption.Left(iInfo.iTitle.MaxLength());
		iReg.Update(iInfo);
		Log(_L("screen %d: title %S"), iScreen, &iInfo.iTitle);
		}
	delete name;
	}

// a frame's pointer event in this screen's pixels: the key focus (and with it WSERV's pointer, pointer_board.cpp) moves to this
// screen first, then the raw event goes in as the digitiser's would (SimulateRawEvent: SwEvent)
void CScreenPub::InjectPointer(const TPheWinPointer& aEv)
	{
	if (iWs.GetFocusScreen() != iScreen)
		{
		const TInt r = iWs.SetFocusScreen(iScreen);
		Log(_L("screen %d: pointer -> SetFocusScreen %d"), iScreen, r);
		}
	TRawEvent raw;
	raw.Set((TRawEvent::TType)aEv.iType, aEv.iPos.iX, aEv.iPos.iY);
	iWs.SimulateRawEvent(raw);
	iWs.Flush();
	static TInt moves;									// (drags: one line per gesture, with its move count)
	if (aEv.iType == TRawEvent::EPointerMove)
		++moves;
	else
		{
		Log(_L("screen %d: pointer %d at %d,%d (%d moves before)"), iScreen, aEv.iType, aEv.iPos.iX, aEv.iPos.iY, moves);
		moves = 0;
		}
	}

// a frame's ping (its "not responding" decoration, hildon-port.md 7.51.12): a classic app has no registry session to answer it, so
// the publisher answers for the app in front on this screen -- while that window group's owning thread is alive. (2010 has no
// responsiveness probe for a classic app: a hung but living app is not detected here; a dead or missing one is never answered.)
void CScreenPub::AnswerPing()
	{
	const TInt wg = iWs.GetFocusWindowGroup(iScreen);
	TThreadId id;
	TBool alive = EFalse;
	RThread t;
	if (wg > 0 && iWs.GetWindowGroupClientThreadId(wg, id) == KErrNone && t.Open(id) == KErrNone)
		{
		alive = t.ExitType() == EExitPending;
		t.Close();
		}
	if (alive)
		iReg.Pong(iInfo.iId);
	static TInt logged;
	if (logged++ < 3 || !alive)
		Log(_L("screen %d: ping -> %S (group %d)"), iScreen, alive ? &_L("answered") : &_L("NOT answered: no live app"), wg);
	}

void CScreenPub::RunL()
	{
	RTimer timer;
	User::LeaveIfError(timer.CreateLocal());
	TRequestStatus tick, action, pointer;
	TPheWinPointerPckg ev;
	iReg.ActionNotify(action);
	iReg.PointerNotify(ev, pointer);
	timer.After(tick, 0);
	TInt titleCountdown = 0;
	for (;;)
		{
		User::WaitForAnyRequest();
		if (pointer != KRequestPending)
			{
			if (pointer.Int() == KErrNone)
				InjectPointer(ev());
			else
				Log(_L("screen %d: PointerNotify %d"), iScreen, pointer.Int());
			if (pointer.Int() == KErrNone)
				iReg.PointerNotify(ev, pointer);
			else
				pointer = KRequestPending;		// (stop listening: one error line, not a loop)
			continue;
			}
		if (action != KRequestPending)
			{
			const TInt ev = action.Int();
			if (ev > 0 && PheWinActionCode(ev) == EPheWinActFocus)
				{
				const TInt r = iWs.SetFocusScreen(iScreen);
				Log(_L("screen %d: focus action -> SetFocusScreen %d"), iScreen, r);
				}
			else if (ev > 0 && PheWinActionCode(ev) == EPheWinActPing)
				AnswerPing();
			else if (ev > 0 && PheWinActionCode(ev) == EPheWinActClose)
				Log(_L("screen %d: close action (ignored: the app owns its life)"), iScreen);
			if (ev < 0)
				{ Log(_L("screen %d: ActionNotify %d"), iScreen, ev); action = KRequestPending; }
			else
				iReg.ActionNotify(action);
			continue;
			}
		TRect r;
		if (CopyDamage(r))
			{
			const TBool first = iInfo.iBitmap == 0;
			iInfo.iBitmap = iBitmap->Handle();
			const TInt subs = iReg.Content(iInfo.iId, iInfo.iBitmap, r);
			if (first)
				Log(_L("screen %d: first content, bitmap %d, %d subscriber(s)"), iScreen, iInfo.iBitmap, subs);
			}
		if (--titleCountdown <= 0)
			{
			UpdateTitle();
			titleCountdown = 50;										// once a second
			}
		timer.After(tick, KPollUs);
		}
	}

static void MainL()
	{
	TBuf<32> arg;
	User::CommandLine(arg);
	TLex lex(arg);
	TInt screen = 1;
	if (lex.Val(screen) != KErrNone || screen < 0)
		User::Leave(KErrArgument);
	CScreenPub* pub = new(ELeave) CScreenPub;
	CleanupStack::PushL(pub);
	pub->ConstructL(screen);
	RProcess::Rendezvous(KErrNone);
	pub->RunL();
	CleanupStack::PopAndDestroy(pub);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	TRAPD(r, MainL());
	if (r != KErrNone)
		Log(_L("failed %d"), r);
	delete tc;
	return r;
	}
