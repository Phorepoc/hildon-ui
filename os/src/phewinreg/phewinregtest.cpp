// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phewinregtest.cpp -- the window registry's self test (hildon-port.md C4): an "application" session registers a window
// and a transient dialog, publishes a real CFbsBitmap as the window's content; a "frame" session waits for the changes,
// lists the windows, duplicates the bitmap by its handle (what the compositor's import does) and reads a pixel back; then
// the application session closes and its windows must go. Prints "[phewinregtest] PASS" or the first failure.
#include <e32base.h>
#include <e32debug.h>
#include <fbs.h>
#include "phewinreg.h"

#define CHECK(c) do { if (!(c)) { RDebug::Printf("[phewinregtest] FAIL line %d: %s", __LINE__, #c); return KErrGeneral; } } while (0)

static TInt ListAll(RPheWinReg& aReg, TPheWinInfo* aOut, TInt aMax)
	{
	TPtr8 p((TUint8*)aOut, 0, aMax * sizeof(TPheWinInfo));
	return aReg.List(p);
	}

static TInt Run()
	{
	CHECK(RFbsSession::Connect() == KErrNone);
	RPheWinReg app, frame;
	CHECK(app.Connect() == KErrNone);
	CHECK(frame.Connect() == KErrNone);
	TRequestStatus st, wst;
	app.Watchers(-1, wst);
	User::WaitForRequest(wst);
	CHECK(wst.Int() == 0);										// no subscriptions yet
	frame.Notify(0, st);

	TPheWinInfo w;
	w.iType = EPheWinNormal;
	w.iGeometry = TRect(TPoint(0, 0), TSize(360, 600));
	w.iTitle = _L("Notes");
	CHECK(app.Register(w) == KErrNone && w.iId > 0);
	User::WaitForRequest(st);
	app.Watchers(0, wst);										// the owner waits for a subscription
	CHECK(app.Subscribe(w.iId, EPheWinWatch) == KErrArgument);	// not to its own window
	CHECK(frame.Subscribe(w.iId + 100, EPheWinWatch) == KErrNotFound);
	CHECK(frame.Subscribe(w.iId, EPheWinComposite) == KErrNone);
	User::WaitForRequest(wst);
	CHECK(wst.Int() == 1);										// it came: publish a full frame now
	CHECK(st.Int() > 0);
	TUint seen = (TUint)st.Int();

	TPheWinInfo d = w;
	d.iId = 0;
	d.iType = EPheWinDialog;
	d.iTransientFor = w.iId;
	d.iState = EPheWinModal;
	d.iGeometry = TRect(TPoint(20, 200), TSize(320, 160));
	d.iTitle = _L("Delete note?");
	CHECK(app.Register(d) == KErrNone && d.iId != w.iId);

	CFbsBitmap* bmp = new CFbsBitmap;
	CHECK(bmp && bmp->Create(TSize(360, 600), EColor16MU) == KErrNone);
	bmp->LockHeap();
	TUint32* px = (TUint32*)bmp->DataAddress();
	px[0] = 0xFF336699u;
	bmp->UnlockHeap();
	CHECK(app.Content(w.iId, bmp->Handle(), TRect(0, 0, 360, 600)) == 1);	// one watcher: the frame session
	CHECK(frame.Content(w.iId, bmp->Handle(), TRect()) == KErrNotFound);	// only the owner publishes a window's content

	// the frame side: a change since `seen`, then the list
	frame.Notify(seen, st);
	User::WaitForRequest(st);
	CHECK((TUint)st.Int() != seen);
	TPheWinInfo list[4];
	CHECK(ListAll(frame, list, 1) == 2);								// too small: the count needed
	CHECK(ListAll(frame, list, 4) == 2);
	CHECK(list[0].iId == w.iId && list[0].iBitmap == bmp->Handle() && list[0].iContentSerial == 1 && list[0].iOwner == (TUint)RProcess().Id().Id());
	CHECK(list[0].iSubscribers == 1 && list[0].iComposited == 1 && list[1].iSubscribers == 0);	// per window: the dialog has none
	app.Watchers(1, wst);
	CHECK(frame.Subscribe(w.iId, EPheWinWatch) == KErrNone);				// composite -> watch only
	User::WaitForRequest(wst);
	CHECK(wst.Int() > 1);
	CHECK(ListAll(frame, list, 4) == 2 && list[0].iSubscribers == 1 && list[0].iComposited == 0);
	CHECK(list[1].iType == EPheWinDialog && list[1].iTransientFor == w.iId && (list[1].iState & EPheWinModal) && list[1].iTitle == _L("Delete note?"));
	CFbsBitmap* imported = new CFbsBitmap;
	CHECK(imported && imported->Duplicate(list[0].iBitmap) == KErrNone);
	imported->LockHeap();
	const TUint32 back = ((TUint32*)imported->DataAddress())[0];
	imported->UnlockHeap();
	CHECK(back == 0xFF336699u && imported->SizeInPixels() == TSize(360, 600));
	delete imported;

	// decoration actions back to the owner: only a subscribed frame may send; queued in order; one waiter at a time; cancel
	CHECK(frame.Action(w.iId, 0) == KErrArgument);
	CHECK(frame.Action(w.iId + 100, EPheWinActClose) == KErrNotFound);
	CHECK(frame.Action(d.iId, EPheWinActClose) == KErrPermissionDenied);	// the frame did not subscribe to the dialog
	CHECK(app.Action(w.iId, EPheWinActClose) == KErrPermissionDenied);		// nor does an owner act on itself
	CHECK(frame.Action(w.iId, EPheWinActMinimise) == KErrNone);				// queued before the owner waits
	CHECK(frame.Action(w.iId, EPheWinActClose) == KErrNone);
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionId(st.Int()) == w.iId && PheWinActionCode(st.Int()) == EPheWinActMinimise);
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionId(st.Int()) == w.iId && PheWinActionCode(st.Int()) == EPheWinActClose);
	app.ActionNotify(st);													// nothing queued: waits
	CHECK(st.Int() == KRequestPending);
	CHECK(frame.Action(w.iId, EPheWinActFocus) == KErrNone);
	User::WaitForRequest(st);
	CHECK(PheWinActionCode(st.Int()) == EPheWinActFocus);
	CHECK(ListAll(frame, list, 4) == 2 && (list[0].iState & EPheWinFocused) && !(list[1].iState & EPheWinFocused));
	{	// the owner cannot clear (or set) focus itself: the registry keeps it
	TPheWinInfo w2 = w; w2.iState = 0;
	CHECK(app.Update(w2) == KErrNone);
	TPheWinInfo d2 = d; d2.iState |= EPheWinFocused;
	CHECK(app.Update(d2) == KErrNone);
	CHECK(ListAll(frame, list, 4) == 2 && (list[0].iState & EPheWinFocused) && !(list[1].iState & EPheWinFocused));
	}
	CHECK(frame.Subscribe(d.iId, EPheWinWatch) == KErrNone);				// focus moves to the dialog: exactly one focused window
	CHECK(frame.Action(d.iId, EPheWinActFocus) == KErrNone);
	CHECK(ListAll(frame, list, 4) == 2 && !(list[0].iState & EPheWinFocused) && (list[1].iState & EPheWinFocused));
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionId(st.Int()) == d.iId && PheWinActionCode(st.Int()) == EPheWinActFocus);
	CHECK(frame.Subscribe(d.iId, EPheWinUnsubscribe) == KErrNone);
	app.ActionNotify(st);
	app.ActionCancel();
	User::WaitForRequest(st);
	CHECK(st.Int() == KErrCancel);

	// stacking: bottom first; a dialog right above its parent; a raise puts a window (and its dialogs) on top
	{
	TPheWinInfo w3 = w; w3.iId = 0; w3.iTransientFor = 0; w3.iTitle = _L("second");
	CHECK(app.Register(w3) == KErrNone);
	CHECK(ListAll(frame, list, 4) == 3 && list[0].iId == w.iId && list[1].iId == d.iId && list[2].iId == w3.iId);
	CHECK(frame.Action(w.iId, EPheWinActRaise) == KErrNone);
	CHECK(ListAll(frame, list, 4) == 3 && list[0].iId == w3.iId && list[1].iId == w.iId && list[2].iId == d.iId);
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionId(st.Int()) == w.iId && PheWinActionCode(st.Int()) == EPheWinActRaise);
	CHECK(frame.Action(w.iId, 6) == KErrArgument);
	CHECK(app.Remove(w3.iId) == KErrNone);
	CHECK(ListAll(frame, list, 4) == 2 && list[0].iId == w.iId && list[1].iId == d.iId);
	}

	// geometry both ways: owner size hints + icon -> frame; frame "resize to" -> owner, clamped to the hints
	{
	TPheWinHints h;
	h.iId = w.iId; h.iMinSize = TSize(200, 300); h.iMaxSize = TSize(800, 900); h.iPreferredSize = TSize(360, 600); h.iIcon = bmp->Handle();
	CHECK(frame.SetHints(h) == KErrNotFound);								// only the owner
	TPheWinHints bad = h; bad.iMaxSize = TSize(100, 900);
	CHECK(app.SetHints(bad) == KErrArgument);								// max below min
	frame.Notify(0, st);
	CHECK(app.SetHints(h) == KErrNone);
	User::WaitForRequest(st);												// frames hear new hints
	CHECK(st.Int() > 0);
	TPheWinHints got;
	CHECK(frame.Hints(w.iId, got) == KErrNone && got.iMinSize == TSize(200, 300) && got.iPreferredSize == TSize(360, 600) && got.iIcon == bmp->Handle() && got.iRequested.IsEmpty());
	CHECK(frame.Hints(w.iId + 100, got) == KErrNotFound);
	CHECK(frame.Resize(d.iId, TRect(0, 0, 400, 400)) == KErrPermissionDenied);	// not subscribed to the dialog
	CHECK(app.Resize(w.iId, TRect(0, 0, 400, 400)) == KErrPermissionDenied);	// nor the owner itself
	CHECK(frame.Resize(w.iId, TRect(10, 20, 110, 2020)) == KErrNone);			// 100 x 2000: clamped to 200 x 900
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionId(st.Int()) == w.iId && PheWinActionCode(st.Int()) == EPheWinActResize);
	CHECK(app.Hints(w.iId, got) == KErrNone && got.iRequested == TRect(TPoint(10, 20), TSize(200, 900)));
	CHECK(frame.Action(w.iId, EPheWinActResize) == KErrArgument);			// only the registry queues it

	// ping / not responding: unanswered pings show with their age; one Pong clears them and is a registry change
	CHECK(frame.Ping(d.iId) == KErrPermissionDenied);
	CHECK(frame.Ping(w.iId) == KErrNone);
	CHECK(frame.Ping(w.iId) == KErrNone);
	User::After(50000);
	CHECK(frame.Hints(w.iId, got) == KErrNone && got.iPingPending == 2 && got.iPingAgeMs >= 40);
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionCode(st.Int()) == EPheWinActPing);
	app.ActionNotify(st);
	User::WaitForRequest(st);
	CHECK(PheWinActionCode(st.Int()) == EPheWinActPing);
	CHECK(frame.Pong(w.iId) == KErrNotFound);								// only the owner answers
	frame.Notify(0, st);
	CHECK(app.Pong(w.iId) == KErrNone);
	User::WaitForRequest(st);
	CHECK(st.Int() > 0);
	CHECK(frame.Hints(w.iId, got) == KErrNone && got.iPingPending == 0 && got.iPingAgeMs == 0);
	}

	// metadata update, then the application goes away: its windows with it
	d.iTitle = _L("Delete 1 note?");
	CHECK(app.Update(d) == KErrNone);
	frame.Notify(0, st);
	app.Close();
	User::WaitForRequest(st);
	CHECK(st.Int() > 0);
	CHECK(ListAll(frame, list, 4) == 0);
	frame.Close();
	delete bmp;
	RFbsSession::Disconnect();
	RDebug::Printf("[phewinregtest] PASS: register, per-window subscribe (watch / composite), transient dialog, content handle + duplicate, notify, list, actions back to the owner, one focused window, stacking + raise, size hints + icon, resize requests, ping / pong, cleanup on close");
	return KErrNone;
	}

