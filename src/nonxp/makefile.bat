windres nonxp.rc -o nonxp.res -O COFF
dllwrap -s -O3 -o nonxp.dll --def export.def --image-base 0x6a000000 nonxp.res -I. nonxp.c stub.c inet_ntop.c inet_pton.c if_nametoindex.c sync.c CriticalSection.c GetFinalPathNameByHandle.c GetTickCount64.c shellpath.c WSASendMsg.c -lmsvcrt -lntdll -lole32 -luuid -liphlpapi -lws2_32 -Wl,--insert-timestamp
del nonxp.res
pause
