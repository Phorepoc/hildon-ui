// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phewinreg.h -- the window registry (hildon-port.md C4): every application's GDK/Clutter backend registers its top-level
// windows here with the metadata a compositor needs (what libmatchbox2 reads from X window properties: type, title,
// transient-for, geometry, state) and publishes its front bitmap (an FBSERV CFbsBitmap handle, shareable between
// processes); the frame process lists them and waits for changes, then imports each bitmap as a texture. The registry
// owns no pixels: a window's content stays in its owner's bitmap. A session's windows go when the session closes.
//
// Client side: header only (RSessionBase), link euser.lib; the server is phewinreg.exe, started on the first Connect.
#ifndef PHEWINREG_H
#define PHEWINREG_H
#include <e32std.h>

_LIT(KPheWinRegServer, "phewinreg");
_LIT(KPheWinRegExe, "phewinreg.exe");

// the window types of the EWMH _NET_WM_WINDOW_TYPE set matchbox handles
enum TPheWinType { EPheWinNormal, EPheWinDialog, EPheWinMenu, EPheWinNotification, EPheWinDesktop, EPheWinInput, EPheWinSplash };
// state flags (_NET_WM_STATE): fullscreen, modal, urgent (demands attention), hidden (iconified)
enum { EPheWinFullscreen = 1, EPheWinModal = 2, EPheWinUrgent = 4, EPheWinHidden = 8 };

class TPheWinInfo
	{
public:
	TPheWinInfo() : iId(0), iOwner(0), iType(EPheWinNormal), iTransientFor(0), iState(0), iBitmap(0), iContentSerial(0), iGpuToken(0), iSubscribers(0), iComposited(0) {}
	TInt iId;					// the registry's id (0 before registering)
	TUint iOwner;				// the owner's process id (the registry fills it)
	TInt iType;					// TPheWinType
	TInt iTransientFor;			// another window's id, or 0
	TRect iGeometry;			// the requested position and size, in screen pixels
	TUint iState;				// EPheWin... flags
	TInt iBitmap;				// the front bitmap's FBSERV handle (CFbsBitmap::Handle()), 0 = no content yet
	TUint iContentSerial;		// counts content updates (the registry increments it)
	TRect iDamage;				// the area of the last content update, in window coordinates
	TInt iGpuToken;				// the front texture's SVGA share token (EGL_SYMBIAN_SVGA_SHARE_PH, hildon-port.md 7.47), 0 = bitmap only
	TInt iSubscribers;			// frame sessions that subscribed to this window's content (the registry fills it)
	TInt iComposited;			// ... of them, those that composite it: the owner keeps the window off the window server's screen
	TBuf<64> iTitle;
	};

// Subscribe() modes (hildon-port.md 7.48)
enum TPheWinSubscription { EPheWinUnsubscribe = 0, EPheWinWatch = 1, EPheWinComposite = 2 };
typedef TPckgBuf<TPheWinInfo> TPheWinInfoPckg;

enum TPheWinRegOp { EPheWinRegister, EPheWinUpdate, EPheWinContent, EPheWinRemove, EPheWinList, EPheWinNotify, EPheWinNotifyCancel, EPheWinWatchers, EPheWinWatchersCancel, EPheWinSubscribe, EPheWinContentNotify };

class RPheWinReg : public RSessionBase
	{
public:
	inline TInt Connect();
	// register a window: aInfo's metadata in, its iId out
	inline TInt Register(TPheWinInfo& aInfo);
	// new metadata for window aInfo.iId (the bitmap and serial are kept)
	inline TInt Update(const TPheWinInfo& aInfo) { TPheWinInfoPckg p(aInfo); return SendReceive(EPheWinUpdate, TIpcArgs(&p)); }
	// the window's front bitmap, the area that changed and (0 = none) its GPU share token; returns the window's subscriber count, or an error
	inline TInt Content(TInt aId, TInt aBitmapHandle, const TRect& aDamage, TInt aGpuToken = 0)
		{ TPckgC<TRect> d(aDamage); return SendReceive(EPheWinContent, TIpcArgs(aId, aBitmapHandle, &d, aGpuToken)); }
	inline TInt Remove(TInt aId) { return SendReceive(EPheWinRemove, TIpcArgs(aId)); }
	// every window, in registration order: fills aBuf with TPheWinInfo records; returns the count (or the count needed, when > the buffer)
	inline TInt List(TDes8& aBuf) { return SendReceive(EPheWinList, TIpcArgs(&aBuf)); }
	// completes with the registry's change serial when anything changes after aSeen (0 = at the next change)
	inline void Notify(TUint aSeen, TRequestStatus& aStatus) { SendReceive(EPheWinNotify, TIpcArgs((TInt)aSeen), aStatus); }
	inline void NotifyCancel() { SendReceive(EPheWinNotifyCancel, TIpcArgs()); }
	// for a compositor: completes with the content tick when a GPU window published its next frame (same token: nothing to re-list)
	// after aSeen (0 = at the next one). Notify still reports everything else (metadata, new tokens, bitmaps)
	inline void ContentNotify(TUint aSeen, TRequestStatus& aStatus) { SendReceive(EPheWinContentNotify, TIpcArgs((TInt)aSeen), aStatus); }
	// for a window owner: completes with its subscription serial when it differs from aSeen (-1 = at once): the serial moves whenever a
	// subscription to one of the owner's windows changes; the owner then reads iSubscribers / iComposited of its windows (List).
	// A window without subscribers publishes no content; when one appears it publishes a full frame
	inline void Watchers(TInt aSeen, TRequestStatus& aStatus) { SendReceive(EPheWinWatchers, TIpcArgs(aSeen), aStatus); }
	// for a frame process: subscribe to one window's content (TPheWinSubscription; EPheWinUnsubscribe ends it). Only other sessions'
	// windows; a session's subscriptions end with it
	inline TInt Subscribe(TInt aId, TInt aMode) { return SendReceive(EPheWinSubscribe, TIpcArgs(aId, aMode)); }
	inline void WatchersCancel() { SendReceive(EPheWinWatchersCancel, TIpcArgs()); }
	};

inline TInt RPheWinReg::Connect()
	{
	for (TInt tries = 0; tries < 2; ++tries)
		{
		TInt r = CreateSession(KPheWinRegServer, TVersion(1, 0, 0), 4);
		if (r != KErrNotFound && r != KErrServerTerminated)
			return r;
		RProcess p;
		if ((r = p.Create(KPheWinRegExe, KNullDesC)) != KErrNone)
			return r;
		TRequestStatus st;
		p.Rendezvous(st);
		p.Resume();
		User::WaitForRequest(st);
		p.Close();
		if (st.Int() != KErrNone && st.Int() != KErrAlreadyExists)
			return st.Int();
		}
	return CreateSession(KPheWinRegServer, TVersion(1, 0, 0), 4);
	}

inline TInt RPheWinReg::Register(TPheWinInfo& aInfo)
	{
	TPheWinInfoPckg p(aInfo);
	const TInt r = SendReceive(EPheWinRegister, TIpcArgs(&p));
	if (r > 0)
		aInfo.iId = r;
	return r < 0 ? r : KErrNone;
	}
#endif