// phewinregtest close <title>: act as a frame's close button on the window with that title (subscribe to it as a watcher, send
// EPheWinActClose), so a boot script can test what the owner does with it
static TInt Close(const TDesC& aTitle)
	{
	RPheWinReg reg;
	TInt r = reg.Connect();
	if (r != KErrNone)
		return r;
	TPheWinInfo list[16];
	const TInt n = ListAll(reg, list, 16);
	r = KErrNotFound;
	for (TInt i = 0; i < n && i < 16; ++i)
		if (list[i].iTitle.CompareF(aTitle) == 0)
			{
			r = reg.Subscribe(list[i].iId, EPheWinWatch);
			if (r == KErrNone)
				r = reg.Action(list[i].iId, EPheWinActClose);
			TBuf8<64> t8;
			t8.Copy(aTitle);
			TUid uid;
			const TInt u = reg.AppUid(list[i].iId, uid);
			RDebug::Printf("[phewinregtest] close \"%S\": window %d (app uid %08x, %d), action %d", &t8, list[i].iId, uid.iUid, u, r);
			break;
			}
	if (r == KErrNotFound)
		RDebug::Printf("[phewinregtest] close: no window with that title (%d windows)", n);
	reg.Close();
	return r;
	}

// phewinregtest ping <title> [thread-name-pattern]: act as a frame's pinger on that window (watch it, ping every 2 s, 5 times) and
// log its unanswered pings. With a thread pattern (e.g. Notepad*) that thread is suspended first and resumed at the end: a hung app
static TInt PingTest(const TDesC& aTitle, const TDesC& aHang)
	{
	RPheWinReg reg;
	TInt r = reg.Connect();
	if (r != KErrNone)
		return r;
	TPheWinInfo list[16];
	const TInt n = ListAll(reg, list, 16);
	TInt id = 0;
	for (TInt i = 0; i < n && i < 16 && !id; ++i)
		if (list[i].iTitle.CompareF(aTitle) == 0)
			id = list[i].iId;
	RThread hang;
	TBool hung = EFalse;
	if (aHang.Length())
		{
		TFindThread f(aHang);
		TFullName name;
		if (f.Next(name) == KErrNone && hang.Open(f) == KErrNone)
			{
			hang.Suspend();
			hung = ETrue;
			}
		}
	r = id ? reg.Subscribe(id, EPheWinWatch) : KErrNotFound;
	for (TInt k = 0; r == KErrNone && k < 5; ++k)
		{
		reg.Ping(id);
		User::After(2000000);
		TPheWinHints h;
		reg.Hints(id, h);
		RDebug::Printf("[phewinregtest] ping window %d (%s): %d unanswered, oldest %d ms", id, hung ? "app suspended" : "app running",
					   h.iPingPending, h.iPingAgeMs);
		}
	if (hung)
		{
		hang.Resume();
		hang.Close();
		}
	reg.Close();
	return r;
	}

