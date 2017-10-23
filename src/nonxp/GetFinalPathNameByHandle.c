// Code taken from WINE Project

#define WINVER 0x0501

#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <wine/unicode.h>

#define FILE_NAME_NORMALIZED 0x0
#define FILE_NAME_OPENED 0x8

#define VOLUME_NAME_DOS 0x0
#define VOLUME_NAME_GUID 0x1
#define VOLUME_NAME_NT 0x2
#define VOLUME_NAME_NONE 0x4

#define TRACE(...)
#define FIXME(...)
#define WARN(...)
#define strncmpiW _wcsnicmp
/***********************************************************************
 *           FILE_name_WtoA
 *
 * Convert a file name back to OEM/Ansi. Returns number of bytes copied.
 */
DWORD FILE_name_WtoA( LPCWSTR src, INT srclen, LPSTR dest, INT destlen )
{
    DWORD ret;

    if (srclen < 0) srclen = strlenW( src ) + 1;
    if (!AreFileApisANSI())
        RtlUnicodeToOemN( dest, destlen, &ret, src, srclen * sizeof(WCHAR) );
    else
        RtlUnicodeToMultiByteN( dest, destlen, &ret, src, srclen * sizeof(WCHAR) );
    return ret;
}

/***********************************************************************
 * GetFinalPathNameByHandleW (KERNEL32.@)
 */
