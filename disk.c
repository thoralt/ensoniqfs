//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// DISK I/O FUNCTIONS
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
#include <io.h>
#include "log.h"
#include "error.h"
#include "disk.h"
#include "cache.h"
#include "progressdlg.h"
#include "fsplugin.h"
#include "ini.h"

//----------------------------------------------------------------------------
// externals
//----------------------------------------------------------------------------
extern FsDefaultParamStruct g_DefaultParams;	// initialization parameters
								// for DLL, needed to find INI file
extern DISK *g_pDiskListRoot;	// pointer to global device list root

// global options
extern int g_iOptionEnableFloppy;
extern int g_iOptionEnableCDROM;
extern int g_iOptionEnableImages;
extern int g_iOptionEnablePhysicalDisks;
extern int g_iOptionAutomaticRescan;

//----------------------------------------------------------------------------
// GetShortEnsoniqFiletype
// 
// Get textual file type description
// 
// -> ucType = type code
//    cType = buffer to receive textual description (13 bytes + NUL)
// <- --
//----------------------------------------------------------------------------
void GetShortEnsoniqFiletype(unsigned char ucType, char *cType)
{
	switch(ucType)
	{
		case 0x03:
			strcpy(cType, "Instrument   ");
			break;

		case 0x04:
		case 0x17:
		case 0x1E:
			strcpy(cType, "Bank         ");
			break;

		case 0x18:
		case 0x21:
			strcpy(cType, "Effect       ");
			break;

		case 0x06:
		case 0x1A:
		case 0x1D:
			strcpy(cType, "Song/Seq     ");
			break;

		case 0x05:
		case 0x19:
		case 0x1C:
		case 0x1F:
			strcpy(cType, "Sequence     ");
			break;

		case 0x09:
		case 34:
			strcpy(cType, "Macro        ");
			break;

		case 0x07:
			strcpy(cType, "System Ex.   ");
			break;

		case 0x01:
		case 0x1B:
		case 0x20:
			strcpy(cType, "O.S.         ");
			break;

		default:
			strcpy(cType, "unknown type ");
			break;
	}
}

//----------------------------------------------------------------------------
// GetNextFreeBlock
// 
// Find the next free block in the FAT
// 
// -> pDisk = pointer to initialized disk structure
//    iStartingBlock = first block to test if free
// <- =0: error or no free block found
//    >0: free block
//----------------------------------------------------------------------------
int GetNextFreeBlock(DISK *pDisk, int iStartingBlock)
{
	int iBlock;
	DWORD i;

	// loop through all FAT entries	
	for(i=iStartingBlock; i<pDisk->dwBlocks; i++)
	{
		iBlock = GetFATEntry(pDisk, i);
		if(-1==iBlock) return 0;
		
		// free block?
		if(0==iBlock) return i;
	}
		
	return 0;
}

//----------------------------------------------------------------------------
// GetContiguousBlocks
// 
// Try to find <dwNumBlocks> contiguous free blocks from FAT
// 
// -> pDisk = pointer to initialized disk structure
//    dwNumBlocks = number of free blocks to find
// <- =0: error or no <dwNumBlocks> contiguous free blocks found
//    >0: starting block
//----------------------------------------------------------------------------
int GetContiguousBlocks(DISK *pDisk, DWORD dwNumBlocks)
{
	static int iRecursiveCounter = 0;
	DWORD i, dwStart = 0, dwCount = 0;
	int iBlock;

	iRecursiveCounter++;
	LOG("GetContiguousBlocks("); LOG_INT(dwNumBlocks); LOG("): ");
	
	// loop through all FAT entries	
	for(i=pDisk->dwLastFreeFATEntry; i<pDisk->dwBlocks; i++)
	{
		iBlock = GetFATEntry(pDisk, i);
		if(-1==iBlock)
		{
			LOG("failed.\n");
			iRecursiveCounter--;
			return 0;
		}
		
		// free block?
		if(0==iBlock)
		{
			// first free block?
			if(0==dwCount) dwStart = i;
			
			dwCount++;

			// did we find enough?
			if(dwCount>=dwNumBlocks)
			{
				LOG("OK.\n");
				
				// memorize last free FAT entry after the currently found
				// blocks, so next time we search for a free block, we don't
				// have to start from block #0
				// the tradeoff: there will be holes inside the used blocks
				// (everytime when a large file is allocated, all smaller
				// chunks before that file will be skipped)
				pDisk->dwLastFreeFATEntry = dwStart + dwNumBlocks;
				iRecursiveCounter--;
				return dwStart;
			}
		}
		else
		{
			// reset counter because contiguous stream was interrupted
			// by a used block
			dwCount = 0;
		}
	}
	
	// if we end up here, there's (a) no contiguous blocks of the requested
	// size on that disk or (b) due to the use of pDisk->dwLastFreeFATEntry
	// we skipped a few "holes" which could possibly be enough for this
	// particular file -> so we start _one_ additional search from the
	// beginning (recursive one level deep)
	pDisk->dwLastFreeFATEntry = 0;
	if(1==iRecursiveCounter)
	{
		dwStart = GetContiguousBlocks(pDisk, dwNumBlocks);
		if(0!=dwStart)
		{
			pDisk->dwLastFreeFATEntry = dwStart + dwNumBlocks;
			iRecursiveCounter--;
			return dwStart;
		}
	}
	
	LOG("failed.\n");
	iRecursiveCounter--;
	return 0;
}