// phewinregtest apps: the launch API's app list (hildon-port.md 7.51.23), a few lines
// phewinregtest open <hex uid>: Open (raise or start), twice when "twice" follows
static TInt AppsTest()
	{
	RPheWinReg reg;
	TInt r = reg.Connect();
	if (r != KErrNone)
		return r;
	TBuf8<1> none;
	TInt n = reg.Apps(none);				// (the count only)
	if (n < 0)
		{ reg.Close(); return n; }
	HBufC8* buf = HBufC8::New((n + 4) * sizeof(TPheWinApp));
	if (!buf)
		{ reg.Close(); return KErrNoMemory; }
	TPtr8 b = buf->Des();
	n = reg.Apps(b);
	const TPheWinApp* a = (const TPheWinApp*)b.Ptr();
	const TInt count = b.Length() / (TInt)sizeof(TPheWinApp);
	TInt icons = 0;
	for (TInt i = 0; i < count; ++i)
		{
		if (a[i].iIcon)
			++icons;
		if (i < 12)
			{
			TBuf8<64> c8;
			c8.Copy(a[i].iCaption);
			RDebug::Printf("[phewinregtest] app %08x \"%S\" window %d icon %d mask %d", a[i].iUid, &c8, a[i].iWindow, a[i].iIcon, a[i].iIconMask);
			}
		}
	RDebug::Printf("[phewinregtest] apps: %d (returned %d), %d with an icon", count, n, icons);
	delete buf;
	reg.Close();
	return KErrNone;
	}

