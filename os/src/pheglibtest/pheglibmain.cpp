// pheglibmain.cpp -- Symbian side of pheglibtest: runs the C test and reports over RDebug (serial log).
#include <e32std.h>
#include <e32debug.h>
extern "C" int phe_glib_test_run(void);
extern "C" void phe_log(const char* aMsg) { RDebug::Printf("[pheglib] %s", aMsg); }
int main()
	{
	phe_log("main entered");
	int fails = phe_glib_test_run();
	RDebug::Printf("[pheglib] DONE fails=%d", fails);
	return fails;
	}