//----------------------------------------------------------------------------
// ReadBlocks
// 
// Read multiple blocks from CDROM, harddisk or image file
// 
// -> pDisk = pointer to initialized disk structure
//    dwBlock = first block to read
//    dwNumBlocks = number of blocks to read
//    ucBuf = pointer to destination buffer (dwNumBlocks*512 bytes)
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall ReadBlocks(DISK *pDisk, DWORD dwBlock, DWORD dwNumBlocks,
			   unsigned char *ucBuf)
{
	int i, iResult;
	
	for(i=0; i<(int)dwNumBlocks; i++)
	{
		iResult = ReadBlock(pDisk, dwBlock + i, ucBuf + i*512);
		if(ERR_OK!=iResult) return iResult;
	}
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// WriteBlocksUncached
// 
// Write multiple blocks to harddisk or image file (uncached).
// 
// -> pDisk = pointer to initialized disk structure
//    dwBlock = first block to read
//    dwNumBlocks = number of blocks to read
//    ucBuf = pointer to destination buffer (dwNumBlocks*512 bytes)
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_WRITE
//    ERR_NOT_SUPPORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall WriteBlocksUncached(DISK *pDisk, DWORD dwBlock, 
											DWORD dwNumBlocks,
			   								unsigned char *ucBuf)
{
	DWORD dwBytesWritten, dwLow, dwHigh, dwError;
	__int64 iiFilepointer;
	int i;

	// check pointer
	if(NULL==pDisk) return ERR_NOT_OPEN;

	// check boundaries
	if(dwBlock+dwNumBlocks>pDisk->dwPhysicalBlocks) return ERR_OUT_OF_BOUNDS;

	// check media type
	if((TYPE_DISK==pDisk->iType)||(TYPE_FLOPPY==pDisk->iType)||
		((TYPE_FILE==pDisk->iType)&&(IMAGE_FILE_ISO==pDisk->iImageType)))
	{
		// set file pointer
		iiFilepointer = dwBlock; iiFilepointer *= 512;
		dwLow = iiFilepointer & 0xFFFFFFFF;
		dwHigh = (iiFilepointer >> 32) & 0xFFFFFFFF;
		if(0xFFFFFFFF==SetFilePointer(pDisk->hHandle, dwLow, &dwHigh, 
			FILE_BEGIN))
		{
			dwError = GetLastError();
			if(NO_ERROR != dwError)
			{
				LOG("WriteBlocksUncached(): ERR_SEEK\n"); LOG_ERR(dwError);
				return ERR_SEEK;
			}
		}
		
		// write to disk
		if(0==WriteFile(pDisk->hHandle, ucBuf, dwNumBlocks*512, 
			&dwBytesWritten, NULL))
		{
			LOG("WriteBlocksUncached(): WriteFile() failed.\n");
			return ERR_WRITE;
		}
		if(dwNumBlocks*512!=dwBytesWritten)
		{
			LOG("WriteBlocksUncached(): WriteFile() failed.\n");
			return ERR_WRITE;
		}
	}
	else if(TYPE_FILE==pDisk->iType)
	{
		return ERR_NOT_SUPPORTED;
	}
	else if(TYPE_CDROM==pDisk->iType)
	{
		return ERR_NOT_SUPPORTED;
	}
	else
	{
		return ERR_NOT_SUPPORTED;
	}
	
	// update cache
	for(i=0; i<(int)dwNumBlocks; i++)
	{
		CacheInsertReadBlock(pDisk, i+dwBlock, ucBuf + i*512);
	}
	
	pDisk->dwReadCounter++;
	return ERR_OK;
}