static TInt OpenTest(const TDesC& aUid)
	{
	TLex l(aUid);
	TUint32 uid = 0;
	if (l.Val(uid, EHex) != KErrNone)
		return KErrArgument;
	RPheWinReg reg;
	TInt r = reg.Connect();
	if (r != KErrNone)
		return r;
	r = reg.Open(TUid::Uid(uid));
	RDebug::Printf("[phewinregtest] open %08x: %d (> 0 = raised window, 0 = started)", uid, r);
	reg.Close();
	return r < 0 ? r : KErrNone;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TBuf<80> line;
	User::CommandLine(line);
	TLex lex(line);
	TPtrC cmd = lex.NextToken();
	if (cmd.CompareF(_L("apps")) == 0 || cmd.CompareF(_L("open")) == 0)
		{
		const TInt c = cmd.CompareF(_L("apps")) == 0 ? AppsTest() : OpenTest(lex.NextToken());
		delete cleanup;
		return c;
		}
	if (cmd.CompareF(_L("ping")) == 0)
		{
		TPtrC title = lex.NextToken();
		TPtrC hang = lex.NextToken();
		const TInt c = PingTest(title, hang);
		delete cleanup;
		return c;
		}
	if (cmd.CompareF(_L("close")) == 0)
		{
		lex.SkipSpace();
		const TInt c = Close(lex.Remainder());
		delete cleanup;
		return c;
		}
	const TInt r = Run();
	delete cleanup;
	return r;
	}
