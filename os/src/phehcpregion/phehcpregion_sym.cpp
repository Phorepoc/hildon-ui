// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehcpregion_sym.cpp -- the Symbian side of the Regional settings applet: the date format (TLocale).
#include <e32std.h>

extern "C" int phe_rg_get_date_format(void)
	{
	TLocale l;
	switch (l.DateFormat())
		{
		case EDateAmerican: return 1;
		case EDateJapanese: return 2;
		default: return 0;
		}
	}

extern "C" void phe_rg_set_date_format(int aFormat)
	{
	TLocale l;
	l.SetDateFormat(aFormat == 1 ? EDateAmerican : aFormat == 2 ? EDateJapanese : EDateEuropean);
	l.Set();
	}