//----------------------------------------------------------------------------
// WriteBlocks
// 
// Write multiple blocks to harddisk or image file (cached).
// 
// -> pDisk = pointer to initialized disk structure
//    dwBlock = first block to read
//    dwNumBlocks = number of blocks to read
//    ucBuf = pointer to destination buffer (dwNumBlocks*512 bytes)
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_WRITE
//    ERR_NOT_SUPPORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall WriteBlocks(DISK *pDisk, DWORD dwBlock, 
									DWORD dwNumBlocks,
			   						unsigned char *ucBuf)
{
	int iResult;
	DWORD i;

	// check pointer
	if(NULL==pDisk) return ERR_NOT_OPEN;

	// check boundaries
	if(dwBlock+dwNumBlocks>pDisk->dwPhysicalBlocks) return ERR_OUT_OF_BOUNDS;

	// check media type
	if((TYPE_DISK==pDisk->iType)||(TYPE_FLOPPY==pDisk->iType)||
		((TYPE_FILE==pDisk->iType)&&(IMAGE_FILE_ISO==pDisk->iImageType)))
	{
		// write all blocks to cache
		for(i=0; i<dwNumBlocks; i++)
		{
			iResult = CacheWriteBlock(pDisk, i+dwBlock, ucBuf + i*512);
			if(ERR_OK!=iResult) return iResult;
		}
	}
	else if(TYPE_FILE==pDisk->iType)
	{
		return ERR_NOT_SUPPORTED;
	}
	else if(TYPE_CDROM==pDisk->iType)
	{
		return ERR_NOT_SUPPORTED;
	}
	else
	{
		return ERR_NOT_SUPPORTED;
	}
	
	pDisk->dwReadCounter++;
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// MakeLegalName
// 
// Create a legal filename (legal DOS name) from Ensoniq name. Since the
// Ensoniq file system can also use characters like *, ?, \, /, >, <, :,
// it is necessary to convert them to '_' when using them as a DOS filename.
// 
// -> cName = pointer to name
// <- the file name is modified in place (*cName)
//----------------------------------------------------------------------------
void MakeLegalName(char *cName)
{
	int j;
	
	// replace illegal chars for windows compatibility
	for(j=0; j<(int)strlen(cName); j++)
	{
		switch(cName[j])
		{
			case '*':
			case '?':
			case '\\':
			case '/':
			case '|':
			case '>':
			case '<':
			case ':':
				cName[j] = '_';
				break;
				
			default:
				break;
		}
		if(cName[j]<32) cName[j] = '_';
	}

	// trim right spaces
	for(j=strlen(cName); j>=0; j--)
	{
		if(0==cName[j]) continue;
		else if(32==cName[j])
		{
			cName[j] = 0;
			continue;
		}
		else break;
	}
	
	// trim left spaces
	while(32==cName[0])
	{
		for(j=0; j<(int)strlen(cName); j++)
		{
			cName[j] = cName[j+1];
		}
	}
}

//----------------------------------------------------------------------------
// ReadBlock
// 
// Reads one block from CDROM, harddisk or image file
// Get this block out of the cache, if possible
// If not, read a minimum of READ_AHEAD blocks into cache
// 
// -> pDisk = pointer to initialized disk structure
//    dwBlock = block to read
//    ucBuf = pointer to destination buffer (512 bytes)
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//	  ERR_SEEK
//----------------------------------------------------------------------------
int ReadBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf)
{
	DWORD dwBytesRead = 0, dwFirstBlock, dwBlocksToRead, dwLow, dwHigh,
		dwError;
	unsigned char *ucTemp, *ucTempUnaligned;
	__int64 iiFilepointer;
	int i, iResult;

	// check pointer
	if(NULL==pDisk) return ERR_NOT_OPEN;

	// check boundaries
	if(dwBlock>=pDisk->dwPhysicalBlocks) return ERR_OUT_OF_BOUNDS;

	// try to read this block from cache
	if(ERR_OK==CacheReadBlock(pDisk, dwBlock, ucBuf)) return ERR_OK;

	// -> block not in cache, so read number of READ_AHEAD blocks into cache

	// buffer has to be aligned at a 2048 byte boundary minimum
	// this is necessary for successfully reading from CDROM devices
	ucTempUnaligned = malloc(READ_AHEAD*512+2048);
	if(NULL==ucTempUnaligned)
	{
		LOG("ReadFile(): ERR_MEM "
			"(error allocating read ahead sector buffer.\n");
		return ERR_MEM;
	}
	ucTemp = ucTempUnaligned;
	while(0!=((DWORD)ucTemp & (DWORD)2047)) ucTemp++;
	
	if((TYPE_CDROM==pDisk->iType)
	   ||(TYPE_DISK==pDisk->iType)
	   ||(TYPE_FLOPPY==pDisk->iType)
	   ||((TYPE_FILE==pDisk->iType)&&(IMAGE_FILE_ISO==pDisk->iImageType)))
	{
		// check device status
		if(INVALID_HANDLE_VALUE==pDisk->hHandle)
		{
			free(ucTempUnaligned);
			return ERR_NOT_OPEN;
		}

		dwBlocksToRead = READ_AHEAD;
		dwFirstBlock = dwBlock;
				
		// fall back to a 2048 byte boundary for CDROM access to work
		if(TYPE_CDROM==pDisk->iType) dwFirstBlock = (dwBlock&0xFFFFFFFC);

		// treat floppy differently (read complete track)
		if(TYPE_FLOPPY==pDisk->iType)
		{
			dwBlocksToRead = pDisk->DiskGeometry.Geometry.SectorsPerTrack;
			dwFirstBlock -= dwFirstBlock % dwBlocksToRead;
		}
		
		// check physical limits
		if((dwFirstBlock+dwBlocksToRead)>=pDisk->dwPhysicalBlocks)
		{
			dwBlocksToRead = pDisk->dwPhysicalBlocks - dwFirstBlock;
		}
		
		// set file pointer
		iiFilepointer = dwFirstBlock; iiFilepointer *= 512;
		dwLow = iiFilepointer & 0xFFFFFFFF;
		dwHigh = (iiFilepointer >> 32) & 0xFFFFFFFF;
		if(0xFFFFFFFF==SetFilePointer(pDisk->hHandle, dwLow, &dwHigh, 
			FILE_BEGIN))
		{
			dwError = GetLastError();
			if(NO_ERROR != dwError)
			{
				LOG("ReadBlock(): ERR_SEEK\n"); LOG_ERR(dwError);
				free(ucTempUnaligned);
				return ERR_SEEK;
			}
		}		
		
		// read from device
		iResult=ReadFile(pDisk->hHandle, ucTemp, dwBlocksToRead*512,
						 &dwBytesRead, 0);
		if((iResult==0)||((dwBlocksToRead*512)!=dwBytesRead))
		{
			LOG("ReadBlock(): ERR_READ\n");
			free(ucTempUnaligned);
			return ERR_READ;
		}
	}
	else if(TYPE_FILE==pDisk->iType)
	{
		dwFirstBlock = dwBlock;
		
		free(ucTempUnaligned);
		return ERR_NOT_SUPPORTED;
	}
	else
	{
		free(ucTempUnaligned);
		return ERR_NOT_SUPPORTED;
	}

	// add freshly read blocks to the cache
	for(i=0; i<(int)dwBlocksToRead; i++)
	{
		CacheInsertReadBlock(pDisk, dwFirstBlock+i, ucTemp + i*512);

		// is this the block we originally wanted to have?
		if((dwFirstBlock+i)==dwBlock)
		{
			// deliver it to the calling function
			memcpy(ucBuf, ucTemp+i*512, 512);
		}
	}

	free(ucTempUnaligned);
	pDisk->dwReadCounter++;
	
	return ERR_OK;
}

//---------------------------------------------------------------------------
// GetFATEntry
//
// Read the FAT entry corresponding to a given block
//
// -> pDisk = pointer to valid disk structure
//    dwBlock = block to lookup in the FAT
// <- FAT entry for dwBlock or -1 if failed to lookup
//    0 = free block
//    1 = EOF
//    2 = bad block
//---------------------------------------------------------------------------
DLLEXPORT int __stdcall GetFATEntry(DISK *pDisk, DWORD dwBlock)
{
	// check pointers
	if(NULL==pDisk) return -1;

	// error checking
	if(((int)dwBlock<0)||(dwBlock>=pDisk->dwBlocks)) return -1;
	
	// read FAT block if necessary
	if((dwBlock/170+5)!=pDisk->dwFATCacheBlock)
	{
		// read FAT block containing the requested FAT entry
		if(ERR_OK!=ReadBlock(pDisk, dwBlock/170 + 5, pDisk->ucFATCache))
		{
			LOG("GetFATEntry(): ReadBlock failed.\n");
			return -1;
		}
		pDisk->dwFATMiss++;
		pDisk->dwFATCacheBlock = dwBlock/170+5;
		
		if((pDisk->ucFATCache[510]!='F')||(pDisk->ucFATCache[511]!='B'))
		{
			LOG("Warning: Missing FAT block signature in block ");
			LOG_INT(dwBlock/170 + 5); LOG(".\n");
		}
	}
	else
	{
		pDisk->dwFATHit++;
	}
	
	// get entry from FAT block
	return (pDisk->ucFATCache[(dwBlock%170)*3+0]<<16) +
	  	   (pDisk->ucFATCache[(dwBlock%170)*3+1]<<8)  +
	  	   (pDisk->ucFATCache[(dwBlock%170)*3+2]);
}

