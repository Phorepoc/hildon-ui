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
	TInt iScreen;
	TUint32* iFb;
	TInt iStride;					// words per line
	TSize iSize;
	RPheWinReg iReg;
	RWsSession iWs;
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

void CScreenPub::RunL()
	{
	RTimer timer;
	User::LeaveIfError(timer.CreateLocal());
	TRequestStatus tick, action;
	iReg.ActionNotify(action);
	timer.After(tick, 0);
	TInt titleCountdown = 0;
	for (;;)
		{
		User::WaitForRequest(tick, action);
		if (action != KRequestPending)
			{
			const TInt ev = action.Int();
			if (ev > 0 && PheWinActionCode(ev) == EPheWinActFocus)
				{
				const TInt r = iWs.SetFocusScreen(iScreen);
				Log(_L("screen %d: focus action -> SetFocusScreen %d"), iScreen, r);
				}
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
			iInfo.iBitmap = iBitmap->Handle();
			iReg.Content(iInfo.iId, iInfo.iBitmap, r);
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
