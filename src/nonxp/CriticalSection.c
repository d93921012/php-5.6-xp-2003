#define WINVER 0x0501

#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>

#define FIXME(...)

/***********************************************************************
 *           RtlInitializeCriticalSectionEx   (NTDLL.@)
 *
 * Initialises a new critical section with a given spin count and flags.
 *
 * PARAMS
 *   crit      [O] Critical section to initialise.
 *   spincount [I] Number of times to spin upon contention.
 *   flags     [I] RTL_CRITICAL_SECTION_FLAG_ flags from winnt.h.
 *
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * NOTES
 *  Available on Vista or later.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlDeleteCriticalSection(),
 *  RtlEnterCriticalSection(), RtlLeaveCriticalSection(),
 *  RtlTryEnterCriticalSection(), RtlSetCriticalSectionSpinCount()
 */
NTSTATUS WINAPI RtlInitializeCriticalSectionEx( RTL_CRITICAL_SECTION *crit, ULONG spincount, ULONG flags )
{
    if (flags & (RTL_CRITICAL_SECTION_FLAG_DYNAMIC_SPIN|RTL_CRITICAL_SECTION_FLAG_STATIC_INIT))
        FIXME("(%p,%u,0x%08x) semi-stub\n", crit, spincount, flags);

    /* FIXME: if RTL_CRITICAL_SECTION_FLAG_STATIC_INIT is given, we should use
     * memory from a static pool to hold the debug info. Then heap.c could pass
     * this flag rather than initialising the process heap CS by hand. If this
     * is done, then debug info should be managed through Rtlp[Allocate|Free]DebugInfo
     * so (e.g.) MakeCriticalSectionGlobal() doesn't free it using HeapFree().
     */
    if (flags & RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO)
        crit->DebugInfo = NULL;
    else
        crit->DebugInfo = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(RTL_CRITICAL_SECTION_DEBUG));

    if (crit->DebugInfo)
    {
        crit->DebugInfo->Type = 0;
        crit->DebugInfo->CreatorBackTraceIndex = 0;
        crit->DebugInfo->CriticalSection = crit;
        crit->DebugInfo->ProcessLocksList.Blink = &(crit->DebugInfo->ProcessLocksList);
        crit->DebugInfo->ProcessLocksList.Flink = &(crit->DebugInfo->ProcessLocksList);
        crit->DebugInfo->EntryCount = 0;
        crit->DebugInfo->ContentionCount = 0;
	crit->DebugInfo->Flags = 0;
	crit->DebugInfo->CreatorBackTraceIndexHigh = 0;
	crit->DebugInfo->SpareWORD = 0;
    }
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    crit->LockSemaphore  = 0;
    if (NtCurrentTeb()->Peb->NumberOfProcessors <= 1) spincount = 0;
    crit->SpinCount = spincount & ~0x80000000;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *           InitializeCriticalSectionEx   (KERNEL32.@)
 *
 * Initialise a critical section with a spin count and flags.
 *
 * PARAMS
 *  crit      [O] Critical section to initialise.
 *  spincount [I] Number of times to spin upon contention.
 *  flags     [I] CRITICAL_SECTION_ flags from winbase.h.
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: Nothing. If the function fails an exception is raised.
 *
 * NOTES
 *  spincount is ignored on uni-processor systems.
 */
BOOL WINAPI InitializeCriticalSectionEx( CRITICAL_SECTION *crit, DWORD spincount, DWORD flags )
{
    NTSTATUS ret = RtlInitializeCriticalSectionEx( crit, spincount, flags );
    if (ret) RtlRaiseStatus( ret );
    return !ret;
}