//---------------------------------------------------------------------------
// AdjustFreeBlocks
//
// Read OS block, adjust the free blocks value, write back OS block
//
// -> pDisk = pointer to valid disk structure
//    iAdjust = amount how to change the free blocks value
//				positive values decrease the free space, negative values
//              increase it
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//	  ERR_SEEK
//    ERR_WRITE
//---------------------------------------------------------------------------
int AdjustFreeBlocks(DISK *pDisk, int iAdjust)
{
	unsigned char ucBuf[512];
	int iResult, iFreeBlocks;
	
	// check pointer
	if(NULL==pDisk) return ERR_NOT_OPEN;
	
	if(0==iAdjust) return ERR_OK;
	
	LOG("Adjusting free blocks ("); LOG_INT(iAdjust); LOG("): ");
	
	// read OS block
	iResult = ReadBlock(pDisk, 2, ucBuf);
	if(ERR_OK!=iResult)
	{
		LOG("read failed, code="); LOG_INT(iResult); LOG(".\n");
		return iResult;
	}
	
	// change free blocks value
	iFreeBlocks = (ucBuf[0]<<24)
			    + (ucBuf[1]<<16)
			    + (ucBuf[2]<<8)
			    + (ucBuf[3]);

	LOG("before="); LOG_INT(iFreeBlocks);
	iFreeBlocks -= iAdjust; pDisk->dwBlocksFree = iFreeBlocks;
	LOG(", after="); LOG_INT(iFreeBlocks);
	
	ucBuf[0] = (iFreeBlocks>>24) & 0xFF;
	ucBuf[1] = (iFreeBlocks>>16) & 0xFF;
	ucBuf[2] = (iFreeBlocks>>8)  & 0xFF;
	ucBuf[3] = (iFreeBlocks)     & 0xFF;
	
	// write back OS block
	iResult = WriteBlocks(pDisk, 2, 1, ucBuf);
	if(ERR_OK!=iResult)
	{
		LOG(", write failed, code="); LOG_INT(iResult); LOG(".\n");
		return iResult;
	}
	
	LOG(", OK.\n");
	return ERR_OK;
}

//---------------------------------------------------------------------------
// SetFATEntry
//
// Write the FAT entry corresponding to a given block and give back its old
// value.
// 
// If you call this function many times, call it with dwMode=FAT_CACHED. The
// last call must be dwMode=FAT_WRITE to flush the cache.
//
// -> pDisk = pointer to valid disk structure
//    dwBlock = block to lookup in the FAT
//    dwNewValue = new value of this entry
// <- -1:  error
//    >-1: previous FAT entry for dwBlock
//---------------------------------------------------------------------------
DLLEXPORT int __stdcall SetFATEntry(DISK *pDisk, DWORD dwBlock, 
									DWORD dwNewValue)
{
	int iPrevious, iResult;
	
	// check pointers
	if(NULL==pDisk) return -1;

	// error checking
	if(((int)dwBlock<0)||(dwBlock>=pDisk->dwBlocks))
	{
		LOG("SetFATEntry(): dwBlock="); LOG_INT(dwBlock); 
		LOG(" out of bounds.\n");
		return -1;
	}
	
	// get old entry from FAT block
	iPrevious = GetFATEntry(pDisk, dwBlock);
	if(-1==iPrevious)
	{
		LOG("SetFATEntry(): GetFATEntry() error.\n");
		return -1;
	}
	
	// set new value
	pDisk->ucFATCache[(dwBlock%170)*3+0] =
		(unsigned char)((dwNewValue>>16) & 0xFF);
	pDisk->ucFATCache[(dwBlock%170)*3+1] =
		(unsigned char)((dwNewValue>>8) & 0xFF);
	pDisk->ucFATCache[(dwBlock%170)*3+2] =
		(unsigned char)(dwNewValue & 0xFF);
	
	// write back FAT block
	iResult = WriteBlocks(pDisk, dwBlock/170 + 5, 1, pDisk->ucFATCache);
	if(ERR_OK!=iResult)
	{
		LOG("SetFATEntry(): WriteBlocks() error, code="); LOG_INT(iResult); 
		LOG(".\n");
		return -1;
	}
	
	return iPrevious;
}

