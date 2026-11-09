// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phewinreg.cpp -- the window registry server (hildon-port.md C4; the protocol is phewinreg.h). One CServer2, the window
// records in registration order, one pending change notification per session. Content updates only record the owner's
// FBSERV bitmap handle and damage: the frame process duplicates the bitmap (CFbsBitmap::Duplicate) when it imports it.
// Exits when the last session closes and no window is left, as transient servers do.
#include <e32base.h>
#include <e32debug.h>
#include "phewinreg.h"

class CRegSession;

class CRegServer : public CServer2
	{
public:
	CRegServer() : CServer2(EPriorityStandard, ESharableSessions) {}
	~CRegServer() { iWins.Close(); }
	CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;
	void Changed();
	void ContentChanged();
	void SessionOpened() { ++iSessions; }
	void SessionClosed(CRegSession* aSession);
	void SubsChanged(const CRegSession* aOwner);
	void Unsubscribe(CRegSession* aFrame, TInt aSubIndex);
	TInt Find(TInt aId) const
		{
		for (TInt i = 0; i < iWins.Count(); ++i)
			if (iWins[i].iInfo.iId == aId)
				return i;
		return KErrNotFound;
		}
	struct TWin { TPheWinInfo iInfo; const CRegSession* iSession; TUint iStack; };
	TUint iStackTop = 0;			// the last raise / registration: a higher iStack is above its siblings
	void AppendStacked(TDes8& aOut, TInt aParent, TInt aDepth) const
		{
		// the children of aParent (0 = top level; a window whose parent is gone counts as top level), lowest iStack first
		TUint after = 0;
		for (;;)
			{
			TInt best = -1;
			for (TInt i = 0; i < iWins.Count(); ++i)
				{
				const TInt t = iWins[i].iInfo.iTransientFor;
				const TInt parent = (t != 0 && t != iWins[i].iInfo.iId && Find(t) >= 0) ? t : 0;
				if (parent == aParent && iWins[i].iStack > after && (best < 0 || iWins[i].iStack < iWins[best].iStack))
					best = i;
				}
			if (best < 0)
				return;
			after = iWins[best].iStack;
			aOut.Append((const TUint8*)&iWins[best].iInfo, sizeof(TPheWinInfo));
			if (aDepth < 16)		// (a transient-for cycle stops here; its windows are then missing from the list)
				AppendStacked(aOut, iWins[best].iInfo.iId, aDepth + 1);
			}
		}
	RArray<TWin> iWins;
	TInt iNextId = 1;
	TUint iSerial = 1;
	TInt iSessions = 0;
	RPointerArray<CRegSession> iWaiters;
	TUint iContentTicks = 1;		// moves on a content-only update (a GPU window's new frame: same token, no metadata change)
	};

class CRegSession : public CSession2
	{
public:
	~CRegSession() { Server().SessionClosed(this); iSubs.Close(); iActions.Close(); iPointers.Close(); }
	void CreateL() { Server().SessionOpened(); }
	void ServiceL(const RMessage2& aMessage);
	void Complete(TUint aSerial)
		{
		if (!iNotify.IsNull() && aSerial != iSeen)
			iNotify.Complete((TInt)aSerial);
		}
	RMessagePtr2 iNotify;
	TUint iSeen;
	RMessagePtr2 iContentNotify;		// a compositor waiting for the next content-only update
	TUint iContentSeen;
	RMessagePtr2 iWatch;			// an owner waiting for its subscription serial to change
	TInt iWatchSeen;
	RMessagePtr2 iAction;			// an owner waiting for a decoration action on its windows
	RArray<TInt> iActions;			// queued actions ((id << 4) | action), oldest first
	void DeliverAction()
		{
		if (!iAction.IsNull() && iActions.Count())
			{
			iAction.Complete(iActions[0]);
			iActions.Remove(0);
			}
		}
	RMessagePtr2 iPointerMsg;		// an owner waiting for a pointer event on its windows
	RArray<TPheWinPointer> iPointers;	// queued pointer events, oldest first (bounded: KMaxPointers)
	enum { KMaxPointers = 64 };
	void DeliverPointer()
		{
		if (!iPointerMsg.IsNull() && iPointers.Count())
			{
			TPheWinPointerPckg p(iPointers[0]);
			const TInt r = iPointerMsg.Write(0, p);
			iPointerMsg.Complete(r);
			iPointers.Remove(0);
			}
		}
	TInt iSubSerial = 0;			// moves when a subscription to one of this session's windows changes
	struct TSub { TInt iId; TInt iMode; };
	RArray<TSub> iSubs;				// this session's subscriptions (a frame process)
private:
	CRegServer& Server() const { return *static_cast<CRegServer*>(const_cast<CServer2*>(CSession2::Server())); }
	TInt Owns(TInt aId) const
		{
		const TInt i = Server().Find(aId);
		return i >= 0 && Server().iWins[i].iSession == this ? i : KErrNotFound;
		}
	};

