/*
 *                      ***** TA BlackBox DLL Runner *****
 *
 *                      Transparent Architecture
 *
 * Idea of the C-starter comes from the OpenBUGS project. See: http://www.mathstat.helsinki.fi/openbugs/LinBUGS.html
 * Refinement: Eugene Temirgaleev (http://forum.oberoncore.ru/memberlist.php?mode=viewprofile&u=7)
 *
 *   Dmitry Dagaev 2012.
 *
 */

#include <ta.h>
#ifdef _WINDOWS
BOOL CtrlHandler( DWORD fdwCtrlType ) 
{ 
	switch( fdwCtrlType ) 
	{ 
		case CTRL_C_EVENT: 
		case CTRL_CLOSE_EVENT: 
		case CTRL_BREAK_EVENT: 
			exit(0);
			return TRUE;
	}
	return FALSE; 
}
#else
#include <dlfcn.h>

#endif

typedef int (TaInitLibStationData) (void *lsd);
#ifdef _WINDOWS
typedef void (*Procedure1) (int);
#else
typedef void (*Procedure1) (int) __attribute ((stdcall));
#endif

/* ------------------------------------------------------- */
#ifndef _WINDOWS
void ParsToEnv(int argc, char **argv)
{
	const char * const argcId = "bb-argc";
	const char * const argvId = "bb-argv";
	int res, ok;
	char argcS[9], argvS[9];
	
	res = sprintf(argcS, "%08X", argc);
	ok = res > 0;
	if (ok) {
		res = sprintf(argvS, "%08X", (unsigned)argv);
		ok = res == 8;
	}
	if (ok) {
		res = setenv(argcId, argcS, 1);
		ok = res == 0;
	}
	if (ok) {
		res = setenv(argvId, argvS, 1);
		ok = res == 0;
	}
	if (!ok) {
		fprintf(stderr, "Failed to setenv \"%s\" & \"%s\"\n", 
			argcId, argvId);
	}
}
#endif

/* ------------------------------------------------------- */
int main(int argc, char **argv)
{
	void *plib;
	int i, res;
	char modname[256], *libname, buffer[2048], *p;
	Procedure1 skbsProc;

	TaInitLibStationData *initLib;
	if (argc < 2) {
		printf("usage: %s dynlibname\n", argv[0]);
		return 1;
	}
	strcpy(modname, "Ta_");
	strcat(modname, argv[1]);
	libname = &modname[3];
	p = strchr(modname, '.');
	if (p)
		*p = 0;

#ifdef _WINDOWS
	strcat(modname, ".dll");
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );
	plib = (void *)LoadLibraryA(libname);
	if (!plib) {
		printf("* LoadLibrary %s: error 0x%x\n",
			libname, GetLastError());
		return 2;
	}
	initLib = (TaInitLibStationData *) GetProcAddress((HMODULE)plib, 
		"taGetLibStationData");
	if (!initLib) {
		printf("* dlsym taGetLibStationData: error 0x%x\n", GetLastError());
		return 2;
	}
	skbsProc = (Procedure1) GetProcAddress((HMODULE)plib, "SetKernelBaseStack");
	if (!skbsProc) {
		printf("* dlsym SetKernelBaseStack: error 0x%x\n", GetLastError());
		return 2;
    }
	res = 0;
	__asm {mov res, ESP};
	skbsProc(res - 8);
#else
	ParsToEnv(argc, argv);
	strcat(modname, ".so");
	dlerror();
	plib = dlopen(libname, RTLD_NOW);
	if (!plib) {
		printf("* dlopen %s: %s\n", libname, dlerror());
		return 2;
	}
	dlerror();
	initLib = (TaInitLibStationData *) dlsym(plib, 
		"taGetLibStationData");
	if (!initLib) {
		printf("* dlsym taGetLibStationData: %s\n", dlerror());
		return 2;
    	}

	skbsProc = (Procedure1)dlsym(plib, "SetKernelBaseStack");
	if (!skbsProc) {
		printf("* dlsym SetKernelBaseStack: %s\n", dlerror());
		return 2;
    	}
	asm ("movl %%esp, %[res]" : [res] "=m" (res) );
	skbsProc(res - 8);
#endif
	i =
	(*initLib)(NULL);
	if (i < 0)
		printf("taGetLibStationData return:%d\n", i);
	return 0;
}