//----------------------------------------------------------------------------
// ScanDevices
// 
// Scans for disk devices and adds them to global list if Ensoniq signature
// is found.
// 
// A new disk structure is allocated (linked list).
// -> dwAllowNonEnsoniqFilesystems = 1: also take disks with no Ensoniq
//                                      signature into list
//                                 = 0: only allow Ensoniq formatted media
// <- =0: error
//    >0: pointer to begin of linked list
//	      this list should be freed using FreeDiskList()
//----------------------------------------------------------------------------
DLLEXPORT DISK __stdcall *ScanDevices(DWORD dwAllowNonEnsoniqFilesystems)
{
	#define BUF_SIZE 65535
	#define ID_SIZE 4096

	char cBuf[BUF_SIZE], cLongName[260], cMsDosName[260], cText[1024];
	DISK *pDisk, *pDiskRoot = NULL, *pCurrentDisk = NULL;
	DWORD dwBytesRead, dwBytesReturned, dwError, fsl, fsh;
	unsigned char *ucBuf = NULL, *ucBufUnaligned = NULL;
	int i, iType, j, iIsEnsoniq = 0, iImageType = IMAGE_FILE_UNKNOWN,
		iDeviceCounter, iMaxDevices;
	INI_LINE *pLine, *pIniFile;
	HANDLE h = INVALID_HANDLE_VALUE;

	LOG("\n--------------------------------------------------------------"
		"-------------\n");
	LOG("QueryDosDevice():\n");
	memset(cBuf, 0, BUF_SIZE);
	if(!QueryDosDevice(NULL, cBuf, BUF_SIZE))
	{
		LOG("FAILED.\n");
		return 0;
	}

	CreateProgressDialog();
	
	// find end of list
	i=0; while((i<BUF_SIZE-2)&&(!((cBuf[i]==0)&&(cBuf[i+1]==0)))) i++;
	LOG("End of QueryDosDevice() buffer: "); LOG_INT(i); LOG("\n");
	i++;

	// open ini file, parse, add image file entries to QueryList
	LOG("Reading INI file: ");
	UpdateProgressDialog("Parsing image file list...", 0);

	if(ERR_OK!=ReadIniFile(g_DefaultParams.DefaultIniName, &pIniFile))
	{
		LOG("failed.\n");
	}
	else
	{
		LOG("OK.\nParsing: ");

		// try to find section header
		pLine = pIniFile;
		while(pLine)
		{	
			if(0==strncmp("[EnsoniqFS]", pLine->cLine, 11))
			{
				LOG("[EnsoniqFS] found.\n");
				break;
			}
			pLine = pLine->pNext;
		}
	
		// section not found?
		if(NULL==pLine)
		{
			LOG("[EnsoniqFS] section not found.\n");
		}
		else
		{
			// skip [EnsoniqFS] section name
			pLine = pLine->pNext;

			// parse all lines after [EnsoniqFS]
			while(pLine)
			{
				// next section found?
				if('['==pLine->cLine[0]) break;
				
				if(0==strncmp("image=", pLine->cLine, 6))
				{
					LOG("adding to list: "); LOG(pLine->cLine);
					
					// add image file name to string list
					j=0;
					while(pLine->cLine[j]>31) cBuf[i++] = pLine->cLine[j++];
					cBuf[i++] = 0;
				}
				
				pLine = pLine->pNext;
			}
		}
	}
	FreeIniLines(pIniFile);

	// add final '\0' to end of string list
	cBuf[i] = 0;

	// count devices (for proper progess reporting)
	i = 0; iMaxDevices = 0;
	while(cBuf[i])
	{
		i += strlen(cBuf + i) + 1;
		iMaxDevices++;
	}
	
	i = 0; iDeviceCounter = 0;
	while(cBuf[i])
	{
		iDeviceCounter++;

		// construct device name			
		strcpy(cMsDosName, "\\\\.\\"); strcat(cMsDosName, cBuf + i);

		if(0==strncmp(cMsDosName, "\\\\.\\image=", 10))
		{
			strcpy(cLongName, "image file");
		}
		else
		{
			// query long name
			QueryDosDevice(cBuf + i, cLongName, BUF_SIZE);
		}

		i += strlen(cBuf + i) + 1;
		
		// check only physical drives, image files, floppies and CDROMs
		if(0==strncmp(cMsDosName, "\\\\.\\PhysicalDrive", 17))
		{
			if(!g_iOptionEnablePhysicalDisks) continue;
			iType = TYPE_DISK;
		}
		else if(0==strncmp(cMsDosName, "\\\\.\\CdRom", 9))
		{
			if(!g_iOptionEnableCDROM) continue;
			iType = TYPE_CDROM;
		}
		else if(0==strncmp(cMsDosName, "\\\\.\\A:", 6))
		{
			if(!g_iOptionEnableFloppy) continue;
			iType = TYPE_FLOPPY;
		}
		else if(0==strncmp(cMsDosName, "\\\\.\\B:", 6))
		{
			if(!g_iOptionEnableFloppy) continue;
			iType = TYPE_FLOPPY;
		}
		else if(0==strncmp(cMsDosName, "\\\\.\\image=", 10))
		{
			if(!g_iOptionEnableImages) continue;
			iType = TYPE_FILE;
			for(j=10; j<(int)strlen(cMsDosName); j++) 
				if('\\'==cMsDosName[j]) cMsDosName[j] = '/';
		}
		else
		{
			continue;
		}

		// update progress dialog
		strcpy(cText, "Mounting devices:\n\n");
		strcat(cText, cMsDosName);
		UpdateProgressDialog(cText, iDeviceCounter*100/iMaxDevices);

		// buffer has to be aligned at a 2048 byte boundary minimum
		// this is necessary for successfully reading from CDROM devices
		ucBufUnaligned = malloc(ID_SIZE+2048);
		if(NULL==ucBufUnaligned)
		{
			dwError = GetLastError();
			LOG("Unable to allocate sector buffer: "); LOG_ERR(dwError); 
			continue;
		}
		ucBuf = ucBufUnaligned;
		while(0!=((DWORD)ucBuf & (DWORD)2047)) ucBuf++;

		LOG("\n"); LOG(cMsDosName); LOG(" = ");
		LOG(cLongName); LOG("\n  Opening: ");

		// if floppy, try to enable 80/2/10x512 and 80/2/20x512 format
		if(TYPE_FLOPPY==iType)
		{
			if(FALSE==EnableExtendedFormats(cMsDosName, TRUE))
			{
				free(ucBufUnaligned);
				continue;
			}

			h = CreateFile(cMsDosName, FILE_ALL_ACCESS, 0,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, 
							NULL, 0, &dwBytesReturned, NULL);
		}
		else if(TYPE_FILE==iType)
		{
			h = CreateFile(cMsDosName+10, FILE_ALL_ACCESS,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
				OPEN_EXISTING, 0, NULL);
		}
		else
		{
			// open other devices
			h = CreateFile(cMsDosName, FILE_ALL_ACCESS,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
				OPEN_EXISTING, 0, NULL);
		}
		
		// check result
		if(INVALID_HANDLE_VALUE==h)
		{
			dwError = GetLastError();
			LOG("failed: "); LOG_ERR(dwError);
			free(ucBufUnaligned);
			continue;
		}

		LOG("OK.\n  Reading block 0-7: ");

		// read first 8 sectors
		if(0==ReadFile(h, ucBuf, ID_SIZE, &dwBytesRead, NULL))
		{
			dwError = GetLastError();
			LOG("failed: "); LOG_ERR(dwError);
			if(TYPE_FLOPPY==iType)
			{
				DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
								&dwBytesReturned, NULL);
				CloseHandle(h);
				EnableExtendedFormats(cMsDosName, FALSE);
			}
			else CloseHandle(h);
			free(ucBufUnaligned);
			continue;
		}

		if(TYPE_FILE==iType)
		{
			iImageType = IMAGE_FILE_UNKNOWN;
			/*	
			// GKH?
			if(IMAGE_FILE_UNKNOWN==iImageType)
			{
				if(0==strncmp(ucBuf, "TDDFI", 5))	// GKH
				{
					LOG("File identified as GKH.\n");
					iImageType = IMAGE_FILE_GKH;
	
					// TODO
					// re-read first sectors so they appear correctly in ucBuf
				}
			}
			
			
			// Giebler?
			if(IMAGE_FILE_UNKNOWN==iImageType)
			{
				if((ucBuf[0x00]==0x0D) &&
				   (ucBuf[0x01]==0x0A) &&
				   (ucBuf[0x4E]==0x0D) &&
				   (ucBuf[0x4F]==0x0A))
				{
					switch(ucBuf[0x1FF])
					{
						case 0x03:    // EDE DD
						case 0x07:    // EDT DD
							iMapOffset = 0xA0;
						break;
						
						case 0xCB:    // EDA HD
						case 0xCC:    // EDT HD
							iMapOffset = 0x60;
						break;
						
						default:
							iMapOffset = 0x00;
					}
				
					if((ucBuf[iMapOffset-3]==0x0D) &&
					   (ucBuf[iMapOffset-2]==0x0A) &&
					   (ucBuf[iMapOffset-1]==0x1A))
					{
						LOG("File identified as Giebler image.\n");
						iImageType = IMAGE_FILE_GIEBLER;
	
						// TODO
						// re-read first sectors so they appear correctly in ucBuf
					}
				}
			}
			*/
			
			// Mode1-CDROM?
			if(IMAGE_FILE_UNKNOWN==iImageType)
			{
				if((0x00==ucBuf[ 0])&&
				   (0xFF==ucBuf[ 1])&&
				   (0xFF==ucBuf[ 2])&&
				   (0xFF==ucBuf[ 3])&&
				   (0xFF==ucBuf[ 4])&&
				   (0xFF==ucBuf[ 5])&&
				   (0xFF==ucBuf[ 6])&&
				   (0xFF==ucBuf[ 7])&&
				   (0xFF==ucBuf[ 8])&&
				   (0xFF==ucBuf[ 9])&&
				   (0xFF==ucBuf[10])&&
				   (0x00==ucBuf[11]))
				{
					if(('I'==ucBuf[ 38+512*1+16])&&('D'==ucBuf[ 39+512*1+16])&&
					   ('O'==ucBuf[ 28+512*2+16])&&('S'==ucBuf[ 29+512*2+16])&&
					   ('D'==ucBuf[510+2352+16])&&('R'==ucBuf[511+2352+16])&&
					   ('F'==ucBuf[510+2352+512*1+16])&&
					   ('B'==ucBuf[511+2352+512*1+16]))
					{		
						LOG("File identified as Mode1CD.\n");
						iImageType = IMAGE_FILE_MODE1;
						iIsEnsoniq = 1;
						
						// re-read first sectors so they appear correctly 
						// in ucBuf
						LOG("Re-reading header: ");
						for(j=0; j<8; j++)
						{
							SetFilePointer(h, (j>>2)*2352+16+((j&0x03)*512), 
								0, FILE_BEGIN);
							DWORD dw;
							ReadFile(h, ucBuf+j*512, 512, &dw, 0);
						}
						LOG("OK.\n");
					}
				}
			}

			// ISO?
			if(IMAGE_FILE_UNKNOWN==iImageType)
			{
				if(('I'==ucBuf[ 38+512*1])&&('D'==ucBuf[ 39+512*1])&&
				   ('O'==ucBuf[ 28+512*2])&&('S'==ucBuf[ 29+512*2])&&
				   ('D'==ucBuf[510+512*4])&&('R'==ucBuf[511+512*4])&&
				   ('F'==ucBuf[510+512*5])&&('B'==ucBuf[511+512*5]))
				{
					LOG("File identified as ISO image.\n");
					iImageType = IMAGE_FILE_ISO;
					iIsEnsoniq = 1;
					
					// re-reading not necessary, ISO is a plain image
				}
			}
			
			if(IMAGE_FILE_UNKNOWN==iImageType)
			{
				LOG("Warning: Unknown image file type. Skipping.\n");
				CloseHandle(h); free(ucBufUnaligned);
				continue;
			}
		}
		else	// not an image file
		{
			LOG("OK.\n  Checking Ensoniq signature: ");
			if(('I'!=ucBuf[ 38+512*1])||('D'!=ucBuf[ 39+512*1])||
			   ('O'!=ucBuf[ 28+512*2])||('S'!=ucBuf[ 29+512*2])||
			   ('D'!=ucBuf[510+512*4])||('R'!=ucBuf[511+512*4]))
			{
				LOG("not found, ");
	
				// leave out this disk if only scanning for Ensoniq disks
				if(0==dwAllowNonEnsoniqFilesystems)
				{
					LOG("skipping.\n");
					if(TYPE_FLOPPY==iType)
					{
						DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
										&dwBytesReturned, NULL);
						CloseHandle(h);
						EnableExtendedFormats(cMsDosName, FALSE);
					}
					else CloseHandle(h);
					free(ucBufUnaligned);
					continue;
				}
				else
				{
					LOG("ignoring (WARNING: RISK OF DATA LOSS!), ");
					iIsEnsoniq = 0;
				}
			}
			else
			{
				LOG("OK, ");
				iIsEnsoniq = 1;
			}
		}
		
		// create new disk structure
		pDisk = malloc(sizeof(DISK));
		if(NULL==pDisk)
		{
			LOG("Unable to allocate new DISK structure.\n");
			if(TYPE_FLOPPY==iType)
			{
				DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
								&dwBytesReturned, NULL);
				CloseHandle(h);
				EnableExtendedFormats(cMsDosName, FALSE);
			}
			else CloseHandle(h);
			if(TYPE_FLOPPY==iType) EnableExtendedFormats(cMsDosName, FALSE);
			free(ucBufUnaligned);
			continue;
		}

		// initialize disk
		memset(pDisk, 0, sizeof(DISK));
		pDisk->hHandle = h;
		strcpy(pDisk->cMsDosName, cMsDosName);
		strcpy(pDisk->cLongName, cLongName);
		pDisk->iType = iType;
		pDisk->iImageType = iImageType;
		pDisk->iIsEnsoniq = iIsEnsoniq;
		
		// read disk geometry
		// treat floppy and image files different (see below)
		if((TYPE_FLOPPY!=iType)&&(TYPE_FILE!=iType))
		{
			if(0==DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
								  NULL, 0, &(pDisk->DiskGeometry),
								  sizeof(DISK_GEOMETRY_EX),
								  &dwBytesReturned, NULL))
			{
				LOG("DeviceIoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY_EX) "
					"failed.\n");
				CloseHandle(h);
				free(ucBufUnaligned);
				continue;
			}
		}
		
		// allocate cache
		pDisk->ucCache = malloc(CACHE_SIZE*512);
		if(NULL==pDisk->ucCache)
		{
			LOG("Unable to allocate cache memory.\n");
			if(TYPE_FLOPPY==iType)
			{
				DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
								&dwBytesReturned, NULL);
				CloseHandle(h);
				EnableExtendedFormats(cMsDosName, FALSE);
			}
			else CloseHandle(h);
			free(ucBufUnaligned);
			free(pDisk);
			continue;
		}
		pDisk->dwCacheTable = malloc(CACHE_SIZE*sizeof(DWORD));
		if(NULL==pDisk->dwCacheTable)
		{
			LOG("Unable to allocate cache table memory.\n");
			if(TYPE_FLOPPY==iType)
			{
				DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
								&dwBytesReturned, NULL);
				CloseHandle(h);
				EnableExtendedFormats(cMsDosName, FALSE);
			}
			else CloseHandle(h);
			free(ucBufUnaligned);
			free(pDisk->ucCache);
			free(pDisk);
			continue;
		}
		pDisk->dwCacheAge = malloc(CACHE_SIZE*sizeof(DWORD));
		if(NULL==pDisk->dwCacheAge)
		{
			LOG("Unable to allocate cache age memory.\n");
			if(TYPE_FLOPPY==iType)
			{
				DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
								&dwBytesReturned, NULL);
				CloseHandle(h);
				EnableExtendedFormats(cMsDosName, FALSE);
			}
			else CloseHandle(h);
			free(ucBufUnaligned);
			free(pDisk->ucCache);
			free(pDisk->dwCacheTable);
			free(pDisk);
			continue;
		}
		pDisk->ucCacheFlags = malloc(CACHE_SIZE);
		if(NULL==pDisk->ucCacheFlags)
		{
			LOG("Unable to allocate cache flags memory.\n");
			if(TYPE_FLOPPY==iType)
			{
				DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0,
								&dwBytesReturned, NULL);
				CloseHandle(h);
				EnableExtendedFormats(cMsDosName, FALSE);
			}
			else CloseHandle(h);
			free(ucBufUnaligned);
			free(pDisk->ucCache);
			free(pDisk->dwCacheAge);
			free(pDisk->dwCacheTable);
			free(pDisk);
			continue;
		}
		memset(pDisk->dwCacheTable, 0xFF, CACHE_SIZE*sizeof(DWORD));
		memset(pDisk->dwCacheAge, 0x00, CACHE_SIZE*sizeof(DWORD));
		memset(pDisk->ucCacheFlags, 0x00, CACHE_SIZE);
		
		// copy disk name
		for(j=0; j<7; j++)
		{
			pDisk->cDiskLabel[j] = ucBuf[j+31+512];
			pDisk->cLegalDiskLabel[j] = ucBuf[j+31+512];
		}
		MakeLegalName(pDisk->cLegalDiskLabel);
		
		// read number of blocks
		pDisk->dwBlocks = ucBuf[17+512] + (ucBuf[16+512]<<8) + 
						  (ucBuf[15+512]<<16) + (ucBuf[14+512]<<24);

		// set physical disk values to logical values from DeviceID block
		if((TYPE_FLOPPY==iType)||(TYPE_FILE==iType))
		{
			__int64 ii = 0;
			if(TYPE_FLOPPY==iType)
			{
				ii = pDisk->dwBlocks; ii *= 512;
			}
			else if(TYPE_FILE==iType)
			{
				// get image file size
				fsl = GetFileSize(h, &fsh); dwError = GetLastError();
				if((0xFFFFFFFF==fsl)&&(NO_ERROR!=dwError))
				{
					LOG("Error reading file size.\n");
					LOG_ERR(dwError);
					free(ucBufUnaligned);
					free(pDisk->ucCache);
					free(pDisk->dwCacheAge);
					free(pDisk->dwCacheTable);
					free(pDisk->ucCacheFlags);
					free(pDisk);
					continue;
				}
				ii = fsh; ii <<= 32; ii += (__int64)fsl;
			}

			pDisk->DiskGeometry.DiskSize.LowPart = ii & 0xFFFFFFFF;;
			pDisk->DiskGeometry.DiskSize.HighPart = ii >> 32;
			pDisk->DiskGeometry.Geometry.BytesPerSector = 
				ucBuf[13+512] + (ucBuf[12+512]<<8) + 
				(ucBuf[11+512]<<16) + (ucBuf[10+512]<<24);
			pDisk->DiskGeometry.Geometry.Cylinders.LowPart =
				ucBuf[9+512] + (ucBuf[8+512]<<8);
			pDisk->DiskGeometry.Geometry.Cylinders.HighPart = 0;
			pDisk->DiskGeometry.Geometry.SectorsPerTrack =
				ucBuf[5+512] + (ucBuf[4+512]<<8);
			pDisk->DiskGeometry.Geometry.TracksPerCylinder =
				ucBuf[7+512] + (ucBuf[6+512]<<8);
			pDisk->DiskGeometry.Geometry.MediaType = 0;
		}
		
		pDisk->dwPhysicalBlocks = 
			(int)(pDisk->DiskGeometry.DiskSize.QuadPart/512);
		pDisk->dwBlocksFree = (ucBuf[0+512*2]<<24)
							+ (ucBuf[1+512*2]<<16)
							+ (ucBuf[2+512*2]<<8)
							+ (ucBuf[3+512*2]);

		pDisk->dwFATCacheBlock = 0xFFFFFFFF;


		LOG_INT(pDisk->dwBlocks); LOG(" blocks (logical), ");
		LOG_INT(pDisk->dwPhysicalBlocks);
		LOG(" blocks (physical), "); LOG_INT(pDisk->dwBlocksFree);
		LOG(" blocks free.\n");
	
		// append newly created disk structure to the list
		if(0==pDiskRoot)
		{
			pDiskRoot = pDisk;
		}
		else
		{
			pCurrentDisk->pNext = pDisk;
		}
		pCurrentDisk = pDisk;

		free(ucBufUnaligned);

		// do not close handle here - will be used later
	}
	LOG("\n");
	
	DestroyProgressDialog();
	
	return pDiskRoot;
}

