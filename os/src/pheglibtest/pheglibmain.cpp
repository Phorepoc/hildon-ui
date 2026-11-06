// pheglibmain.cpp -- Symbian side of pheglibtest: runs the C test and reports over RDebug (serial log). With "--phe-run <test>" or
// "--GTestSubprocess" in the command line it is a CHILD of the test run (a GLib test program, or a g_test_trap_subprocess re-run).
#include <e32std.h>
#include <e32debug.h>
extern "C" int phe_glib_test_run(void);
extern "C" int phe_glib_child(int argc, char** argv);
extern "C" void phe_log(const char* aMsg) { RDebug::Printf("[pheglib] %s", aMsg); }
int main(int argc, char** argv)
	{
	if (argc > 1) RDebug::Printf("[pheglib] child started: argc=%d argv1=%s argv2=%s", argc, argv[1], argc > 2 ? argv[2] : "-");
	for (int i = 1; i < argc; i++)
		if (argv[i][0] == '-' && argv[i][1] == '-' && (argv[i][2] == 'p' || argv[i][2] == 'G'))
			return phe_glib_child(argc, argv);
	phe_log("main entered");
	int fails = phe_glib_test_run();
	RDebug::Printf("[pheglib] DONE fails=%d", fails);
	return fails;
	}
