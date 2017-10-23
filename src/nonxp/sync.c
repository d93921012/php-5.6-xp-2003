#define WINVER 0x0501

#include <wine/port.h>
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>

HANDLE keyed_event = NULL;

static inline int interlocked_dec_if_nonzero( int *dest )
{
    int val, tmp;
    for (val = *dest;; val = tmp)
    {
        if (!val || (tmp = interlocked_cmpxchg( dest, val - 1, val )) == val)
            break;
    }
    return val;
}

/* helper for kernel32->ntdll timeout format conversion */
static inline PLARGE_INTEGER get_nt_timeout( PLARGE_INTEGER pTime, DWORD timeout )
{
    if (timeout == INFINITE) return NULL;
    pTime->QuadPart = (ULONGLONG)timeout * -10000;
    return pTime;
}

/***********************************************************************
 *           RtlInitializeConditionVariable   (NTDLL.@)
 *
 * Initializes the condition variable with NULL.
 *
 * PARAMS
 *  variable [O] condition variable
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI RtlInitializeConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    variable->Ptr = NULL;
}

/***********************************************************************
 *           RtlWakeConditionVariable   (NTDLL.@)
 *
 * Wakes up one thread waiting on the condition variable.
 *
 * PARAMS
 *  variable [I/O] condition variable to wake up.
 *
 * RETURNS
 *  Nothing.
 *
 * NOTES
 *  The calling thread does not have to own any lock in order to call
 *  this function.
 */
void WINAPI RtlWakeConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    if (interlocked_dec_if_nonzero( (int *)&variable->Ptr ))
        NtReleaseKeyedEvent( keyed_event, &variable->Ptr, FALSE, NULL );
}

/***********************************************************************
 *           RtlWakeAllConditionVariable   (NTDLL.@)
 *
 * See WakeConditionVariable, wakes up all waiting threads.
 */
void WINAPI RtlWakeAllConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    int val = interlocked_xchg( (int *)&variable->Ptr, 0 );
    while (val-- > 0)
        NtReleaseKeyedEvent( keyed_event, &variable->Ptr, FALSE, NULL );
}

/***********************************************************************
 *           RtlSleepConditionVariableCS   (NTDLL.@)
 *
 * Atomically releases the critical section and suspends the thread,
 * waiting for a Wake(All)ConditionVariable event. Afterwards it enters
 * the critical section again and returns.
 *
 * PARAMS
 *  variable  [I/O] condition variable
 *  crit      [I/O] critical section to leave temporarily
 *  timeout   [I]   timeout
 *
 * RETURNS
 *  see NtWaitForKeyedEvent for all possible return values.
 */
NTSTATUS WINAPI RtlSleepConditionVariableCS( RTL_CONDITION_VARIABLE *variable, RTL_CRITICAL_SECTION *crit,
                                             const LARGE_INTEGER *timeout )
{
    NTSTATUS status;
    interlocked_xchg_add( (int *)&variable->Ptr, 1 );
    RtlLeaveCriticalSection( crit );

    status = NtWaitForKeyedEvent( keyed_event, &variable->Ptr, FALSE, timeout );
    if (status != STATUS_SUCCESS)
    {
        if (!interlocked_dec_if_nonzero( (int *)&variable->Ptr ))
            status = NtWaitForKeyedEvent( keyed_event, &variable->Ptr, FALSE, NULL );
    }

    RtlEnterCriticalSection( crit );
    return status;
}

/***********************************************************************
 *           SleepConditionVariableCS   (KERNEL32.@)
 */
BOOL WINAPI SleepConditionVariableCS( CONDITION_VARIABLE *variable, CRITICAL_SECTION *crit, DWORD timeout )
{
    NTSTATUS status;
    LARGE_INTEGER time;

    status = RtlSleepConditionVariableCS( variable, crit, get_nt_timeout( &time, timeout ) );

    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return FALSE;
    }
    return TRUE;
}

