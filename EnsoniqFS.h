//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// MAIN MODULE header file
//----------------------------------------------------------------------------
//
// (c) 2006 Thoralt Franz
//
// This source code was written using Dev-Cpp 4.9.9.2
// If you want to compile it, get Dev-Cpp. Normally the code should compile
// with other IDEs/compilers too (with small modifications), but I did not
// test it.
//
//----------------------------------------------------------------------------
// License
//----------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
// MA  02110-1301, USA.
// 
// Alternatively, download a copy of the license here:
// http://www.gnu.org/licenses/gpl.txt
//----------------------------------------------------------------------------
#ifndef _ENSONIQFS_H_
#define _ENSONIQFS_H_

#define DLLEXPORT __declspec (dllexport)

#define FILE_TYPE_EMPTY				0x00
#define FILE_TYPE_DIRECTORY			0x02
#define FILE_TYPE_EPS_BANK			0x04
#define FILE_TYPE_PARENT_DIRECTORY	0x08
#define FILE_TYPE_EPS16_BANK		0x17
#define FILE_TYPE_ASR_SONG			0x1D
#define FILE_TYPE_ASR_BANK			0x1E
#define FILE_TYPE_ASR_AUDIOTRACK	0x1F

//----------------------------------------------------------------------------
// structs for Ensoniq directory
//----------------------------------------------------------------------------
typedef struct _ENSONIQDIRENTRY
{
	char cName[13], cLegalName[13];
	unsigned char ucType, ucMultiFileIndex;
	DWORD dwContiguous, dwStart, dwLen;
} ENSONIQDIRENTRY;

typedef struct _VIRTUALWAVEFILE
{
	char cName[13], cLegalName[13];
	DWORD dwContiguous1, dwStart1, dwLen1;
	DWORD dwContiguous2, dwStart2, dwLen2;
	unsigned char ucIsStereo;
} VIRTUALWAVEFILE;

typedef struct _ENSONIQDIR
{
	ENSONIQDIRENTRY Entry[39];
	unsigned char ucDirectory[1024];
	DWORD dwDirectoryBlock;
	VIRTUALWAVEFILE VirtualWaveEntry[58]; // max. 39 waves + 19 stereo pairs	
} ENSONIQDIR;

//----------------------------------------------------------------------------
// handle struct for find operations
//----------------------------------------------------------------------------
typedef struct _FIND_HANDLE
{
	struct _FIND_HANDLE *pNext, *pPrevious;
	char cPath[260];
	int iNextDirIndex;
	ENSONIQDIR EnsoniqDir;
	DISK *pDisk;
} FIND_HANDLE;

//----------------------------------------------------------------------------
// file type constants
//----------------------------------------------------------------------------
#define FILETYPE_UNKNOWN	0
#define FILETYPE_GKH		1
#define FILETYPE_PLAIN		2
#define FILETYPE_MODE1CD	3
#define FILETYPE_GIEBLER	4

//----------------------------------------------------------------------------
// copy mode constants
//----------------------------------------------------------------------------
#define COPY_DOS		0
#define COPY_ENSONIQ	1

//----------------------------------------------------------------------------
// prototypes
//----------------------------------------------------------------------------
void AddToImageList(char *cName);

// prototypes for usage in bank.c
void upcase(char *c);
int ReadDirectoryFromPath(FIND_HANDLE *pHandle, unsigned char ucShowWarning);
DISK *GetDiskFromPath(char *cPath);
int ReadDirectory(DISK *pDisk, ENSONIQDIR *pDir, unsigned char ucShowWarning);

    


//----------------------------------------------------------------------------
// DLL exports
//----------------------------------------------------------------------------
DLLEXPORT void __stdcall FsSetDefaultParams(FsDefaultParamStruct* dps);
DLLEXPORT int __stdcall FsInit(int PluginNr, tProgressProc pProgressProc,
	tLogProc pLogProc, tRequestProc pRequestProc);
DLLEXPORT HANDLE __stdcall FsFindFirst(char* cPath, WIN32_FIND_DATA *FindData);
DLLEXPORT BOOL __stdcall FsFindNext(HANDLE Handle, WIN32_FIND_DATA *FindData);
DLLEXPORT int __stdcall FsFindClose(HANDLE Handle);
DLLEXPORT void __stdcall FsGetDefRootName(char* cDefRootName, int iMaxLen);
DLLEXPORT int __stdcall FsGetFile(char* RemoteName, char* LocalName,
	int CopyFlags, RemoteInfoStruct* ri);
DLLEXPORT BOOL __stdcall FsMkDir(char* Path);
DLLEXPORT BOOL __stdcall FsRemoveDir(char* RemoteName);
DLLEXPORT BOOL __stdcall FsDeleteFile(char* RemoteName);
DLLEXPORT int __stdcall FsPutFile(char* LocalName, char* RemoteName,
	int CopyFlags);
DLLEXPORT void __stdcall FsStatusInfo(char* RemoteDir, int InfoStartEnd,
	int InfoOperation);
DLLEXPORT int __stdcall FsRenMovFile(char* OldName, char* NewName, BOOL Move,
	BOOL OverWrite, RemoteInfoStruct* ri);
DLLEXPORT int __stdcall FsExecuteFile(HWND MainWin, char* RemoteName,
	char* Verb);
	
DLLEXPORT int __stdcall FsExtractCustomIcon(char* RemoteName,
											int ExtractFlags,
											HICON* TheIcon);
#endif