CSession2* CRegServer::NewSessionL(const TVersion& aVersion, const RMessage2&) const
	{
	if (!User::QueryVersionSupported(TVersion(1, 0, 0), aVersion))
		User::Leave(KErrNotSupported);
	return new (ELeave) CRegSession;
	}

void CRegServer::Changed()
	{
	++iSerial;
	for (TInt i = iWaiters.Count() - 1; i >= 0; --i)
		{
		CRegSession* s = iWaiters[i];
		s->Complete(iSerial);
		if (s->iNotify.IsNull())
			iWaiters.Remove(i);
		}
	}

// a content-only update: every compositor waiting for one hears the new tick
void CRegServer::ContentChanged()
	{
	++iContentTicks;
	iSessionIter.SetToFirst();
	for (CSession2* s; (s = iSessionIter++) != NULL; )
		{
		CRegSession* r = static_cast<CRegSession*>(s);
		if (!r->iContentNotify.IsNull() && r->iContentSeen != iContentTicks)
			r->iContentNotify.Complete((TInt)iContentTicks);
		}
	}

// a subscription to one of aOwner's windows changed: the owner, if it waits, hears its new serial (and then reads its windows)
void CRegServer::SubsChanged(const CRegSession* aOwner)
	{
	CRegSession* r = const_cast<CRegSession*>(aOwner);
	++r->iSubSerial;
	if (!r->iWatch.IsNull() && r->iWatchSeen != r->iSubSerial)
		r->iWatch.Complete(r->iSubSerial);
	}

// aFrame's subscription aSubIndex ends: the window's counts drop, its owner hears it
void CRegServer::Unsubscribe(CRegSession* aFrame, TInt aSubIndex)
	{
	const CRegSession::TSub s = aFrame->iSubs[aSubIndex];
	aFrame->iSubs.Remove(aSubIndex);
	const TInt i = Find(s.iId);
	if (i < 0)
		return;					// (the window went first)
	TPheWinInfo& w = iWins[i].iInfo;
	--w.iSubscribers;
	if (s.iMode == EPheWinComposite)
		--w.iComposited;
	RDebug::Printf("[phewinreg] window %d: %d subscribers (%d compositing)", w.iId, w.iSubscribers, w.iComposited);
	SubsChanged(iWins[i].iSession);
	Changed();
	}

void CRegServer::SessionClosed(CRegSession* aSession)
	{
	TBool removed = EFalse;
	for (TInt i = iWins.Count() - 1; i >= 0; --i)
		if (iWins[i].iSession == aSession)
			{
			RDebug::Printf("[phewinreg] window %d gone with its session", iWins[i].iInfo.iId);
			iWins.Remove(i);
			removed = ETrue;
			}
	aSession->iWatch = RMessagePtr2();
	aSession->iAction = RMessagePtr2();
	aSession->iContentNotify = RMessagePtr2();		// (the kernel completes a closing session's messages)
	while (aSession->iSubs.Count())
		Unsubscribe(aSession, aSession->iSubs.Count() - 1);
	const TInt w = iWaiters.Find(aSession);
	if (w >= 0)
		iWaiters.Remove(w);
	if (removed)
		Changed();
	if (--iSessions == 0 && iWins.Count() == 0)
		CActiveScheduler::Stop();
	}