DWORD WINAPI GetFinalPathNameByHandleW(HANDLE file, LPWSTR path, DWORD charcount, DWORD flags)
{
	WCHAR buffer[sizeof(OBJECT_NAME_INFORMATION) + MAX_PATH + 1];
	OBJECT_NAME_INFORMATION *info = (OBJECT_NAME_INFORMATION*)&buffer;
	WCHAR drive_part[MAX_PATH];
	DWORD drive_part_len;
	NTSTATUS status;
	DWORD result = 0;
	ULONG dummy;
	WCHAR *ptr;

	TRACE( "(%p,%p,%d,%x)\n", file, path, charcount, flags );

	/* check for invalid arguments */
	if (!path)
	{
		SetLastError( ERROR_INVALID_PARAMETER );
		return 0;
	}
	else if (file == INVALID_HANDLE_VALUE)
	{
		SetLastError( ERROR_INVALID_HANDLE );
		return 0;
	}
	else if (flags & ~(FILE_NAME_OPENED | VOLUME_NAME_GUID | VOLUME_NAME_NONE | VOLUME_NAME_NT))
	{
		WARN("Invalid or unsupported flags: %x\n", flags);
		SetLastError( ERROR_INVALID_PARAMETER );
		return 0;
	}

	/* get object name */
	status = NtQueryObject( file, ObjectNameInformation, &buffer, sizeof(buffer) - sizeof(WCHAR), &dummy );
	if (status != STATUS_SUCCESS)
	{
		SetLastError( RtlNtStatusToDosError( status ) );
		return 0;
	}
	else if (info->Name.Length < 4 * sizeof(WCHAR) || info->Name.Buffer[0] != '\\' ||
		info->Name.Buffer[1] != '?' || info->Name.Buffer[2] != '?' || info->Name.Buffer[3] != '\\' )
	{
		FIXME("Unexpected object name: %s\n", debugstr_wn(info->Name.Buffer, info->Name.Length / sizeof(WCHAR)));
		SetLastError( ERROR_GEN_FAILURE );
		return 0;
	}

	/* add terminating null character, remove "\\??\\" */
	info->Name.Buffer[info->Name.Length / sizeof(WCHAR)] = 0;
	info->Name.Length -= 4 * sizeof(WCHAR);
	info->Name.Buffer += 4;

	/* FILE_NAME_OPENED is not supported yet, and would require Wineserver changes */
	if (flags & FILE_NAME_OPENED)
	{
		FIXME("FILE_NAME_OPENED not supported\n");
		flags &= ~FILE_NAME_OPENED;
	}

	/* Get information required for VOLUME_NAME_NONE, VOLUME_NAME_GUID and VOLUME_NAME_NT */
	if (flags == VOLUME_NAME_NONE || flags == VOLUME_NAME_GUID || flags == VOLUME_NAME_NT)
	{
		if (!GetVolumePathNameW( info->Name.Buffer, drive_part, MAX_PATH ))
			return 0;

		drive_part_len = strlenW(drive_part);
		if (!drive_part_len || drive_part_len > strlenW(info->Name.Buffer) ||
			drive_part[drive_part_len-1] != '\\' ||
			strncmpiW( info->Name.Buffer, drive_part, drive_part_len ))
		{
			FIXME("Path %s returned by GetVolumePathNameW does not match file path %s\n",
			debugstr_w(drive_part), debugstr_w(info->Name.Buffer));
			SetLastError( ERROR_GEN_FAILURE );
			return 0;
		}
	}

	if (flags == VOLUME_NAME_NONE)
	{
		ptr = info->Name.Buffer + drive_part_len - 1;
		result = strlenW(ptr);
		if (result < charcount)
			memcpy(path, ptr, (result + 1) * sizeof(WCHAR));
		else result++;
	}
	else if (flags == VOLUME_NAME_GUID)
	{
		WCHAR volume_prefix[51];

		/* GetVolumeNameForVolumeMountPointW sets error code on failure */
		if (!GetVolumeNameForVolumeMountPointW( drive_part, volume_prefix, 50 ))
		return 0;

		ptr = info->Name.Buffer + drive_part_len;
		result = strlenW(volume_prefix) + strlenW(ptr);
		if (result < charcount)
		{
			path[0] = 0;
			strcatW(path, volume_prefix);
			strcatW(path, ptr);
		}
		else result++;
	}
	else if (flags == VOLUME_NAME_NT)
	{
		WCHAR nt_prefix[MAX_PATH];

		/* QueryDosDeviceW sets error code on failure */
		drive_part[drive_part_len - 1] = 0;
		if (!QueryDosDeviceW( drive_part, nt_prefix, MAX_PATH ))
		return 0;

		ptr = info->Name.Buffer + drive_part_len - 1;
		result = strlenW(nt_prefix) + strlenW(ptr);
		if (result < charcount)
		{
			path[0] = 0;
			strcatW(path, nt_prefix);
			strcatW(path, ptr);
		}
		else result++;
	}
	else if (flags == VOLUME_NAME_DOS)
	{
		static const WCHAR dos_prefix[] = {'\\','\\','?','\\', '\0'};

		result = strlenW(dos_prefix) + strlenW(info->Name.Buffer);
		if (result < charcount)
		{
			path[0] = 0;
			strcatW(path, dos_prefix);
			strcatW(path, info->Name.Buffer);
		}
		else result++;
	}
	else
	{
		/* Windows crashes here, but we prefer returning ERROR_INVALID_PARAMETER */
		WARN("Invalid combination of flags: %x\n", flags);
		SetLastError( ERROR_INVALID_PARAMETER );
	}

	return result;
}

/***********************************************************************
 * GetFinalPathNameByHandleA (KERNEL32.@)
 */
DWORD WINAPI GetFinalPathNameByHandleA(HANDLE file, LPSTR path, DWORD charcount, DWORD flags)
{
	WCHAR *str;
	DWORD result;

	TRACE( "(%p,%p,%d,%x)\n", file, path, charcount, flags );

	if (!path || !charcount)
		return GetFinalPathNameByHandleW(file, (LPWSTR)path, charcount, flags);

	str = HeapAlloc( GetProcessHeap(), 0, charcount * sizeof(WCHAR) );
	if (!str)
	{
		SetLastError( ERROR_NOT_ENOUGH_MEMORY );
		return 0;
	}

	result = GetFinalPathNameByHandleW(file, (LPWSTR)str, charcount, flags);
	if (result)
	{
		if (result < charcount)
		{
			result = FILE_name_WtoA( str, result, path, charcount - 1 );
			path[result] = 0;
		}
		else result--; /* Why does Windows do this? */
	}

	HeapFree( GetProcessHeap(), 0, str );
	return result;
}


