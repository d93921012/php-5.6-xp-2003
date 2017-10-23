#pragma warning(disable:4503)
#pragma warning(disable:4786)

#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
using namespace std;

//#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))

typedef struct {
	const char * OldFuncName;
	const char * OldDll;
	const char * NewFuncName;
	const char * NewDll;
} PatchFunc;
PatchFunc patch[] = {
	// funcs
	{"GetFinalPathNameByHandleA", "kernel32.dll", "GetFinalPathNameByHandleA", "nonxp.dll"},
	{"GetFinalPathNameByHandleW", "kernel32.dll", "GetFinalPathNameByHandleW", "nonxp.dll"},
	{"GetTickCount64", "kernel32.dll", "GetTickCount64", "nonxp.dll"},
	{"InitializeConditionVariable", "kernel32.dll", "InitializeConditionVariable", "nonxp.dll"},
	{"InitializeCriticalSectionEx", "kernel32.dll", "InitializeCriticalSectionEx", "nonxp.dll"},
	{"SHGetKnownFolderPath", "shell32.dll", "SHGetKnownFolderPath", "nonxp.dll"},
	{"SleepConditionVariableCS", "kernel32.dll", "SleepConditionVariableCS", "nonxp.dll"},
	{"WSASendMsg", "ws2_32.dll", "WSASendMsg", "nonxp.dll"},
	{"WakeConditionVariable", "kernel32.dll", "WakeConditionVariable", "nonxp.dll"},
	{"if_nametoindex", "iphlpapi.dll", "if_nametoindex", "nonxp.dll"},
	{"inet_ntop", "ws2_32.dll", "inet_ntop", "nonxp.dll"},
	{"inet_pton", "ws2_32.dll", "inet_pton", "nonxp.dll"},

	// corrections
	{"_ftol2", "msvcrt.dll", "_ftol", "msvcrt.dll"},
	{"_ftol2_sse", "msvcrt.dll", "_ftol", "msvcrt.dll"},

	//{"__stdio_common_vsprintf_s", "api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsprintf_s", "msvcr110.dll"},
	//{"_calloc_base", "api-ms-win-crt-heap-l1-1-0.dll", "_calloc_base", "msvcr110.dll"},
	//{"_free_base", "api-ms-win-crt-heap-l1-1-0.dll", "_free_base", "msvcr110.dll"},
	//{"_malloc_base", "api-ms-win-crt-heap-l1-1-0.dll", "_malloc_base", "msvcr110.dll"},
	//{"abort", "api-ms-win-crt-runtime-l1-1-0.dll", "abort", "msvcr110.dll"},
	//{"atol", "api-ms-win-crt-convert-l1-1-0.dll", "atol", "msvcr110.dll"},
	//{"free", "api-ms-win-crt-heap-l1-1-0.dll", "free", "msvcr110.dll"},
	//{"malloc", "api-ms-win-crt-heap-l1-1-0.dll", "malloc", "msvcr110.dll"},
	//{"strcpy_s", "api-ms-win-crt-string-l1-1-0.dll", "strcpy_s", "msvcr110.dll"},
	//{"terminate", "api-ms-win-crt-runtime-l1-1-0.dll", "terminate", "msvcr110.dll"},

	// non-exist function corrections
	{"_ftol2", "msvcr100.dll", "_ftol", "msvcr100.dll"},
	{"_ftol2_sse", "msvcr100.dll", "_ftol", "msvcr100.dll"},
	{"time", "msvcr100.dll", "_time32", "msvcr100.dll"},

	// stubs
	{"CancelSynchronousIo", "kernel32.dll", "Stub1", "nonxp.dll"},
	{"FlsAlloc", "kernel32.dll", "Stub1", "nonxp.dll"},
	{"FlsFree", "kernel32.dll", "Stub1", "nonxp.dll"},
	{"FlsGetValue", "kernel32.dll", "Stub1", "nonxp.dll"},
	{"FlsSetValue", "kernel32.dll", "Stub2", "nonxp.dll"},
	{"InitializeCriticalSectionEx", "kernel32.dll", "Stub2", "nonxp.dll"},
	{"SetFileInformationByHandle", "kernel32.dll", "Stub3", "nonxp.dll"},
	{"SetThreadStackGuarantee", "kernel32.dll", "Stub1", "nonxp.dll"},

};
#define PATCHCOUNT ARRAYSIZE(patch)
int funcspatched = 0;
int forcefindjmp = 0;