void CRegSession::ServiceL(const RMessage2& aMessage)
	{
	CRegServer& srv = Server();
	switch (aMessage.Function())
		{
	case EPheWinRegister:
	case EPheWinUpdate:
		{
		TPheWinInfoPckg p;
		aMessage.ReadL(0, p);
		TPheWinInfo& in = p();
		if (aMessage.Function() == EPheWinRegister)
			{
			CRegServer::TWin w;
			w.iInfo = in;
			w.iInfo.iId = srv.iNextId++;
			w.iInfo.iBitmap = 0;
			w.iInfo.iContentSerial = 0;
			w.iInfo.iDamage = TRect();
			w.iInfo.iGpuToken = 0;
			w.iInfo.iSubscribers = 0;
			w.iInfo.iComposited = 0;
			w.iInfo.iState &= ~EPheWinFocused;
			RThread client;
			RProcess proc;
			w.iInfo.iOwner = 0;
			if (aMessage.Client(client) == KErrNone)
				{
				if (client.Process(proc) == KErrNone)
					{
					w.iInfo.iOwner = (TUint)proc.Id().Id();
					proc.Close();
					}
				client.Close();
				}
			w.iSession = this;
			w.iStack = ++srv.iStackTop;
			srv.iWins.AppendL(w);
			RDebug::Print(_L("[phewinreg] window %d registered: type %d, %dx%d, '%S'"), w.iInfo.iId, w.iInfo.iType,
				w.iInfo.iGeometry.Width(), w.iInfo.iGeometry.Height(), &w.iInfo.iTitle);
			aMessage.Complete(w.iInfo.iId);
			}
		else
			{
			const TInt i = Owns(in.iId);
			if (i < 0) { aMessage.Complete(KErrNotFound); return; }
			TPheWinInfo& cur = srv.iWins[i].iInfo;
			cur.iType = in.iType;
			cur.iTransientFor = in.iTransientFor;
			cur.iGeometry = in.iGeometry;
			cur.iState = (in.iState & ~EPheWinFocused) | (cur.iState & EPheWinFocused);
			cur.iTitle = in.iTitle;
			aMessage.Complete(KErrNone);
			}
		srv.Changed();
		return;
		}
	case EPheWinContent:
		{
		const TInt i = Owns(aMessage.Int0());
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		TPckgBuf<TRect> d;
		aMessage.ReadL(2, d);
		TPheWinInfo& cur = srv.iWins[i].iInfo;
		// a GPU window's next frame (same token, no bitmap) changes nothing a compositor has to re-read: only the content tick moves
		// (hildon-port.md 7.49); a new token or a bitmap is a registry change (the compositor lists again)
		const TBool contentOnly = aMessage.Int3() != 0 && aMessage.Int3() == cur.iGpuToken && aMessage.Int1() == 0;
		cur.iBitmap = aMessage.Int1();
		cur.iDamage = d();
		cur.iGpuToken = aMessage.Int3();
		++cur.iContentSerial;
		aMessage.Complete(cur.iSubscribers);		// how many subscribed to this window
		if (contentOnly)
			srv.ContentChanged();
		else
			srv.Changed();
		return;
		}
	case EPheWinRemove:
		{
		const TInt i = Owns(aMessage.Int0());
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		srv.iWins.Remove(i);
		aMessage.Complete(KErrNone);
		srv.Changed();
		return;
		}
	case EPheWinList:
		{
		const TInt n = srv.iWins.Count();
		const TInt room = aMessage.GetDesMaxLengthL(0) / (TInt)sizeof(TPheWinInfo);
		if (room < n) { aMessage.Complete(n); return; }
		HBufC8* buf = HBufC8::NewLC(n * sizeof(TPheWinInfo) + 1);
		TPtr8 b = buf->Des();
		srv.AppendStacked(b, 0, 0);
		const TInt listed = b.Length() / (TInt)sizeof(TPheWinInfo);
		aMessage.WriteL(0, b);
		CleanupStack::PopAndDestroy(buf);
		aMessage.Complete(listed);
		return;
		}
	case EPheWinNotify:
		if (!iNotify.IsNull()) { aMessage.Complete(KErrInUse); return; }
		iSeen = (TUint)aMessage.Int0();
		if (iSeen && iSeen != srv.iSerial) { aMessage.Complete((TInt)srv.iSerial); return; }	// it changed since the caller looked
		iNotify = aMessage;
		srv.iWaiters.AppendL(this);
		return;
	case EPheWinContentNotify:
		if (!iContentNotify.IsNull()) { aMessage.Complete(KErrInUse); return; }
		iContentSeen = (TUint)aMessage.Int0();
		if (iContentSeen && iContentSeen != srv.iContentTicks) { aMessage.Complete((TInt)srv.iContentTicks); return; }
		iContentNotify = aMessage;
		return;
	case EPheWinNotifyCancel:
		if (!iNotify.IsNull())
			iNotify.Complete(KErrCancel);
		{
		const TInt w = srv.iWaiters.Find(this);
		if (w >= 0) srv.iWaiters.Remove(w);
		}
		aMessage.Complete(KErrNone);
		return;
	case EPheWinWatchers:
		if (!iWatch.IsNull()) { aMessage.Complete(KErrInUse); return; }
		iWatchSeen = aMessage.Int0();
		if (iWatchSeen != iSubSerial) { aMessage.Complete(iSubSerial); return; }
		iWatch = aMessage;
		return;
	case EPheWinSubscribe:
		{
		const TInt id = aMessage.Int0(), mode = aMessage.Int1();
		if (mode < EPheWinUnsubscribe || mode > EPheWinComposite) { aMessage.Complete(KErrArgument); return; }
		const TInt i = srv.Find(id);
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		if (srv.iWins[i].iSession == this) { aMessage.Complete(KErrArgument); return; }	// (a frame does not subscribe to itself)
		TInt s = 0;
		while (s < iSubs.Count() && iSubs[s].iId != id)
			++s;
		if (s < iSubs.Count())
			{
			if (iSubs[s].iMode == mode) { aMessage.Complete(KErrNone); return; }
			srv.Unsubscribe(this, s);
			}
		if (mode != EPheWinUnsubscribe)
			{
			const TSub sub = { id, mode };
			iSubs.AppendL(sub);
			TPheWinInfo& w = srv.iWins[i].iInfo;
			++w.iSubscribers;
			if (mode == EPheWinComposite)
				++w.iComposited;
			RDebug::Printf("[phewinreg] window %d: %d subscribers (%d compositing)", w.iId, w.iSubscribers, w.iComposited);
			srv.SubsChanged(srv.iWins[i].iSession);
			srv.Changed();
			}
		aMessage.Complete(KErrNone);
		return;
		}
	case EPheWinAction:
		{
		const TInt id = aMessage.Int0(), act = aMessage.Int1();
		if (act < EPheWinActClose || act > EPheWinActRaise || id <= 0 || id > 0x7FFFFFF) { aMessage.Complete(KErrArgument); return; }
		const TInt i = srv.Find(id);
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		TInt s = 0;
		while (s < iSubs.Count() && iSubs[s].iId != id)
			++s;
		if (s == iSubs.Count()) { aMessage.Complete(KErrPermissionDenied); return; }	// (only a frame that shows the window)
		CRegSession* owner = const_cast<CRegSession*>(srv.iWins[i].iSession);
		if (act == EPheWinActRaise)
			{
			srv.iWins[i].iStack = ++srv.iStackTop;
			srv.Changed();
			}
		if (act == EPheWinActFocus)		// the one focused window: the registry records it, everyone hears it through Notify
			{
			for (TInt k = 0; k < srv.iWins.Count(); ++k)
				srv.iWins[k].iInfo.iState &= ~EPheWinFocused;
			srv.iWins[i].iInfo.iState |= EPheWinFocused;
			srv.Changed();
			}
		const TInt r = owner->iActions.Append((id << 4) | act);
		if (r == KErrNone)
			{
			RDebug::Printf("[phewinreg] window %d: action %d for its owner", id, act);
			owner->DeliverAction();
			}
		aMessage.Complete(r);
		return;
		}
	case EPheWinPointer:
		{
		const TInt id = aMessage.Int0();
		const TInt i = id > 0 ? srv.Find(id) : KErrNotFound;
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		TInt s = 0;
		while (s < iSubs.Count() && iSubs[s].iId != id)
			++s;
		if (s == iSubs.Count()) { aMessage.Complete(KErrPermissionDenied); return; }	// (only a frame that shows the window)
		TPckgBuf<TPoint> pos;
		aMessage.ReadL(2, pos);
		CRegSession* owner = const_cast<CRegSession*>(srv.iWins[i].iSession);
		if (owner->iPointers.Count() >= KMaxPointers) { aMessage.Complete(KErrOverflow); return; }
		TPheWinPointer ev;
		ev.iId = id;  ev.iType = aMessage.Int1();  ev.iPos = pos();
		const TInt r = owner->iPointers.Append(ev);
		if (r == KErrNone)
			owner->DeliverPointer();
		aMessage.Complete(r);
		return;
		}
	case EPheWinPointerNotify:
		if (!iPointerMsg.IsNull()) { aMessage.Complete(KErrInUse); return; }
		iPointerMsg = aMessage;
		DeliverPointer();
		return;
	case EPheWinPointerCancel:
		if (!iPointerMsg.IsNull())
			iPointerMsg.Complete(KErrCancel);
		aMessage.Complete(KErrNone);
		return;
	case EPheWinActionNotify:
		if (!iAction.IsNull()) { aMessage.Complete(KErrInUse); return; }
		iAction = aMessage;
		DeliverAction();
		return;
	case EPheWinActionCancel:
		if (!iAction.IsNull())
			iAction.Complete(KErrCancel);
		aMessage.Complete(KErrNone);
		return;
	case EPheWinWatchersCancel:
		if (!iWatch.IsNull())
			iWatch.Complete(KErrCancel);
		aMessage.Complete(KErrNone);
		return;
	default:
		aMessage.Panic(KPheWinRegServer, 1);
		}
	}

static void RunL()
	{
	CActiveScheduler* as = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(as);
	CActiveScheduler::Install(as);
	CRegServer* srv = new (ELeave) CRegServer;
	CleanupStack::PushL(srv);
	const TInt r = srv->Start(KPheWinRegServer);
	if (r == KErrAlreadyExists) { RProcess::Rendezvous(r); CleanupStack::PopAndDestroy(2); return; }
	User::LeaveIfError(r);
	User::RenameThread(KPheWinRegServer);
	RProcess::Rendezvous(KErrNone);
	RDebug::Printf("[phewinreg] up");
	CActiveScheduler::Start();
	RDebug::Printf("[phewinreg] last session gone: exit");
	CleanupStack::PopAndDestroy(2);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, RunL());
	if (err != KErrNone)
		RProcess::Rendezvous(err);
	delete cleanup;
	return err;
	}
