/* vim:ts=8:sts=8:sw=4:noai:noexpandtab
 *
 * Interface name to interface index function.  Defined as part of RFC2553
 * for IPv6 basic socket extensions.
 *
 * Copyright (c) 2006-2011 Miru Limited.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define WINVER 0x0501

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string.h>
#include <assert.h>


//#define NAMETOINDEX_DEBUG

#define MAX_TRIES		3
#define DEFAULT_BUFFER_SIZE	4096
#define IF_NAMESIZE 32

typedef unsigned short sa_family_t;

/* Retrieve adapter index via name.
 * Wine edition:  First try GetAdapterIndex() then fallback to enumerating
 * adapters via GetAdaptersInfo().
 *
 * On error returns zero, no errors are defined.
 *
 * Requires Windows 2000 or Wine 1.0.
 */

/*
static
unsigned					// type matching if_nametoindex()
getadaptersinfo_nametoindex (
	const sa_family_t	iffamily,
	const char*		ifname
        )
{
	if (NULL == ifname) return 0;

	assert (AF_INET6 != iffamily);

	DWORD dwRet, ifIndex;
	ULONG ulOutBufLen = DEFAULT_BUFFER_SIZE;
	PIP_ADAPTER_INFO pAdapterInfo = NULL;
	PIP_ADAPTER_INFO pAdapter = NULL;

// loop to handle interfaces coming online causing a buffer overflow
// between first call to list buffer length and second call to enumerate.
 
	unsigned int i;
	for (i = MAX_TRIES; i; i--)
	{
		//pgm_debug ("IP_ADAPTER_INFO buffer length %lu bytes.", ulOutBufLen);
		pAdapterInfo = (IP_ADAPTER_INFO*)malloc (ulOutBufLen);
		dwRet = GetAdaptersInfo (pAdapterInfo, &ulOutBufLen);
		if (ERROR_BUFFER_OVERFLOW == dwRet) {
			free (pAdapterInfo);
			pAdapterInfo = NULL;
		} else {
			break;
		}
	}

	switch (dwRet) {
	case ERROR_SUCCESS:	// NO_ERROR
		break;
	case ERROR_BUFFER_OVERFLOW:
		//pgm_warn (_("GetAdaptersInfo repeatedly failed with ERROR_BUFFER_OVERFLOW."));
		if (pAdapterInfo)
			free (pAdapterInfo);
		return 0;
	default:
		//pgm_warn (_("GetAdaptersInfo failed"));
		if (pAdapterInfo)
			free (pAdapterInfo);
		return 0;
	}

	for (pAdapter = pAdapterInfo;
		 pAdapter;
		 pAdapter = pAdapter->Next)
	{
		IP_ADDR_STRING *pIPAddr;
		for (pIPAddr = &pAdapter->IpAddressList;
			 pIPAddr;
			 pIPAddr = pIPAddr->Next)
		{
// skip null adapters
			if (strlen (pIPAddr->IpAddress.String) == 0)
				continue;

			if (0 == strncmp (ifname, pAdapter->AdapterName, IF_NAMESIZE)) {
				ifIndex = pAdapter->Index;
				free (pAdapterInfo);
				return ifIndex;
			}
		}
	}

	if (pAdapterInfo)
		free (pAdapterInfo);
	return 0;
}
*/

/* Retrieve adapter index via name.
 * Windows edition:  First try GetAdapterIndex() then fallback to enumerating
 * adapters via GetAdaptersAddresses().
 *
 * On error returns zero, no errors are defined.
 *
 * Requires Windows XP or Wine 1.3.
 */

static
unsigned					/* type matching if_nametoindex() */
getadaptersaddresses_nametoindex (
	const sa_family_t	iffamily,
	const char*		ifname
        )
{
	if (NULL == ifname) return 0;

	ULONG ifIndex;
	DWORD dwSize = DEFAULT_BUFFER_SIZE, dwRet;
	IP_ADAPTER_ADDRESSES *pAdapterAddresses = NULL, *adapter;
	char szAdapterName[IF_NAMESIZE];

// first see if GetAdapterIndex is working,

	strncpy(szAdapterName, ifname, sizeof(szAdapterName));
	dwRet = GetAdapterIndex ((LPWSTR)szAdapterName, &ifIndex);
	if (NO_ERROR == dwRet)
		return ifIndex;

// fallback to finding index via iterating adapter list

// loop to handle interfaces coming online causing a buffer overflow
// between first call to list buffer length and second call to enumerate.

	unsigned int i;
	for (i = MAX_TRIES; i; i--)
	{
		pAdapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc (dwSize);
		dwRet = GetAdaptersAddresses (AF_UNSPEC,
						GAA_FLAG_SKIP_ANYCAST |
						GAA_FLAG_SKIP_DNS_SERVER |
						GAA_FLAG_SKIP_FRIENDLY_NAME |
						GAA_FLAG_SKIP_MULTICAST,
						NULL,
						pAdapterAddresses,
						&dwSize);
		if (ERROR_BUFFER_OVERFLOW == dwRet) {
			free (pAdapterAddresses);
			pAdapterAddresses = NULL;
		} else {
			break;
		}
	}

	switch (dwRet) {
	case ERROR_SUCCESS:
		break;
	case ERROR_BUFFER_OVERFLOW:
		//pgm_warn (_("GetAdaptersAddresses repeatedly failed with ERROR_BUFFER_OVERFLOW"));
		if (pAdapterAddresses)
			free (pAdapterAddresses);
		return 0;
	default:
		//pgm_warn (_("GetAdaptersAddresses failed"));
		if (pAdapterAddresses)
			free (pAdapterAddresses);
		return 0;
	}

	for (adapter = pAdapterAddresses;
		adapter;
		adapter = adapter->Next)
	{
		if (0 == strcmp (szAdapterName, adapter->AdapterName)) {
			ifIndex = AF_INET6 == iffamily ? adapter->Ipv6IfIndex : adapter->IfIndex;
			free (pAdapterAddresses);
			return ifIndex;
		}
	}

	if (pAdapterAddresses)
		free (pAdapterAddresses);
	return 0;
}

/* Retrieve interface index for a specified adapter name.
 * On error returns zero, no errors are defined.
 */

unsigned __stdcall				/* type matching if_nametoindex() */
if_nametoindex (
	const sa_family_t	iffamily,
	const char*		ifname
        )
{
	if (NULL == ifname) return 0;

	return getadaptersaddresses_nametoindex (iffamily, ifname);
}

/* eof */
