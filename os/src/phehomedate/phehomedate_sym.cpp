// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehomedate_sym.cpp -- the Symbian side of the date home widget: the home date (TTime).
#include <e32std.h>
extern "C" void phe_date_now(int* aWday, int* aDay, int* aMonth, int* aYear)
	{
	TTime now;
	now.HomeTime();
	TDateTime dt = now.DateTime();
	*aWday = now.DayNoInWeek(); *aDay = dt.Day() + 1; *aMonth = dt.Month(); *aYear = dt.Year();
	}