//----------------------------------------------------------------------------
// FreeDiskList
// 
// Frees a linked list of disks from memory, closes all associated devices
// and files
// 
// -> iShowProgress != 0: Show progress dialog
// <- --
//----------------------------------------------------------------------------
DLLEXPORT void __stdcall FreeDiskList(int iShowProgress, DISK *pRoot)
{
	int iMaxDevices, iDeviceCounter;
	DWORD dwBytesReturned;
	DISK *pTemp, *pDisk;
	char cText[1024];

	pDisk = pRoot;
	if(!pDisk) return;

	if(iShowProgress) CreateProgressDialog();
	
	// count devices for proper progress reporting
	iMaxDevices = 0;
	while(pDisk)
	{
		pDisk = pDisk->pNext;
		iMaxDevices++;
	}
	
	pDisk = pRoot;
	iDeviceCounter = 0;

	while(pDisk)
	{
		if(iShowProgress)
		{
			// update progress dialog
			iDeviceCounter++;
			strcpy(cText, "Unmounting devices:\n\n");
			strcat(cText, pDisk->cMsDosName);
			UpdateProgressDialog(cText, iDeviceCounter*100/iMaxDevices);
		}
		LOG("FreeDiskList(): \"");
		LOG(pDisk->cMsDosName); LOG("\"\n");

		// flush the cache before deleting it
		CacheFlush(pDisk);

		if(pDisk->dwCacheTable) free(pDisk->dwCacheTable);
		if(pDisk->ucCacheFlags) free(pDisk->ucCacheFlags);
		if(pDisk->dwCacheAge) free(pDisk->dwCacheAge);
		if(pDisk->ucCache) free(pDisk->ucCache);
		if(pDisk->hHandle!=INVALID_HANDLE_VALUE)
		{
			if(TYPE_FLOPPY==pDisk->iType)
			{
				DeviceIoControl(pDisk->hHandle, FSCTL_UNLOCK_VOLUME, NULL, 0,
								NULL, 0, &dwBytesReturned, NULL);
				CloseHandle(pDisk->hHandle);
				EnableExtendedFormats(pDisk->cMsDosName, FALSE);
			}
			else CloseHandle(pDisk->hHandle);

		}
		pTemp = pDisk->pNext;
		free(pDisk);
		pDisk = pTemp;
	}
	
	g_pDiskListRoot = 0;

	if(iShowProgress) DestroyProgressDialog();
}

