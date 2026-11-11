// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehomeclock_sym.cpp -- the Symbian side of the clock home widget: the home time (TTime) and the clock format (TLocale).
#include <e32std.h>

extern "C" void phe_clk_now(int* aHour, int* aMinute, int* a24, int* aWday, int* aDay, int* aMonth, int* aYear)
	{
	TTime now;
	now.HomeTime();
	TDateTime dt = now.DateTime();
	TLocale l;
	*aHour = dt.Hour(); *aMinute = dt.Minute(); *a24 = l.TimeFormat() == ETime24 ? 1 : 0;
	*aWday = now.DayNoInWeek(); *aDay = dt.Day() + 1; *aMonth = dt.Month(); *aYear = dt.Year();
	}
