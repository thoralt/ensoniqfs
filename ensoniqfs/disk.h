//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// DISK I/O FUNCTIONS header file
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
#ifndef _DISK_H_
#define _DISK_H_

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include "omniflop.h"
#include "diskstructure.h"

#define DLLEXPORT __declspec (dllexport)

#define READ_AHEAD	256

#define MAX_IMAGE_FILES		16

//----------------------------------------------------------------------------
// Prototypes
//----------------------------------------------------------------------------
BOOL UnlockMediaType(const char *szDrive, BOOL bLog);
BOOL UnlockMediaType(const char *szDrive, BOOL bLog);
BOOL LockMediaType(const char *szDrive, MEDIA_TYPE MediaType, BOOL bLog);
BOOL EnableExtendedFormats(const char *szDrive, BOOL bEnable);
void MakeLegalName(char *cName);
void GetShortEnsoniqFiletype(unsigned char ucType, char *cType);
int ReadBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf);
int GetContiguousBlocks(DISK *pDisk, DWORD dwNumBlocks);
int GetNextFreeBlock(DISK *pDisk, int iStartingBlock);
int AdjustFreeBlocks(DISK *pDisk, int iAdjust);
int DetectImageFileType(HANDLE h, unsigned char *ucReturnBuf, 
	DWORD *dwDataOffset, DWORD *dwGieblerMapOffset);
	
//----------------------------------------------------------------------------
// DLL exports
//----------------------------------------------------------------------------
DLLEXPORT void __stdcall FreeDiskList(int iShowProgress, DISK *pRoot);
DLLEXPORT int __stdcall ReadBlocks(DISK *pDisk, DWORD dwBlock,
	DWORD dwNumBlocks, unsigned char *ucBuf);
DLLEXPORT int __stdcall WriteBlocks(DISK *pDisk, DWORD dwBlock,
	DWORD dwNumBlocks, unsigned char *ucBuf);
DLLEXPORT int __stdcall WriteBlocksUncached(DISK *pDisk, DWORD dwBlock,
	DWORD dwNumBlocks, unsigned char *ucBuf);
DLLEXPORT int __stdcall GetFATEntry(DISK *pDisk, DWORD dwBlock);
DLLEXPORT int __stdcall SetFATEntry(DISK *pDisk, DWORD dwBlock,
	DWORD dwNewValue);
DLLEXPORT DISK __stdcall *ScanDevices(DWORD dwAllowNonEnsoniqFilesystems);
DLLEXPORT int __stdcall GetUsageCount(void);

#endif