//----------------------------------------------------------------------------
// EnableExtendedFormats
// 
// Enable/disable special floppy disk format
// 
// -> szDrive = name of drive (e. g. "\\\\.\\A:")
//    bEnable = TRUE:  enable
//              FALSE: disable
// <- =0: Error
//    >0: OK
//----------------------------------------------------------------------------
BOOL EnableExtendedFormats(const char *szDrive, BOOL bEnable)
{

// TODO: report failure to load to user (either OmniFlop not installed or
//       no license available)
//       Allow user to avoid this warning in the future

	DWORD nBytesReturned, dwError;
	BOOL status;
	
	LOG("EnableExtendedFormats(\""); LOG((char*)szDrive); LOG("\", ");
	LOG_INT(bEnable); LOG("): ");

	// We need to enable the Extended formats without prompting the driver
	// to test the media (first) - so we have to open it with Query access
	// only before opening it for 'read' or write seperately.
	HANDLE hMedia = CreateFile(
		szDrive,
		0, /* NO SHARING */
		0, /* QUERY ACCESS ONLY */
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		0 /* No template file */ );

	if(INVALID_HANDLE_VALUE==hMedia)
	{
		dwError = GetLastError();
		LOG("CreateFile() failed: "); LOG_ERR(dwError);
		return FALSE;
	}
	
	status = !! DeviceIoControl(
		hMedia,
		bEnable ?
		IOCTL_OMNIFLOP_ENABLE_EXTENDED_FORMATS :
		IOCTL_OMNIFLOP_DISABLE_EXTENDED_FORMATS,
		bEnable ? "EFS" : NULL,
		bEnable ? 4 : 0, /* Formats to enable/disable */
		NULL, 0,
		&nBytesReturned,
		NULL);

	// Returns status == 0 and GetLastError == 0x00000005 (ERROR_ACCESS_DENIED)
	// if not registered or invalid code
	if(!status)
	{
		dwError = GetLastError();
		LOG("DeviceIoControl() failed: "); LOG_ERR(dwError);
		CloseHandle(hMedia);
		return FALSE;
	}

	LOG("OK.\n");
	CloseHandle(hMedia);
	return status;
}
