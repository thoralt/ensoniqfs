//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// DISK READ AND WRITE CACHE FUNCTIONS header file
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
#ifndef _CACHE_H_
#define _CACHE_H_

#include "error.h"
#include "log.h"
#include "disk.h"

//----------------------------------------------------------------------------
// #defines
//----------------------------------------------------------------------------
// cache size (in blocks)
#define CACHE_SIZE	8192

#define CACHE_FLAG_NONE		0
#define CACHE_FLAG_DIRTY	1

//----------------------------------------------------------------------------
// Prototypes
//----------------------------------------------------------------------------
int CacheWriteBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf);
int CacheInsertReadBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf);
int CacheReadBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf);

//----------------------------------------------------------------------------
// DLL exports
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall CacheFlush(DISK *pDisk);

#endif