const DWORD zero = 0;
char *buf = NULL;
#define NEWBUFSIZE 0x100000
char newbuf[NEWBUFSIZE];
size_t newbytes = 0;

IMAGE_DOS_HEADER *dosheader;
IMAGE_NT_HEADERS32 *peheader;
DWORD imagebase32;
DWORD oldiatptr, oldiatsize;

IMAGE_SECTION_HEADER *secheader;
DWORD sections = 0;
DWORD fileendrva;

DWORD *reloctable = NULL, reloccount = 0;
DWORD *reloctable2 = NULL, reloccount2 = 0; // force reloc 专用
//

typedef struct {
	WORD Offset:12;
	WORD Status:4;
} RelocItem;

typedef struct {
	DWORD NameRva;
	DWORD ThunkPtr;
} ThunkData;
typedef map<string, ThunkData> FuncList;
map<string, FuncList> ImportList;
map<string, FuncList>::iterator ili;

DWORD RawToRva(DWORD p) {
	int i;
	for (i=sections-1; i>=0; i--) {
		if (secheader[i].PointerToRawData <= p)
			return p - secheader[i].PointerToRawData + secheader[i].VirtualAddress;
	}
	return 0;
}

DWORD RvaToRaw(DWORD p) {
	int i;
	for (i=sections-1; i>=0; i--) {
		if (secheader[i].VirtualAddress <= p)
			return p - secheader[i].VirtualAddress + secheader[i].PointerToRawData;
	}
	return 0;
}

DWORD align(DWORD p, DWORD num) {
	return ((p - 1) & ~(num - 1)) + num;
}

DWORD alignptr(DWORD p) {
	return align(p, sizeof(size_t));
}

char * memfind(char *buf, size_t len, char *token, size_t tlen){
	size_t i, j;
	for(i=0; i<len; i++){
		for(j=0; j<tlen; j++){
			if(buf[i+j]!=token[j])
				goto nexti;
		}
		return buf+i;
nexti:		;
	}
	return NULL;
}

void GetImp(DWORD *prva, DWORD *size) {
	*prva = peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	*size = peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
}

void SetImp(DWORD prva, DWORD size) {
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = prva;
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = size;
}

void GetIat(DWORD *prva, DWORD *size) {
	*prva = peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
	*size = peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size;
}

void SetIat(DWORD prva, DWORD size) {
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = prva;
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = size;
}

void GetReloc(DWORD *prva, DWORD *size) {
	*prva = peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	*size = peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
}

DWORD GetFileEndRva() {
	return align(secheader[sections - 1].VirtualAddress + secheader[sections - 1].Misc.VirtualSize,
		peheader->OptionalHeader.SectionAlignment);
}

void FreeRelocTable() {
	if (reloctable) {
		free(reloctable);
		reloctable = NULL;
		reloccount = 0;
	}
	if (reloctable2) {
		free(reloctable2);
		reloctable2 = NULL;
		reloccount2 = 0;
	}
}

