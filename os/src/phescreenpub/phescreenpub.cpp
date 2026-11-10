// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phescreenpub -- publish a (virtual) WSERV screen as one window of the window registry (phewinreg), so a compositor shows a
// classic app that runs on its own screen as a decorated window (multi-window stage 2).
//
//   phescreenpub <screen> [thread id]
//
// With a thread id (phescreenlaunch window: the app it started on that screen) the publisher ends when that thread ends, and
// its window leaves the registry with its session.
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
#include <apgtask.h>
#include "phewinreg.h"

static const TInt KPheDisplayHalDamage = 0x7103;		// the x86 display driver, virtual screens (xk_display.cpp)
static const TInt KPheDisplayHalTile = 0x7105;			// ... the screen's framebuffer tile off (a compositor shows it) / on
_LIT(KOff, "off");
_LIT(KOn, "on");
static const TInt KPheDisplayHalDamageNotify = 0x7106;	// ... complete a request when the screen's generation moves
static const TInt KPollUs = 20000;
static const TInt KCloseGraceUs = 10000000;			// the app's own exit after a close, before it is killed						// 50 Hz, the display flusher's own rate

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
	void RunL(TUint64 aWatch);
private:
	TBool CopyDamage(TRect& aRect);
	void UpdateTitle();
	void InjectPointer(const TPheWinPointer& aEv);
	void AnswerPing();
	TInt AppWindowGroup(TThreadId aApp);
	void UpdateTile();
	TInt iPingWg;
	TBool iPingStuck;
	TBool iTileOff;
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
	TUid iAppUid;
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
	iGen = (TUint)d[4];												// (seen, even when the change was already read: no re-wake)
	if (iInfo.iBitmap == 0)											// the first frame: all of it
		{
		d[0] = 0;  d[1] = 0;  d[2] = iSize.iWidth;  d[3] = iSize.iHeight;
		}
	else if (d[0] >= d[2] || d[1] >= d[3])
		return EFalse;
	aRect.SetRect(d[0], d[1], d[2], d[3]);
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
	// the app in front, for a frame that raises an app's window instead of starting it twice (RPheWinReg::AppUid)
	const TUid uid = name->AppUid();
	if (uid != KNullUid && uid != iAppUid)
		{
		iAppUid = uid;
		const TInt e = iReg.SetAppUid(iInfo.iId, uid);
		Log(_L("screen %d: app uid %08x (%d)"), iScreen, uid.iUid, e);
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
	// Alive is not enough: a hung app's thread lives on. Each ping also sends the app's window group a no-op event, which signals
	// the app's thread (WSERV completes its event request). A responsive app runs its event loop and takes that signal at once, so
	// at the next ping (the frame pings every 2 s) its request count is back to <= 0. A count still above 0 means the thread has
	// not returned to its scheduler since: busy, or blocked on something else. That ping goes unanswered, and the frame shows
	// "not responding" once the oldest unanswered ping is old enough (hildon-port.md 7.51.19)
	const TInt wg = iWs.GetFocusWindowGroup(iScreen);
	TThreadId id;
	TBool alive = EFalse, stuck = EFalse;
	TInt pending = 0;
	RThread t;
	if (wg > 0 && iWs.GetWindowGroupClientThreadId(wg, id) == KErrNone && t.Open(id) == KErrNone)
		{
		alive = t.ExitType() == EExitPending;
		pending = t.RequestCount();
		stuck = alive && iPingWg == wg && pending > 0;
		t.Close();
		}
	iPingWg = wg;
	if (alive)
		{
		TWsEvent e;
		e.SetType(EEventUser + 0x5045);		// (an event type no app handles: AppUi HandleWsEventL ignores unknown types)
		e.SetTimeNow();
		iWs.SendEventToWindowGroup(wg, e);
		}
	if (alive && !stuck)
		iReg.Pong(iInfo.iId);
	static TInt logged;
	if (logged++ < 3 || !alive || stuck != iPingStuck)
		Log(_L("screen %d: ping -> %S (group %d, %d signal(s) unread)"), iScreen,
			!alive ? &_L("NOT answered: no live app") : stuck ? &_L("NOT answered: the app has not run its event loop since the last ping") : &_L("answered"),
			wg, pending);
	iPingStuck = stuck;
	}

// a frame that composites this window shows the screen itself: the display driver then stops copying the screen into its
// framebuffer tile (hildon-port.md 7.51.19); the tile comes back when no frame composites it
void CScreenPub::UpdateTile()
	{
	TPheWinInfo list[16];
	TPtr8 p((TUint8*)list, 0, sizeof(list));
	const TInt n = iReg.List(p);
	for (TInt i = 0; i < n && i < 16; ++i)
		if (list[i].iId == iInfo.iId)
			{
			const TBool off = list[i].iComposited > 0;
			if (off != iTileOff)
				{
				iTileOff = off;
				const TInt e = UserSvr::HalFunction(EHalGroupDisplay, KPheDisplayHalTile, (TAny*)(off ? 0 : 1), NULL, iScreen);
				Log(_L("screen %d: composited by %d frame(s): framebuffer tile %S (%d)"), iScreen, list[i].iComposited,
					off ? &KOff : &KOn, e);
				}
			break;
			}
	}

// the window group on this screen that the app's thread owns (0 = none)
TInt CScreenPub::AppWindowGroup(TThreadId aApp)
	{
	CArrayFixFlat<TInt>* ids = new CArrayFixFlat<TInt>(4);
	if (!ids)
		return 0;
	TInt found = 0;
	if (iWs.WindowGroupList(ids, iScreen) == KErrNone)
		for (TInt i = 0; i < ids->Count() && !found; ++i)
			{
			TThreadId t;
			if (iWs.GetWindowGroupClientThreadId(ids->At(i), t) == KErrNone && t == aApp)
				found = ids->At(i);
			}
	delete ids;
	return found;
	}

