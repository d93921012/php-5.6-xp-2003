#include <windows.h>

int Is2003 = 0;
int Is2003OrGreater = 0;

BOOL WINAPI DllMain (HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
	unsigned int ver, major, minor;
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		ver = GetVersion();
		major = ver & 0xFF;
		minor = ver >> 8 & 0xFF;
		if (major == 5 && minor >= 2 || major >= 6) {
			Is2003OrGreater = 1;
			if (major == 5 && minor == 2)
				Is2003 = 1;
			else
				Is2003 = 0;
		} else {
			Is2003OrGreater = 0;
			Is2003 = 0;
		}
		break;
		
	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}
