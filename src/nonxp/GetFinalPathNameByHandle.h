#ifndef GETFINALPATHNAMEBYHADLE_H___
#define GETFINALPATHNAMEBYHADLE_H___

#if WINVER < 0x0600

#include <windows.h>

#define FILE_NAME_NORMALIZED 0x0
#define FILE_NAME_OPENED 0x8

#define VOLUME_NAME_DOS 0x0
#define VOLUME_NAME_GUID 0x1
#define VOLUME_NAME_NT 0x2
#define VOLUME_NAME_NONE 0x4

DWORD WINAPI GetFinalPathNameByHandleW(HANDLE file, LPWSTR path, DWORD charcount, DWORD flags);
DWORD WINAPI GetFinalPathNameByHandleA(HANDLE file, LPSTR path, DWORD charcount, DWORD flags);
#ifdef UNICODE
#define GetFinalPathNameByHandle GetFinalPathNameByHandleW
#else
#define GetFinalPathNameByHandle GetFinalPathNameByHandleA
#endif

#endif

#endif