void ParseRelocTable(DWORD praw, DWORD size) {
	IMAGE_BASE_RELOCATION *rel;
	DWORD pp;
	DWORD blockcount;
	DWORD i, j;
	RelocItem *r;

	if (reloccount > 0 || reloctable != NULL)
		FreeRelocTable();

	reloccount = 0;
	pp = praw;
	while (pp < praw + size) {
		rel = (IMAGE_BASE_RELOCATION*)&buf[pp];
		blockcount = (rel->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
		reloccount += blockcount;
		pp += rel->SizeOfBlock;
	}

	reloctable = (DWORD *)malloc(reloccount * sizeof(DWORD));
	
	j = 0;
	pp = praw;
	while (pp < praw + size) {
		rel = (IMAGE_BASE_RELOCATION*)&buf[pp];
		blockcount = (rel->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
		for (i=0; i<blockcount; i++) {
			r = (RelocItem*)&buf[pp + sizeof(IMAGE_BASE_RELOCATION) + i*sizeof(RelocItem)];
			if (r->Status == IMAGE_REL_BASED_HIGHLOW) {
				reloctable[j] = rel->VirtualAddress + r->Offset;
				j++;
			}
		}
		pp += rel->SizeOfBlock;
	}
	reloccount = j;
}

// 无重定位信息暴力查找
void ForceScanJmpTable() {
	char *p;
	DWORD l, i, value;
	unsigned char mem[][4] = {
		{1, 0x68},		// push DWORD
		{1, 0xB8},		// mov eax, DWORD
		{1, 0xB9},		// mov ecx, DWORD
		{1, 0xBA},		// mov edx, DWORD
		{1, 0xBB},		// mov ebx, DWORD
		{1, 0xBC},		// mov esp, DWORD
		{1, 0xBD},		// mov ebp, DWORD
		{1, 0xBE},		// mov esi, DWORD
		{1, 0xBF},		// mov edi, DWORD

		{2, 0x8F, 0x05},	// pop [DWORD]
		{2, 0xFE, 0x05},	// inc byte ptr[DWORD]
		{2, 0xFF, 0x05},	// inc dword ptr[DWORD]
		{2, 0xFE, 0x0D},	// dec byte ptr[DWORD]
		{2, 0xFF, 0x0D},	// dec dword ptr[DWORD]
		{2, 0xFF, 0x15},	// call [DWORD]
		{2, 0xFF, 0x25},	// jmp [DWORD]
		{2, 0xFF, 0x35},	// push [DWORD]
		
		{3, 0xFF, 0x24, 0x85},	// jmp [eax * 4 + DWORD]
		{3, 0xFF, 0x24, 0x8D},	// jmp [ecx * 4 + DWORD]
		{3, 0xFF, 0x24, 0x95},	// jmp [edx * 4 + DWORD]
		{3, 0xFF, 0x24, 0x9D},	// jmp [ebx * 4 + DWORD]
		{3, 0xFF, 0x24, 0xAD},	// jmp [ebp * 4 + DWORD]
		{3, 0xFF, 0x24, 0xB5},	// jmp [esi * 4 + DWORD]
		{3, 0xFF, 0x24, 0xBD},	// jmp [edi * 4 + DWORD]

		{1, 0xA0},		// mov al, [DWORD]
		{2, 0x8A, 0x0D},	// mov cl, [DWORD]
		{2, 0x8A, 0x15},	// mov dl, [DWORD]
		{2, 0x8A, 0x1D},	// mov bl, [DWORD]
		{2, 0x8A, 0x25},	// mov ah, [DWORD]
		{2, 0x8A, 0x2D},	// mov ch, [DWORD]
		{2, 0x8A, 0x35},	// mov dh, [DWORD]
		{2, 0x8A, 0x3D},	// mov bh, [DWORD]
		
		{1, 0xA1},		// mov eax, [DWORD]
		{2, 0x8B, 0x0D},	// mov ecx, [DWORD]
		{2, 0x8B, 0x15},	// mov edx, [DWORD]
		{2, 0x8B, 0x1D},	// mov ebx, [DWORD]
		{2, 0x8B, 0x25},	// mov esp, [DWORD]
		{2, 0x8B, 0x2D},	// mov ebp, [DWORD]
		{2, 0x8B, 0x35},	// mov esi, [DWORD]
		{2, 0x8B, 0x3D},	// mov edi, [DWORD]
		
		{1, 0xA2},		// mov [DWORD], al
		{2, 0x88, 0x0D},	// mov [DWORD], cl
		{2, 0x88, 0x15},	// mov [DWORD], dl
		{2, 0x88, 0x1D},	// mov [DWORD], bl
		{2, 0x88, 0x25},	// mov [DWORD], ah
		{2, 0x88, 0x2D},	// mov [DWORD], ch
		{2, 0x88, 0x35},	// mov [DWORD], dh
		{2, 0x88, 0x3D},	// mov [DWORD], bh
		
		{1, 0xA3},		// mov [DWORD], eax
		{2, 0x89, 0x0D},	// mov [DWORD], ecx
		{2, 0x89, 0x15},	// mov [DWORD], edx
		{2, 0x89, 0x1D},	// mov [DWORD], ebx
		{2, 0x89, 0x25},	// mov [DWORD], esp
		{2, 0x89, 0x2D},	// mov [DWORD], ebp
		{2, 0x89, 0x35},	// mov [DWORD], esi
		{2, 0x89, 0x3D},	// mov [DWORD], esp
		
		{2, 0x80, 0x3D},	// cmp byte ptr[DWORD], byte
		{2, 0x81, 0x3D},	// cmp dword ptr[DWORD], dword
		{2, 0x83, 0x3D},	// cmp dword ptr[DWORD], byte

		{2, 0x38, 0x05},	// cmp [DWORD], al
		{2, 0x38, 0x0D},	// cmp [DWORD], cl
		{2, 0x38, 0x15},	// cmp [DWORD], dl
		{2, 0x38, 0x1D},	// cmp [DWORD], bl
		{2, 0x38, 0x25},	// cmp [DWORD], ah
		{2, 0x38, 0x2D},	// cmp [DWORD], ch
		{2, 0x38, 0x35},	// cmp [DWORD], dh
		{2, 0x38, 0x3D},	// cmp [DWORD], bh

		{2, 0x39, 0x05},	// cmp [DWORD], eax
		{2, 0x39, 0x0D},	// cmp [DWORD], ecx
		{2, 0x39, 0x15},	// cmp [DWORD], edx
		{2, 0x39, 0x1D},	// cmp [DWORD], ebx
		{2, 0x39, 0x25},	// cmp [DWORD], esp
		{2, 0x39, 0x2D},	// cmp [DWORD], ebp
		{2, 0x39, 0x35},	// cmp [DWORD], esi
		{2, 0x39, 0x3D},	// cmp [DWORD], edi
		
		{2, 0x3A, 0x05},	// cmp al, [DWORD]
		{2, 0x3A, 0x0D},	// cmp cl, [DWORD]
		{2, 0x3A, 0x15},	// cmp dl, [DWORD]
		{2, 0x3A, 0x1D},	// cmp bl, [DWORD]
		{2, 0x3A, 0x25},	// cmp ah, [DWORD]
		{2, 0x3A, 0x2D},	// cmp ch, [DWORD]
		{2, 0x3A, 0x35},	// cmp dh, [DWORD]
		{2, 0x3A, 0x3D},	// cmp bh, [DWORD]

		{2, 0x3B, 0x05},	// cmp eax, [DWORD]
		{2, 0x3B, 0x0D},	// cmp ecx, [DWORD]
		{2, 0x3B, 0x15},	// cmp edx, [DWORD]
		{2, 0x3B, 0x1D},	// cmp ebx, [DWORD]
		{2, 0x3B, 0x25},	// cmp esp, [DWORD]
		{2, 0x3B, 0x2D},	// cmp ebp, [DWORD]
		{2, 0x3B, 0x35},	// cmp esi, [DWORD]
		{2, 0x3B, 0x3D},	// cmp edi, [DWORD]
		
		{3, 0x0F, 0xB6, 0x05},	// movzx eax, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x0D},	// movzx ecx, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x15},	// movzx edx, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x1D},	// movzx ebx, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x25},	// movzx esp, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x2D},	// movzx ebp, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x35},	// movzx esi, byte ptr[DWORD]
		{3, 0x0F, 0xB6, 0x3D},	// movzx edi, byte ptr[DWORD]

		{3, 0x0F, 0xB7, 0x05},	// movzx eax, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x0D},	// movzx ecx, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x15},	// movzx edx, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x1D},	// movzx ebx, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x25},	// movzx esp, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x2D},	// movzx ebp, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x35},	// movzx esi, word ptr[DWORD]
		{3, 0x0F, 0xB7, 0x3D},	// movzx edi, word ptr[DWORD]

		{3, 0x0F, 0xBE, 0x05},	// movsx eax, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x0D},	// movsx ecx, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x15},	// movsx edx, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x1D},	// movsx ebx, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x25},	// movsx esp, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x2D},	// movsx ebp, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x35},	// movsx esi, byte ptr[DWORD]
		{3, 0x0F, 0xBE, 0x3D},	// movsx edi, byte ptr[DWORD]

		{3, 0x0F, 0xBF, 0x05},	// movsx eax, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x0D},	// movsx ecx, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x15},	// movsx edx, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x1D},	// movsx ebx, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x25},	// movsx esp, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x2D},	// movsx ebp, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x35},	// movsx esi, word ptr[DWORD]
		{3, 0x0F, 0xBF, 0x3D},	// movsx edi, word ptr[DWORD]
	};
	vector<DWORD> v, v2;
	DWORD praw;
	DWORD size;
	
	if (oldiatptr) {
		// .text 使用上述pattern匹配
		praw = secheader[0].PointerToRawData;
		size = secheader[0].SizeOfRawData;
		for (i=0; i<sizeof(mem)/sizeof(mem[0]); i++) {
			p = &buf[praw];
			while (p) {
				l = size - (p - &buf[praw]);
				p = memfind(p, l, (char*)&mem[i][1], mem[i][0]);
				if (p) {
					value = *(DWORD*)&buf[p-buf+mem[i][0]];
					if (value >= imagebase32 + oldiatptr && value < imagebase32 + oldiatptr + oldiatsize) {
						v.push_back(p-buf+mem[i][0]);
					}
					p += mem[i][0];
					l = size - (p - &buf[praw]);
				}
			}
		}

		// .data 穷举所有DWORD
		praw = secheader[1].PointerToRawData;
		size = secheader[1].SizeOfRawData;
		for (i=praw; i<praw+size; i+=4) {
			value = *(DWORD*)&buf[i];
			if (value >= imagebase32 + oldiatptr && value < imagebase32 + oldiatptr + oldiatsize) {
				v.push_back(i);
			}
		}
		// .rdata 穷举所有DWORD
		praw = secheader[2].PointerToRawData;
		size = secheader[2].SizeOfRawData;
		for (i=praw; i<praw+size; i+=4) {
			value = *(DWORD*)&buf[i];
			if (value >= oldiatptr && value < oldiatptr + oldiatsize) {
				v2.push_back(i);
			}
		}
	}

	reloccount = v.size();
	reloctable = (DWORD*)malloc(reloccount*sizeof(DWORD));
	for (i=0; i<reloccount; i++) {
		reloctable[i] = v[i];
	}
	reloccount2 = v2.size();
	reloctable2 = (DWORD*)malloc(reloccount2*sizeof(DWORD));
	for (i=0; i<reloccount2; i++) {
		reloctable2[i] = v2[i];
	}
	if (1) {
		FILE *fp=fopen("debug1.txt","wt");
		for (i=0; i<reloccount; i++) {
			fprintf(fp,"%p\n",v[i]);
		}
		fprintf(fp,"=============================\n");
		for (i=0; i<reloccount2; i++) {
			fprintf(fp,"%p\n",v2[i]);
		}
		fclose(fp);
	}
}

// 无重定位信息暴力替换
void ForceReloc(DWORD oldptr, DWORD newptr) {
	DWORD *p;
	DWORD l, i;
	
	if (reloctable) {
		for (i=0; i<reloccount; i++) {
			p = (DWORD*)&buf[reloctable[i]];
			if (*p == imagebase32 + oldptr)
				*p = imagebase32 + newptr;
		}
	}
	if (reloctable2) {
		for (i=0; i<reloccount2; i++) {
			p = (DWORD*)&buf[reloctable2[i]];
			if (*p == oldptr)
				*p = newptr;
		}
	}
}

// 有重定位信息正常替换
void DoReloc(DWORD oldptr, DWORD newptr) {
	DWORD i;
	DWORD *p;
	
	if (forcefindjmp) {
		ForceReloc(oldptr, newptr);
		return;
	}
	if (reloctable == NULL) {
		return;
	}
	for (i=0; i<reloccount; i++) {
		p = (DWORD*)&buf[RvaToRaw(reloctable[i])];
		if (*p == imagebase32 + oldptr)
			*p = imagebase32 + newptr;
	}
}

DWORD WriteFileAndGetPtr(const void *p, size_t len) {
	DWORD ptr;

	ptr = newbytes;
	memmove(&newbuf[newbytes], p, len);
	newbytes += len;
	newbytes = alignptr(newbytes);
	return ptr + fileendrva;
}

int main(int argc, char**argv) {
	
	char *file = argv[1];
	FILE *fp;
	size_t flen;
	DWORD p, size, i, j, dllcount, funccount, oldimpptr, oldimpsize, oldintptr, newiatsize;
	IMAGE_IMPORT_DESCRIPTOR *imp;
	DWORD impcount;
	int needwrite = 0;
	
	if (argc < 2) {
		fprintf(stderr, "Usage: %s PeFileToPatch\n", argv[0]);
		return 1;
	}

	fp = fopen(file, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Can't open file %s\n", file);
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	buf = (char*)malloc(flen);
	fseek(fp, 0, SEEK_SET);
	fread(buf, 1, flen, fp);
	fclose(fp);
	
	printf("Processing PE header...\n");
	
	dosheader = (IMAGE_DOS_HEADER*)&buf[0];
	peheader = (IMAGE_NT_HEADERS32*)&buf[dosheader->e_lfanew];
	if (peheader->Signature != IMAGE_NT_SIGNATURE) {
		fprintf(stderr, "Not a valid PE file\n");
		goto error;
	}
	if (peheader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
		fprintf(stderr, "Only support x86 executables\n");
		goto error;
	}

	sections = peheader->FileHeader.NumberOfSections;
	secheader = (IMAGE_SECTION_HEADER*)((char*)peheader + sizeof(IMAGE_NT_HEADERS32) - sizeof(peheader->OptionalHeader) + peheader->FileHeader.SizeOfOptionalHeader);

	fileendrva = GetFileEndRva();

	imagebase32 = peheader->OptionalHeader.ImageBase;

	GetImp(&p, &oldimpsize);
	imp = (IMAGE_IMPORT_DESCRIPTOR*)&buf[RvaToRaw(p)];
	oldimpptr = p;
	impcount = oldimpsize / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;
	
	GetIat(&oldiatptr, &oldiatsize);
	oldintptr = imp->OriginalFirstThunk;

	GetReloc(&p, &size);
	if (p == 0) {
		if (argc >= 3 && strcmp(argv[2], "-f") == 0) {
			forcefindjmp = 1;
			ForceScanJmpTable();
		} else {
			fprintf(stderr, "No relocation table found, not supported\n");
			goto error;
		}
	} else {
		p = RvaToRaw(p);
		ParseRelocTable(p, size);
	}
	
	printf("Processing import table...\n");

	// scan import list
	ImportList.clear();
	for (i=0; i<impcount; i++, imp++) {
		FuncList fl;
		FuncList::iterator fli;
		DWORD *ThunkTable;
		DWORD FirstThunkTable;
		IMAGE_IMPORT_BY_NAME *nn;
		ThunkData thunkdata;
		string funcname, dllname;
		char tmps[20];

		if (imp->FirstThunk == 0)
			break;
		
		fl.clear();
		ThunkTable = (DWORD*)&buf[RvaToRaw(imp->OriginalFirstThunk)];
		FirstThunkTable = imp->FirstThunk;
		while (*ThunkTable != 0) {
			if (IMAGE_SNAP_BY_ORDINAL32(*ThunkTable)) {
				thunkdata.NameRva = *ThunkTable;
				sprintf(tmps, "#%d", IMAGE_ORDINAL(*ThunkTable));
				funcname = string(tmps);
			} else {
				thunkdata.NameRva = *ThunkTable;
				nn = (IMAGE_IMPORT_BY_NAME*)&buf[RvaToRaw(*ThunkTable)];
				funcname = string((char *)&nn->Name);
			}
			thunkdata.ThunkPtr = FirstThunkTable;
			ThunkTable++;
			FirstThunkTable += sizeof(FirstThunkTable);
			if ((fli = fl.find(funcname)) != fl.end()) {
				if (forcefindjmp)	// 没有重定向的暴力搜索，先处理函数名重复的情况
					ForceReloc(thunkdata.ThunkPtr, fli->second.ThunkPtr);
			} else {
				fl.insert(make_pair(funcname, thunkdata));
			}
		}

		dllname = string(&buf[RvaToRaw(imp->Name)]);
		transform(dllname.begin(), dllname.end(), dllname.begin(), ::tolower);
		if ((ili = ImportList.find(dllname)) != ImportList.end()) {
			for (fli = fl.begin(); fli != fl.end(); fli++) {
				ili->second.insert(*fli);
			}
		} else {
			ImportList.insert(make_pair(dllname, fl));
		}
	}
	// 导入表没这么大,PE头里面的是乱写的
	if (i != impcount) {
		oldimpsize = (i + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
	}
	if (0) {
		FuncList::iterator fli;
		FILE *fp = fopen("debug.txt","wt");
		for (ili = ImportList.begin(); ili != ImportList.end(); ili++) {
			for (fli = ili->second.begin(); fli != ili->second.end(); fli++) {
				fprintf(fp, "%s\t%s\t%p\t%p\n", ili->first.c_str(), fli->first.c_str(), fli->second.NameRva, fli->second.ThunkPtr);
			}
		}
		fclose(fp);
	}
	
	// replace import functions
	for (i=0; i<PATCHCOUNT; i++) {
		FuncList::iterator fli;
		ThunkData oldthunk;
		string funcname, dllname, oldfuncname, olddllname;

		olddllname = string(patch[i].OldDll);
		dllname = string(patch[i].NewDll);
		oldfuncname = string(patch[i].OldFuncName);
		funcname = string(patch[i].NewFuncName);
		if ((ili = ImportList.find(olddllname)) == ImportList.end()) {
			//fprintf(stderr, "Patch dll %s not exist\n", patch[i].OldDll);
			continue;
		}
		if ((fli = ili->second.find(oldfuncname)) == ili->second.end()) {
			//fprintf(stderr, "Patch function %s not exist\n", patch[i].OldFuncName);
			continue;
		}
		printf("Patching function %s.%s -> %s.%s\n", patch[i].OldDll, patch[i].OldFuncName, patch[i].NewDll, patch[i].NewFuncName);
		if (oldfuncname != funcname) {
			char buf[256] = "";
			memcpy(&buf[2], funcname.c_str(), funcname.size() + 1);
			oldthunk.NameRva = WriteFileAndGetPtr(buf, funcname.size() + 3);
		} else {
			oldthunk.NameRva = fli->second.NameRva;
		}
		oldthunk.ThunkPtr = fli->second.ThunkPtr;
		ili->second.erase(fli++);
		ili = ImportList.find(dllname);
		if (ili == ImportList.end()) {
			FuncList fl;
			fl.insert(make_pair(funcname, oldthunk));
			ImportList.insert(make_pair(dllname, fl));
		} else {
			ili->second.insert(make_pair(funcname, oldthunk));
		}
		funcspatched++;
	}

	if (peheader->OptionalHeader.MajorSubsystemVersion >= 6) {
		peheader->OptionalHeader.MajorSubsystemVersion = 5;
		peheader->OptionalHeader.MinorSubsystemVersion = 1;
		needwrite = 1;
	}

	if (funcspatched == 0) {
		printf("Nothing need to patch file %s\n", file);
		goto error;
	}
	
	if (((char *)&secheader[sections] - buf) + sizeof(IMAGE_SECTION_HEADER) > peheader->OptionalHeader.SizeOfHeaders) {
		fprintf(stderr, "No space for new sections in file headers\n");
		goto error;
	}
	
	memset(&buf[RvaToRaw(oldimpptr)], 0, oldimpsize);
	memset(&buf[RvaToRaw(oldiatptr)], 0, oldiatsize);
//	memset(&buf[RvaToRaw(oldintptr)], 0, oldiatsize);

	printf("Processing relocation...\n");

	// generate new import list and thunk list, and do relocation if needed
	IMAGE_IMPORT_DESCRIPTOR *newimp;
	dllcount = ImportList.size();
	newimp = (IMAGE_IMPORT_DESCRIPTOR*)malloc(dllcount * sizeof(IMAGE_IMPORT_DESCRIPTOR));
	newiatsize = 0;
	for (i=0, ili=ImportList.begin(); i<dllcount; i++, ili++) {
		FuncList::iterator fli;
		funccount = ili->second.size();
		DWORD newthunk;
		DWORD newthunkptr;
		newimp[i].FirstThunk = fileendrva + newbytes;
		for (j=0, fli=ili->second.begin(); fli!=ili->second.end(); j++, fli++) {
			newthunk = fli->second.NameRva;
			newthunkptr = WriteFileAndGetPtr(&newthunk, sizeof(DWORD));
			DoReloc(fli->second.ThunkPtr, newthunkptr);
		}
		WriteFileAndGetPtr(&zero, sizeof(DWORD));
		newiatsize += (funccount + 1) * sizeof(DWORD);
	}
	newimp[0].OriginalFirstThunk = WriteFileAndGetPtr(&newbuf[newimp[0].FirstThunk - fileendrva], newiatsize);
	SetIat(newimp[0].FirstThunk, newiatsize);
	for (i=0, ili=ImportList.begin(); i<dllcount; i++, ili++) {
		newimp[i].Name = WriteFileAndGetPtr(ili->first.c_str(), ili->first.size() + 1);
		newimp[i].OriginalFirstThunk = newimp[i].FirstThunk - newimp[0].FirstThunk + newimp[0].OriginalFirstThunk;
		newimp[i].TimeDateStamp = 0;
		newimp[i].ForwarderChain = 0;
	}
	p = WriteFileAndGetPtr(newimp, dllcount * sizeof(IMAGE_IMPORT_DESCRIPTOR));
	SetImp(p, (dllcount + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));
	free(newimp);
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;
	
	printf("Writing output...\n");

	flen = align(secheader[sections - 1].PointerToRawData + secheader[sections - 1].SizeOfRawData, peheader->OptionalHeader.FileAlignment);
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = 0;
	peheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = 0;
	
	DWORD newbytesrva, newbytesraw;
	newbytesrva = align(newbytes, peheader->OptionalHeader.SectionAlignment);
	newbytesraw = align(newbytes, peheader->OptionalHeader.FileAlignment);
	strcpy((char*)&secheader[sections].Name, ".idata");
	secheader[sections].VirtualAddress = fileendrva;
	secheader[sections].Misc.VirtualSize = newbytesrva;
	secheader[sections].PointerToRawData = flen;
	secheader[sections].SizeOfRawData = newbytesraw;
	secheader[sections].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA;
	secheader[sections].PointerToRelocations = 0;
	secheader[sections].PointerToLinenumbers = 0;
	secheader[sections].NumberOfRelocations = 0;
	secheader[sections].NumberOfLinenumbers = 0;

	peheader->FileHeader.NumberOfSections++;
	peheader->OptionalHeader.SizeOfImage += newbytesraw;

	fp = fopen(file, "wb");
	//fp = fopen("out.dll", "wb");
	if (fp == NULL) {
		fprintf(stderr, "Can't open file %s for write\n", file);
		goto error;
	}
	fwrite(buf, 1, flen, fp);
	fwrite(newbuf, 1, newbytesraw, fp);
	fclose(fp);
/*
	for (ili = ImportList.begin(); ili!=ImportList.end(); ili++) {
		printf("%s\n", ili->first.c_str());

		FuncList::iterator fli;
		for (fli = ili->second.begin(); fli!=ili->second.end(); fli++)
			printf("%s\n", fli->first.c_str());
	}
*/
	free(buf);
	printf("Patch completed: %s\n", file);
	return 0;
	
error:
	if (needwrite) {
		fp = fopen(file, "wb");
		if (fp == NULL) {
			fprintf(stderr, "Can't open file %s for write\n", file);
			goto error2;
		}
		fwrite(buf, 1, flen, fp);
		fwrite(newbuf, 1, newbytesraw, fp);
		fclose(fp);
	}
error2:
	free(buf);
	return 1;
}