void CScreenPub::RunL(TUint64 aWatch)
	{
	RTimer timer;
	User::LeaveIfError(timer.CreateLocal());
	TRequestStatus tick, action, pointer, died, killTick;
	RThread app;
	RTimer killTimer;					// after a close: the app's own exit gets KCloseGraceUs, then it is killed
	User::LeaveIfError(killTimer.CreateLocal());
	TBool closing = EFalse;
	if (aWatch)
		{
		User::LeaveIfError(app.Open(TThreadId(aWatch)));
		app.Logon(died);
		}
	TPheWinPointerPckg ev;
	iReg.ActionNotify(action);
	iReg.PointerNotify(ev, pointer);
	// content: the display driver completes a request when this screen changed (7.51.19); an older driver = the 50 Hz poll
	TRequestStatus damage, watch;
	damage = KRequestPending;				// (a HAL call is not an async request: the caller marks it pending, the driver completes it)
	const TBool notify = UserSvr::HalFunction(EHalGroupDisplay, KPheDisplayHalDamageNotify, &damage, (TAny*)iGen, iScreen) == KErrNone;
	if (!notify)
		damage = KRequestPending;
	_LIT(KNotify, "on damage (driver notify)");
	_LIT(KPoll, "polled at 50 Hz");
	Log(_L("screen %d: content %S"), iScreen, notify ? &KNotify : &KPoll);
	iReg.Watchers(-1, watch);
	timer.After(tick, 0);
	TInt titleCountdown = 0;
	for (;;)
		{
		User::WaitForAnyRequest();
		if (aWatch && died != KRequestPending)
			{
			Log(_L("screen %d: its app ended (%d): the window goes"), iScreen, died.Int());
			if (notify)
				UserSvr::HalFunction(EHalGroupDisplay, KPheDisplayHalDamageNotify, NULL, NULL, iScreen);
			iReg.WatchersCancel();
			if (iTileOff)
				UserSvr::HalFunction(EHalGroupDisplay, KPheDisplayHalTile, (TAny*)1, NULL, iScreen);
			app.Close();
			if (closing)
				killTimer.Cancel();
			killTimer.Close();
			iReg.ActionCancel();
			iReg.PointerCancel();
			timer.Cancel();
			timer.Close();
			return;
			}
		if (closing && killTick != KRequestPending)
			{
			// the app did not end within the grace time: end it the hard way (as the 2010 task switcher's KillTask)
			RProcess p;
			if (app.Process(p) == KErrNone)
				{
				Log(_L("screen %d: the app did not end %d s after EndTask: killing it"), iScreen, KCloseGraceUs / 1000000);
				p.Kill(KErrNone);
				p.Close();
				}
			closing = EFalse;
			continue;
			}
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
				{
				if (!aWatch)
					Log(_L("screen %d: close action (ignored: no app given, the app owns its life)"), iScreen);
				else if (!closing)
					{
					// the 2010 way (task switcher, TApaTask::EndTask): an exit event to the app's window group, so the app
					// exits through its own path and can save
					const TInt wg = AppWindowGroup(app.Id());
					if (wg > 0)
						{
						TApaTask task(iWs);
						task.SetWgId(wg);
						task.EndTask();
						}
					Log(_L("screen %d: close action -> EndTask on window group %d"), iScreen, wg);
					killTimer.After(killTick, KCloseGraceUs);
					closing = ETrue;
					}
				}
			if (ev < 0)
				{ Log(_L("screen %d: ActionNotify %d"), iScreen, ev); action = KRequestPending; }
			else
				iReg.ActionNotify(action);
			continue;
			}
		if (watch != KRequestPending)
			{
			const TInt serial = watch.Int();
			if (serial < 0)
				{ Log(_L("screen %d: Watchers %d"), iScreen, serial); watch = KRequestPending; continue; }
			UpdateTile();
			iReg.Watchers(serial, watch);
			continue;
			}
		TBool damaged = EFalse;
		if (notify && damage != KRequestPending)
			damaged = ETrue;
		else if (tick != KRequestPending)
			{
			if (!notify)
				damaged = ETrue;
			if (--titleCountdown <= 0)
				{
				UpdateTitle();
				titleCountdown = notify ? 1 : 50;						// once a second
				}
			timer.After(tick, notify ? 1000000 : KPollUs);
			}
		if (!damaged)
			continue;
		TRect r;
		if (CopyDamage(r))
			{
			const TBool first = iInfo.iBitmap == 0;
			iInfo.iBitmap = iBitmap->Handle();
			const TInt subs = iReg.Content(iInfo.iId, iInfo.iBitmap, r);
			if (first)
				Log(_L("screen %d: first content, bitmap %d, %d subscriber(s)"), iScreen, iInfo.iBitmap, subs);
			}
		if (notify)
			{
			damage = KRequestPending;
			UserSvr::HalFunction(EHalGroupDisplay, KPheDisplayHalDamageNotify, &damage, (TAny*)iGen, iScreen);
			}
		}
	}

static void MainL()
	{
	TBuf<64> arg;
	User::CommandLine(arg);
	TLex lex(arg);
	TInt screen = 1;
	if (lex.Val(screen) != KErrNone || screen < 0)
		User::Leave(KErrArgument);
	TInt64 watch = 0;
	lex.SkipSpace();
	if (!lex.Eos() && (lex.Val(watch) != KErrNone || watch <= 0))
		User::Leave(KErrArgument);
	CScreenPub* pub = new(ELeave) CScreenPub;
	CleanupStack::PushL(pub);
	pub->ConstructL(screen);
	RProcess::Rendezvous(KErrNone);
	pub->RunL(watch);
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
