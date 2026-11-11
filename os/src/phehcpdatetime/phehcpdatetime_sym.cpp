// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehcpdatetime_sym.cpp -- the Symbian side of the Date and time applet: the home time (TTime, User::SetHomeTime) and the clock format (TLocale).
#include <e32std.h>

extern "C" void phe_dt_get(int* aYear, int* aMonth, int* aDay, int* aHour, int* aMinute)
	{
	TTime now;
	now.HomeTime();
	TDateTime dt = now.DateTime();
	*aYear = dt.Year(); *aMonth = dt.Month(); *aDay = dt.Day() + 1; *aHour = dt.Hour(); *aMinute = dt.Minute();
	}

extern "C" int phe_dt_set(int aYear, int aMonth, int aDay, int aHour, int aMinute)
	{
	TDateTime dt;
	TInt r = dt.Set(aYear, (TMonth)aMonth, aDay - 1, aHour, aMinute, 0, 0);
	if (r != KErrNone) return r;
	return User::SetHomeTime(TTime(dt));
	}

extern "C" int phe_dt_get_24h(void)
	{
	TLocale l;
	return l.TimeFormat() == ETime24 ? 1 : 0;
	}

extern "C" void phe_dt_set_24h(int aOn)
	{
	TLocale l;
	l.SetTimeFormat(aOn ? ETime24 : ETime12);
	l.Set();
	}
