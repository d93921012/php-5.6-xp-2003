#include <stdint.h>
//#include <windows.h>

/*
typedef ULONG (__stdcall * NTQUERYSYSTEMINFORMATION)(IN int, IN OUT PVOID, INT ULONG, OUT PULONG);

NTQUERYSYSTEMINFORMATION _NtQuerySystemInformation = (NTQUERYSYSTEMINFORMATION)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), ("NtQuerySystemInformation"));

typedef struct _SYSTEM_TIME_OF_DAY_INFORMATION
{
	LARGE_INTEGER BootTime;
	LARGE_INTEGER CurrentTime;
	LARGE_INTEGER TimeZoneBias;
	ULONG CurrentTimeZoneId;
} SYSTEM_TIME_OF_DAY_INFORMATION, *PSYSTEM_TIME_OF_DAY_INFORMATION;
*/
extern int Is2003OrGreater;
uint64_t __stdcall GetTickCount(void);

uint64_t __stdcall GetTickCount64()
{
//	SYSTEM_TIME_OF_DAY_INFORMATION  st;
//	ULONG                           oSize = 0;
//	_NtQuerySystemInformation(3, &st, sizeof(st), &oSize);
//	return (st.CurrentTime.QuadPart - st.BootTime.QuadPart)/10000;
	if (Is2003OrGreater)	// Win2003+ GetTickCount����40λ������������34��Ÿ�λ��Ҳ���Ը���ɷ���64λ������Ū��
		return GetTickCount();
	else			// WinXP GetTickCountֻ����32λ����������Ĵ����Ƿ���40λ��
		asm (
		"movl $0x7FFE0000, %edx\n"
		"movl (%edx), %eax\n"
		"mull 4(%edx)\n"
		"shrdl $0x18, %edx, %eax\n"
		"shrl $0x18, %edx\n"
		);
}

