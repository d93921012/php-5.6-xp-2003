#define WINVER 0x0501
#include <stdint.h>
#include <winsock2.h>
#include <mswsock.h>

INT WINAPI WSASendMsg(SOCKET s, LPWSAMSG lpMsg, DWORD dwFlags,
			LPDWORD lpNumberOfBytesSent,
			LPWSAOVERLAPPED lpOverlapped,
			LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	if (lpMsg == NULL) {
		//FATAL("Error: no data in WSASendMsg_WRAPPER");
		return 0;
	}
	if (lpOverlapped != NULL) {
		//FATAL("Error: overlapped in WSASendMsg_WRAPPER");
		return 0;
	}
	if (lpCompletionRoutine != NULL) {
		//FATAL("Error: completion routine in WSASendMsg_WRAPPER");
		return 0;
	}
	char tmpbuf[65536];
	uint32_t tmplen = 0;
	DWORD i;
	for (i = 0; i < lpMsg->dwBufferCount; i++) {
		WSABUF * wsaBuf = &lpMsg->lpBuffers[i];
		if ((tmplen + wsaBuf->len) > sizeof(tmpbuf)) {
			//FATAL("Error: can not send data WSASendMsg_WRAPPER - data too long");
			return 0;
		}
		memcpy(tmpbuf + tmplen, wsaBuf->buf, wsaBuf->len);
		tmplen += wsaBuf->len;
	}
	int res = sendto(s, tmpbuf, tmplen, dwFlags, lpMsg->name, lpMsg->namelen);
	if (res == SOCKET_ERROR) {
		return res;
	}
	if (lpNumberOfBytesSent != NULL) {
		*lpNumberOfBytesSent = res;
	}
	return 0;
}

