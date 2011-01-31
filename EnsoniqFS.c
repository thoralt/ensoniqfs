//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// MAIN MODULE
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
// rea
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
// MA  02110-1301, USA.
// 
// Alternatively, download a copy of the license here:
// http://www.gnu.org/licenses/gpl.txt
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// includes
//----------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>
#include <ctype.h>
#include "log.h"
#include "fsplugin.h"
#include "disk.h"
#include "ini.h"
#include "EnsoniqFS.h"
#include "error.h"
#include "cache.h"
#include "optionsdlg.h"
#include "choosediskdlg.h"

//----------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------

// global root of device list
DISK *g_pDiskListRoot = NULL;

// global root find handle list (linked list with all open find handles,
// multiple open find lists for Total Commander are possible, yet unlikely)
FIND_HANDLE *g_pHandleRoot = NULL;

// variables to receive initialization parameters from Total Commander
FsDefaultParamStruct g_DefaultParams;
int g_iPluginNr = 0;
tProgressProc g_pProgressProc;
tLogProc g_pLogProc;
tRequestProc g_pRequestProc;
HINSTANCE g_hInst = 0;

// global options
int g_iOptionEnableFloppy = 1;
int g_iOptionEnableCDROM = 1;
int g_iOptionEnableImages = 1;
int g_iOptionEnablePhysicalDisks = 1;
int g_iOptionAutomaticRescan = 1;
int g_iOptionEnableLogging = 1;
char g_cOptionInstallPath[261];

// flag for operations on multiple files to flush the cache only once after
// the last file
unsigned char g_ucMultiple = 0;

// synchronization between multiple instances of EnsoniqFS
const char g_cMemoryFN[] = "8EEA3415-4462-4f26-87BE-58ED9B9E5D86-EnsoniqFS";
#define SHARED_MEMORY_SIZE	4096
unsigned char *g_ucSharedMemory = NULL;
HANDLE g_hMemoryMappedFile = INVALID_HANDLE_VALUE;

//----------------------------------------------------------------------------
// upcase
// 
// Converts a path to uppercase, keeping the case of the first two path
// elements (group name and disk name)
// 
// -> c = pointer to string to convert
// <- --
//----------------------------------------------------------------------------
void upcase(char *c)
{
	int i, j=0, k=0;
	
	if(NULL==c) return;
	while((k<3)&&(j<(int)strlen(c))) if('\\'==c[j++]) k++;
	for(i=j; i<(int)strlen(c); i++) c[i] = toupper(c[i]);
}

int FixDirectoryEntries(DISK *pDisk, ENSONIQDIR *pDir)
{
	int i, iResult;
	DWORD dwBlock;
	
	LOG("Fixing directory entries\n");
		
	for(i=0; i<39; i++)
	{
		if(FILE_TYPE_EMPTY==pDir->ucDirectory[i*26+1]) continue; // skip blank entries
		if(FILE_TYPE_PARENT_DIRECTORY==pDir->ucDirectory[i*26+1]) continue; // skip link to parent
		if(FILE_TYPE_DIRECTORY==pDir->ucDirectory[i*26+1]) continue; // skip subdirectories
		
		// calculate real length according to FAT
	    pDir->Entry[i].dwLen = 0;
		dwBlock = pDir->Entry[i].dwStart;
		while(dwBlock>2)
		{
            pDir->Entry[i].dwLen++;
			dwBlock = GetFATEntry(pDisk, dwBlock);    
		}
		
		pDir->ucDirectory[i*26 + 14] = pDir->Entry[i].dwLen >> 8; 
		pDir->ucDirectory[i*26 + 15] = pDir->Entry[i].dwLen & 0xFF;

		LOG("%s: LEN = 0x%02X 0x%02X START = 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                 pDir->Entry[i].cName,
                 pDir->ucDirectory[i*26 + 14],
		         pDir->ucDirectory[i*26 + 15],
                 pDir->ucDirectory[i*26 + 18],
                 pDir->ucDirectory[i*26 + 19],
                 pDir->ucDirectory[i*26 + 20],
                 pDir->ucDirectory[i*26 + 21]); 
                 
		iResult = WriteBlocks(pDisk, pDir->dwDirectoryBlock, 2, pDir->ucDirectory);
		if(ERR_OK!=iResult)
		{
			LOG("Error writing Ensoniq file, code=%d\n", iResult);
			return ERR_WRITE;
		}
		iResult = CacheFlush(pDisk);
		if(ERR_OK!=iResult)
		{
			LOG("Error flushing cache, code=%d\n", iResult);
			return ERR_WRITE;
		}
	}
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// ReadDirectory
// 
// Reads the directory starting with dwBlock from pDisk
// 
// -> pDisk: pointer to valid disk structure
//    dwBlock: starting block
//    pDir: ENSONIQDIR structure to receive directory listing
//	  ucShowWarning: Show warning about bad file sizes
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
int ReadDirectory(DISK *pDisk, ENSONIQDIR *pDir, unsigned char ucShowWarning)
{
 	char cWarningMsg[2048] = "Some files in this directory have invalid "
		 "file sizes according to their directory\nentries. "
		 "You should consider using ETools to scan and/or repair "
		 "this disc.\n\n"
		 "The following files have differing values for directory entry "
		 "and size\ncalculation from FAT:\n\n";
    char cWarning[256];
    unsigned char ucWarningFlag = 0;
	int iResult, i, j, iIndex;
	DWORD dwBlock, dwLen, dwStart, dwContiguous;

	LOG("ReadDirectory(block=%d)\n", pDir->dwDirectoryBlock);

	// initialize ENSONIQDIR, but keep dwDirectoryBlock
	dwBlock = pDir->dwDirectoryBlock;
	memset(pDir, 0, sizeof(ENSONIQDIR));
	pDir->dwDirectoryBlock = dwBlock;

	// read directory blocks from disk
	iResult = ReadBlocks(pDisk, dwBlock, 2, pDir->ucDirectory);
	if(ERR_OK!=iResult) return iResult;

	// loop through all entries
//	LOG("\n");
	for(i=0; i<39; i++)
	{
		pDir->Entry[i].ucType = pDir->ucDirectory[i*26+1];
		memset(&(pDir->VirtualWaveEntry[i]), 0, sizeof(VIRTUALWAVEFILE));
		
/*
		// debug output
		LOG("Directory entry #"); LOG_HEX2(i); LOG(": ");
		for(j=0; j<26; j++) 
		{
			LOG_HEX2(pDir->ucDirectory[i*26+j]);
			LOG(" ");
		}
		LOG("\n");
*/		
		// check type
		if(0x00==pDir->ucDirectory[i*26+1]) continue; // skip blank entries
		if(0x08==pDir->ucDirectory[i*26+1]) continue; // skip link to parent

		// copy name
		strncpy(pDir->Entry[i].cName, pDir->ucDirectory+i*26+2, 12);
		while(strlen(pDir->Entry[i].cName)<12)
			strcat(pDir->Entry[i].cName, " ");
		pDir->Entry[i].cName[12] = 0;
		
		strncpy(pDir->Entry[i].cLegalName, pDir->ucDirectory+i*26+2, 12);
		MakeLegalName(pDir->Entry[i].cLegalName);

		// calculate file properties
		pDir->Entry[i].dwLen        = (pDir->ucDirectory[i*26 + 14]<<8)
									+  pDir->ucDirectory[i*26 + 15];
		pDir->Entry[i].dwContiguous = (pDir->ucDirectory[i*26 + 16]<<8)
									+  pDir->ucDirectory[i*26 + 17];
		if(0==pDir->Entry[i].dwLen)
		{
			pDir->Entry[i].dwLen = pDir->Entry[i].dwContiguous;
		}
		pDir->Entry[i].dwStart      = (pDir->ucDirectory[i*26 + 18]<<24)
									+ (pDir->ucDirectory[i*26 + 19]<<16)
									+ (pDir->ucDirectory[i*26 + 20]<<8)  
									+  pDir->ucDirectory[i*26 + 21];
		pDir->Entry[i].ucMultiFileIndex = pDir->ucDirectory[i*26 + 22];
		// calculate real length according to FAT
	    dwLen = 0;
		dwBlock = pDir->Entry[i].dwStart;
		while(dwBlock>2)
		{
            dwLen++;
			dwBlock = GetFATEntry(pDisk, dwBlock);    
		}

/*		LOG("%s: LEN = 0x%02X 0x%02X START = 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                 pDir->Entry[i].cName,
                 pDir->ucDirectory[i*26 + 14],
		         pDir->ucDirectory[i*26 + 15],
                 pDir->ucDirectory[i*26 + 18],
                 pDir->ucDirectory[i*26 + 19],
                 pDir->ucDirectory[i*26 + 20],
                 pDir->ucDirectory[i*26 + 21]);
*/                 
		if((dwLen!=pDir->Entry[i].dwLen)&&(pDir->Entry[i].ucType!=FILE_TYPE_DIRECTORY))
		{
  		    LOG("Warning: for file '%s' length FAT (%d) != length directory entry (%d)\n",
			    pDir->Entry[i].cName, dwLen, pDir->Entry[i].dwLen);
			    
		    sprintf(cWarning, "%s: %d blocks (dir) / %d blocks (FAT), %d blocks contiguous\n",
		        pDir->Entry[i].cName, (int)pDir->Entry[i].dwLen, (int)dwLen, 
				(int)pDir->Entry[i].dwContiguous);
		    strcat(cWarningMsg, cWarning);
		    ucWarningFlag = 1;
		}	 							   
	}
	
	if(ucWarningFlag&&ucShowWarning)
	{
		strcat(cWarningMsg, "\n\nShould this problem be corrected?");
   		if(IDYES == MessageBoxA(0, cWarningMsg, "EnsoniqFS � Warning", 
		   MB_ICONWARNING | MB_YESNO))
		{
			if(ERR_OK!=FixDirectoryEntries(pDisk, pDir))
			{
		   		MessageBoxA(0, "Could not write directory.", "EnsoniqFS � Error", 
				   MB_ICONSTOP);
			}
		}
 	}
	
	// loop again through all entries to find ASR audio tracks and
	// assign virtual wave files to them
	iIndex = 0;
	for(i=0; i<39; i++)
	{
		// skip all non-audio-track-files		
		if(pDir->Entry[i].ucType != FILE_TYPE_ASR_AUDIOTRACK) continue;
		
		// this is a new audio track
		strcpy(pDir->VirtualWaveEntry[iIndex].cName, pDir->Entry[i].cName);
		strcpy(pDir->VirtualWaveEntry[iIndex].cLegalName, pDir->Entry[i].cLegalName);
		dwLen = pDir->Entry[i].dwLen;
		dwStart = pDir->Entry[i].dwStart;
		dwContiguous = pDir->Entry[i].dwContiguous;
		pDir->VirtualWaveEntry[iIndex].dwLen1 = dwLen;
		pDir->VirtualWaveEntry[iIndex].dwStart1 = dwStart;
		pDir->VirtualWaveEntry[iIndex].dwContiguous1 = dwContiguous;
		pDir->VirtualWaveEntry[iIndex].ucIsStereo = 0;
		
		LOG("audio track '%s', len=%d\n", pDir->VirtualWaveEntry[iIndex].cLegalName, dwLen);
		
		// search for a matching file among the already scanned files
		// to form a stereo pair
		for(j=iIndex-1; j>=0; j--)
		{
//			LOG("comparing audio track '%s', len=%d to '%s, len=%d\n", 
//				pDir->VirtualWaveEntry[iIndex].cLegalName, dwLen,
//				pDir->VirtualWaveEntry[j].cLegalName, pDir->VirtualWaveEntry[j].dwLen1);

			if(dwLen==pDir->VirtualWaveEntry[j].dwLen1)
			{				
				iIndex++;	// store stereo information in next record
				pDir->VirtualWaveEntry[iIndex].ucIsStereo = 1;
				
				pDir->VirtualWaveEntry[iIndex].dwLen2 = 
					pDir->VirtualWaveEntry[j].dwLen1;
				pDir->VirtualWaveEntry[iIndex].dwStart2 = 
					pDir->VirtualWaveEntry[j].dwStart1;
				pDir->VirtualWaveEntry[iIndex].dwContiguous2 = 
					pDir->VirtualWaveEntry[j].dwContiguous1;
					
				pDir->VirtualWaveEntry[iIndex].dwLen1 = dwLen;
				pDir->VirtualWaveEntry[iIndex].dwStart1 = dwStart;
				pDir->VirtualWaveEntry[iIndex].dwContiguous1 = dwContiguous;
					
				strcpy(pDir->VirtualWaveEntry[iIndex].cName, 
					pDir->VirtualWaveEntry[j].cName);
				strcpy(pDir->VirtualWaveEntry[iIndex].cLegalName, 
					pDir->VirtualWaveEntry[j].cLegalName);
				LOG("Stereo pair detected: name='%s', len1=%d, len2=%d, start1=%d, start2=%d.\n",
					pDir->VirtualWaveEntry[iIndex].cLegalName, 
					pDir->VirtualWaveEntry[iIndex].dwLen1,
					pDir->VirtualWaveEntry[iIndex].dwLen2,
					pDir->VirtualWaveEntry[iIndex].dwStart1,
					pDir->VirtualWaveEntry[iIndex].dwStart2);
				break;
			}
		}
		
		iIndex++;
	}
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// ReadWaveFile
// 
// Reads a file using the FAT to a local file in WAV format (44 bytes header)
// 
// -> pDisk = pointer to disk to use
//    pVirtualWaveFile = pointer to directory entry describing the wave file
//    cDestFN = name of file on local disk to receive data of Ensoniq file
//    cSourceFN = name of source file (only for progress reporting)
// <- ERR_OK
//    ERR_FAT
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//	  ERR_LOCAL_WRITE
//	  ERR_ABORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
int ReadWaveFile(DISK *pDisk, VIRTUALWAVEFILE *pVirtualWaveFile, char *cDestFN,
				 char *cSourceFN)
{
	unsigned char ucBuf1[512], ucBuf2[512], ucStereoBuf[1024], ucTemp;
	int i, j, iResult, iProgress, iLastProgress = -1;
	DWORD dwByteRate, dwBlock1, dwBlock2, dwChunkSize, dwSampleRate, dwSize;
	FILE *f;

	// TODO: Handle multi disk audio tracks (priority: very low, multi disk 
	// audio tracks are theoretically not possible)

	dwSize = pVirtualWaveFile->dwLen1 * 512;
	if(pVirtualWaveFile->ucIsStereo) dwSize *= 2;
	
	dwChunkSize = dwSize + 36;
	dwSampleRate = 44100;
	dwByteRate = dwSampleRate*2; // 16 bits
	if(pVirtualWaveFile->ucIsStereo) dwByteRate *= 2;
	
	LOG("ReadWaveFile('%s'->'%s', size=%d bytes)\n", cSourceFN, cDestFN, 
		dwSize);
	
	// open destination file
	f = fopen(cDestFN, "wb");
	if(NULL==f)
	{
		LOG("Error opening destination file for writing.\n");
		return ERR_NOT_OPEN;
	}
	
#define BYTE0(x) ((x>> 0)&0xFF)
#define BYTE1(x) ((x>> 8)&0xFF)
#define BYTE2(x) ((x>>16)&0xFF)
#define BYTE3(x) ((x>>24)&0xFF)

	// write WAV header to file	
	LOG("Writing WAV header... ");
	memset(ucBuf1, 0, 512);
	ucBuf1[ 0] = 'R';
	ucBuf1[ 1] = 'I';
	ucBuf1[ 2] = 'F';
	ucBuf1[ 3] = 'F';
	ucBuf1[ 4] = BYTE0(dwChunkSize);
	ucBuf1[ 5] = BYTE1(dwChunkSize);
	ucBuf1[ 6] = BYTE2(dwChunkSize);
	ucBuf1[ 7] = BYTE3(dwChunkSize);
	ucBuf1[ 8] = 'W'; 
	ucBuf1[ 9] = 'A';
	ucBuf1[10] = 'V';
	ucBuf1[11] = 'E';
	ucBuf1[12] = 'f';
	ucBuf1[13] = 'm';
	ucBuf1[14] = 't';
	ucBuf1[15] = ' ';
	ucBuf1[16] = 16;		// sub chunk size (16)
	ucBuf1[17] = 0;
	ucBuf1[18] = 0;
	ucBuf1[19] = 0;
	ucBuf1[20] = 0x01;	// format = PCM (0x0001)
	ucBuf1[21] = 0x00;
	ucBuf1[22] = (pVirtualWaveFile->ucIsStereo) ? 0x02 : 0x01;
	ucBuf1[23] = 0x00;
	ucBuf1[24] = BYTE0(dwSampleRate);
	ucBuf1[25] = BYTE1(dwSampleRate);
	ucBuf1[26] = BYTE2(dwSampleRate);
	ucBuf1[27] = BYTE3(dwSampleRate);
	ucBuf1[28] = BYTE0(dwByteRate);	// byte rate
	ucBuf1[29] = BYTE1(dwByteRate);
	ucBuf1[30] = BYTE2(dwByteRate);
	ucBuf1[31] = BYTE3(dwByteRate);
	ucBuf1[32] = (pVirtualWaveFile->ucIsStereo) ? 0x04 : 0x02;	// block align
	ucBuf1[33] = 0x00;
	ucBuf1[34] = 16;		// 16 bits per sample
	ucBuf1[35] = 0;
	ucBuf1[36] = 'd';
	ucBuf1[37] = 'a';
	ucBuf1[38] = 't';
	ucBuf1[39] = 'a';
	ucBuf1[40] = BYTE0(dwSize);
	ucBuf1[41] = BYTE1(dwSize);
	ucBuf1[42] = BYTE2(dwSize);
	ucBuf1[43] = BYTE3(dwSize);
	
	if(44!=fwrite(ucBuf1, 1, 44, f))
	{
		LOG("Error writing to destination file.\n");
		fclose(f);
		return ERR_LOCAL_WRITE;
	}
	
	LOG("Extracting file... ");
	
	// set starting block
	dwBlock1 = pVirtualWaveFile->dwStart1;
	dwBlock2 = pVirtualWaveFile->dwStart2;
	
	// loop through all blocks
	for(i=0; i<(int)pVirtualWaveFile->dwLen1; i++)
	{
		// read next block from disk image
		iResult = ReadBlock(pDisk, dwBlock1, ucBuf1);
		if(ERR_OK!=iResult)
		{
			LOG("Error reading block, code=%d.\n", iResult);
			fclose(f);
			return iResult;
		}

		if(pVirtualWaveFile->ucIsStereo)
		{
			iResult = ReadBlock(pDisk, dwBlock2, ucBuf2);
			if(ERR_OK!=iResult)
			{
				LOG("Error reading block, code=%d.\n", iResult);
				fclose(f);
				return iResult;
			}
			// swap bytes and multiplex two channels
			for(j=0; j<256; j++)
			{
				ucStereoBuf[j*4+0] = ucBuf1[j*2+1];
				ucStereoBuf[j*4+1] = ucBuf1[j*2+0];
				ucStereoBuf[j*4+2] = ucBuf2[j*2+1];
				ucStereoBuf[j*4+3] = ucBuf2[j*2+0];
			}
			// write next block to file
			if(1024!=fwrite(ucStereoBuf, 1, 1024, f))
			{
				LOG("Error writing to destination file.\n");
				fclose(f);
				return ERR_LOCAL_WRITE;
			}
			// get next FAT entry
			dwBlock2 = GetFATEntry(pDisk, dwBlock2);
		}
		else
		{
			// swap bytes, only one channel
			for(j=0; j<256; j++)
			{
				ucTemp = ucBuf1[j*2];
				ucBuf1[j*2] = ucBuf1[j*2+1];
				ucBuf1[j*2+1] = ucTemp;
			}
			// write next block to file
			if(512!=fwrite(ucBuf1, 1, 512, f))
			{
				LOG("Error writing to destination file.\n");
				fclose(f);
				return ERR_LOCAL_WRITE;
			}
		}
				
		// get next FAT entry
		dwBlock1 = GetFATEntry(pDisk, dwBlock1);

		// either end of file reached or error getting FAT entry occured
		if((dwBlock1<3)||((pVirtualWaveFile->ucIsStereo)&&(dwBlock2<3)))
		{
			LOG("Warning: EOF in FAT occured before file length was reached.\n");
			break;
		}
		
		// notify TotalCmd of progress
		iProgress = i*100/pVirtualWaveFile->dwLen1;
		if((iProgress-iLastProgress)>5)
		{
			if(1==g_pProgressProc(g_iPluginNr, cSourceFN, cDestFN, iProgress))
			{
				fclose(f);
				return ERR_ABORTED;
			}
			iLastProgress = iProgress;
		}
	}
	LOG("OK.\n");
	fclose(f);
	
	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, cSourceFN, cDestFN, 100))
		return ERR_ABORTED;
		
	return ERR_OK;
}

DISK *SearchDisk(char *cMsDosName)
{
	DISK *pDisk = g_pDiskListRoot;
	
	while(pDisk)
	{
		if(0==strcmp(cMsDosName, pDisk->cMsDosName))
		{
			return pDisk;
		}
		pDisk = pDisk->pNext;
	}
	
	return 0;
}

int SelectNextDisk(DISK **pDisk, DWORD *dwBlock, ENSONIQDIRENTRY *pDirEntry)
{
	char cMessage[512], *cMsDosName;
	DISK *pNewDisk;
	ENSONIQDIRENTRY *pe;
	int i, iResult;
	
	// loop until a valid next disk is selected or user aborts
	// -> user gets chance to switch disks or disk images multiple
	// times if choice was wrong
	while(1)
	{
		// ask user for new disk
		pNewDisk = 0;
		iResult = CreateChooseDiskDialogModal(0, &pNewDisk, *pDisk);
		if((iResult==IDCANCEL)||(pNewDisk==0))
		{
			return ERR_ABORTED;
		}
		
		// get the MSDOS path of the chosen disk
		cMsDosName = malloc(strlen(pNewDisk->cMsDosName));
		strcpy(cMsDosName, pNewDisk->cMsDosName);
		
		// reload disk list, just in case user has exchanged some
		// removable media
		FreeDiskList(0, g_pDiskListRoot);
		g_pDiskListRoot = ScanDevices(0);
		if(0==g_pDiskListRoot)
		{
			free(cMsDosName);
			if(IDCANCEL==MessageBoxA(0, 
				"There was an error during device scan. "
				"Try again?", "EnsoniqFS � Warning", 
				MB_ICONWARNING | MB_RETRYCANCEL))
			{
				return ERR_ABORTED;
			}
			continue; // retry
		}
		
		// find the disk from the remembered name
		pNewDisk = SearchDisk(cMsDosName);
		free(cMsDosName);
		if(0==pNewDisk)
		{
			if(IDCANCEL==MessageBoxA(0, 
				"The selected disk could not be found. "
				"Try again?", "EnsoniqFS � Warning", 
				MB_ICONWARNING | MB_RETRYCANCEL))
			{
				return ERR_ABORTED;
			}
			continue; // retry
		}

		// read root directory
		ENSONIQDIR e;
		e.dwDirectoryBlock = 3;
		if(ERR_OK!=ReadDirectory(pNewDisk, &e, 0))
		{
			if(IDCANCEL==MessageBoxA(0, 
				"Could not read the selected disk. "
				"Retry?", "EnsoniqFS � Warning", 
				MB_ICONWARNING | MB_RETRYCANCEL))
			{
				return ERR_ABORTED;
			}
			continue; // retry
		}

		// search the new directory for the file in question
		for(i=0; i<39; i++)
		{
			pe = &e.Entry[i];
			
			// skip empty lines and directory pointers
			if(pe->ucType==FILE_TYPE_EMPTY) continue;
			if(pe->ucType==FILE_TYPE_DIRECTORY) continue;
			if(pe->ucType==FILE_TYPE_PARENT_DIRECTORY) continue;
			
			// continue if type or name differs
			if(pe->ucType!=pDirEntry->ucType) continue;
			if(0!=strncmp(pe->cName, pDirEntry->cName, 12)) continue;

			// if we reach this point, all checks have been made
			// -> this is our file, leave the loop
			break;
		}

		// Did we reach the end of the directory without finding our file?
		if(i>=39)
		{
			if(IDCANCEL==MessageBoxA(0, 
				"Could not find the multi disk file on the selected disk. "
				"Retry?", "EnsoniqFS � Warning", 
				MB_ICONWARNING | MB_RETRYCANCEL))
			{
				return ERR_ABORTED;
			}
			continue; // retry
		}
		
		// check multi disk index
		if(pe->ucMultiFileIndex != pDirEntry->ucMultiFileIndex+1)
		{
			sprintf(cMessage, 
				"The selected disk contains the requested file, but this\n"
				"is part %d instead of %d. Retry?", 
				pe->ucMultiFileIndex, pDirEntry->ucMultiFileIndex+1);
			if(IDCANCEL==MessageBoxA(0, 
				cMessage, "EnsoniqFS � Warning", 
				MB_ICONWARNING | MB_RETRYCANCEL))
			{
				return ERR_ABORTED;
			}
			continue; // retry
		}
		
		// Switch to next part
		memcpy(pDirEntry, pe, sizeof(ENSONIQDIRENTRY));
		*pDisk = pNewDisk;
		*dwBlock = pe->dwStart;
		break;
	}
	
	return ERR_OK;
}
//----------------------------------------------------------------------------
// ReadEnsoniqFile
// 
// Reads a file using the FAT to a local file in EFE format (512 bytes header)
// 
// -> pDisk = pointer to disk to use
//    pDirEntry = pointer to directory entry describing the Ensoniq file
//    cDestFN = name of file on local disk to receive data of Ensoniq file
//    cSourceFN = name of source file (only for progress reporting)
// <- ERR_OK
//    ERR_FAT
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//	  ERR_LOCAL_WRITE
//	  ERR_ABORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
int ReadEnsoniqFile(DISK *pDisk, ENSONIQDIRENTRY *pDirEntry, char *cDestFN,
					char *cSourceFN)
{
	unsigned char ucBuf[512], ucCopyMultidisk = 0;
	char cFiletype[13];
	int i, iResult, iProgress, iLastProgress = -1;
	DWORD dwBlock, dwSize, dwBlockCounter = 0;
	FILE *f;
	
	LOG("Reading Ensoniq file (Start=%d, size=%d blocks, type=%d, "
		"destination='%s'\n", pDirEntry->dwStart, pDirEntry->dwLen,
		pDirEntry->ucType, cDestFN);
	
	// check pointers
	if(NULL==pDisk) return ERR_NOT_OPEN;
	if(NULL==pDirEntry) return ERR_NOT_OPEN;
	if(NULL==cDestFN) return ERR_LOCAL_WRITE;

	// apply special treatment if this is a multi disk file
	if(pDirEntry->ucMultiFileIndex)
	{
		// ask if multi disk copy is required
		if(1==pDirEntry->ucMultiFileIndex)
		{
	   		i = MessageBoxA(0, "This file is the first part of a "
			   "multi-disk file. To merge all parts\n"
			   "into one file make sure you have\n"
			   " (a) all necessary removable disks ready\n"
			   "   or\n"
			   " (b) all necessary fixed disks or image files mounted.\n\n"
			   "Do you want to merge all files (Yes), copy only this single\n"
			   "file (No) or cancel the process?", 
			   "EnsoniqFS � Warning", 
			   MB_ICONWARNING | MB_YESNOCANCEL);
			if(i==IDCANCEL) 
			{
				return ERR_ABORTED;
			}
			if(i==IDYES)
			{
				ucCopyMultidisk = 1;	
			}
		}
		else
		{
			// warn user that this is some middle part of a multi disk file
	   		if(IDNO == MessageBoxA(0, "This file is the middle or end part of a "
			   "multi-disk file. To join all parts\n"
			   "to one file you have to start with the first part.\n\n"
			   "Do you want to copy this part only?", 
			   "EnsoniqFS � Warning", 
			   MB_ICONWARNING | MB_YESNO))
			{
				return ERR_ABORTED;
			}
		}
	}

	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, cSourceFN, cDestFN, 0))
		return ERR_ABORTED;
	
	// open destination file
	f = fopen(cDestFN, "wb");
	if(NULL==f)
	{
		LOG("Error opening destination file for writing.\n");
		return ERR_NOT_OPEN;
	}

	// use length from directory entry as size; this can change for multi disk
	// files once the first block has been read and the real file size is known
	dwSize = pDirEntry->dwLen;

	// set starting block
	dwBlock = pDirEntry->dwStart;
		
	if(ucCopyMultidisk)
	{
		// read first block (instrument header)
		iResult = ReadBlock(pDisk, dwBlock, ucBuf);
		if(ERR_OK!=iResult)
		{
			LOG("Error reading block, code=%d.\n", iResult);
			fclose(f);
			return iResult;
		}
		
		// get real size from instrument header
		dwSize = ((DWORD)ucBuf[0x28] << 8) + (DWORD)ucBuf[0x29];
		LOG("Reading multi disk file, real size=%d blocks.\n", dwSize); 
	}
		
	// write EFE header to file	
	LOG("Writing header... ");
	memset(ucBuf, 0, 512);
	sprintf(ucBuf, "\r\nEps File:       %s    ", pDirEntry->cName);
	GetShortEnsoniqFiletype(pDirEntry->ucType, cFiletype);
	strcat(ucBuf, cFiletype);	
	strcat(ucBuf, "\r\n");
	ucBuf[0x31] = 0x1A;
	ucBuf[0x32] = pDirEntry->ucType;
	ucBuf[0x34] = (dwSize>>8)&0xFF;
	ucBuf[0x35] = (dwSize)&0xFF;
	ucBuf[0x36] = (pDirEntry->dwContiguous>>8)&0xFF;
	ucBuf[0x37] = (pDirEntry->dwContiguous)&0xFF;
	ucBuf[0x38] = (pDirEntry->dwStart>>8)&0xFF;
	ucBuf[0x39] = (pDirEntry->dwStart)&0xFF;
	ucBuf[0x3A] = pDirEntry->ucMultiFileIndex;

	// reset the multi disk field if we are about to copy all parts
	// and merge them into one file which isn't multi disk anymore afterwards
	if(ucCopyMultidisk) ucBuf[0x3A] = 0;
	
	if(512!=fwrite(ucBuf, 1, 512, f))
	{
		LOG("Error writing to destination file.\n");
		fclose(f);
		return ERR_LOCAL_WRITE;
	}

	LOG("Extracting file... ");
	
	// loop through all blocks
	i = 0;
	while(dwBlockCounter<dwSize)
	{
		// read next block from disk image
		iResult = ReadBlock(pDisk, dwBlock, ucBuf);
		if(ERR_OK!=iResult)
		{
			LOG("Error reading block, code=%d.\n", iResult);
			fclose(f);
			return iResult;
		}
		
		// write next block to file
		if(512!=fwrite(ucBuf, 1, 512, f))
		{
			LOG("Error writing to destination file.\n");
			fclose(f);
			return ERR_LOCAL_WRITE;
		}
		
		// get next FAT entry
		dwBlock = GetFATEntry(pDisk, dwBlock);

		// either end of file reached or error getting FAT entry occured
		if(dwBlock<3)
		{
			LOG("FAT entry=%d, i=%d, Len=%d - stopping.\n", dwBlock, i, 
				pDirEntry->dwLen);
			i = pDirEntry->dwLen; // stop here even if file is too short
		}
		
		// notify TotalCmd of progress
		iProgress = i*100/dwSize;
		if((iProgress-iLastProgress)>5)
		{
			if(1==g_pProgressProc(g_iPluginNr, cSourceFN, cDestFN, iProgress))
			{
				fclose(f);
				return ERR_ABORTED;
			}
			iLastProgress = iProgress;
		}
		
		i++;
		dwBlockCounter++;
		
		// check counter in case of a multi-disk file
		if(ucCopyMultidisk && (dwBlockCounter<dwSize))
		{
			// next disk?
			if(i>=(int)pDirEntry->dwLen)
			{
				// ask user for next disk
				iResult = SelectNextDisk(&pDisk, &dwBlock, pDirEntry);
				if(ERR_OK!=iResult)
				{
					fclose(f);
					return iResult;
				}
				
				LOG("Multi-disk file: Continuing with next disk (%d)\n", 
					pDirEntry->ucMultiFileIndex);
				
				// reset counter for current part
				i = 0;
			}
		}
	}
	
	LOG("OK.\n");
	fclose(f);
	
	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, cSourceFN, cDestFN, 100))
		return ERR_ABORTED;
		
	return ERR_OK;
	
	// TODO: Delete destination file if there was an abort or if multi disk
	// copy was stopped
}

//----------------------------------------------------------------------------
// GetDirectoryLevel
//
// Counts the directory level (backslashes)
//
// -> cDir = directory
// <- number of backslashes
//----------------------------------------------------------------------------
int GetDirectoryLevel(char *cDir)
{
	int iCount = 0, i;
	
	// root level = 0
	if(0==strcmp(cDir, "\\")) return 0;
	
	// count backslashes
	for(i=0; i<(int)strlen(cDir); i++) if('\\'==cDir[i]) iCount++;
	return iCount;
}

//----------------------------------------------------------------------------
// GetDiskFromPath
//
// Find the disk which is addressed with given path
//
// -> cPath = path with disk name in the beginning
// <- >0: pointer to disk structure
//    =0: error
//----------------------------------------------------------------------------
DISK *GetDiskFromPath(char *cPath)
{
	char cDiskName[260];
	DISK *pDisk;
	int i, j;
	
	LOG("GetDiskFromPath(\"%s\"): ", cPath);
	
	// check pointer
	if(NULL==cPath) return NULL;
	
	// image file?
	if(0==strncmp("\\Image files\\", cPath, 13))
	{
		// find backslash after image file name
		j=13; while((cPath[j]!='\\')&&(cPath[j]!=0)) j++;

		// loop through all registered disks
		pDisk = g_pDiskListRoot;
		while(pDisk)
		{
			if(TYPE_FILE==pDisk->iType)
			{
				// disk name found?
				if(0==strncmp(pDisk->cMsDosName+10, cPath+13, j-13))
				{
					LOG("\"%s\"\n", pDisk->cMsDosName+10);
					return pDisk;
				}
			}
			pDisk = pDisk->pNext;
		}
	}
	else	// normal device
	{
		// find second backslash
		j=1; while((cPath[j]!='\\')&&(cPath[j]!=0)) j++;
		j++;
		
		// find first space or \0
		i=0;
		while((cPath[i+j]!=' ')&&(cPath[i+j]!=0))
		{
			cDiskName[i] = cPath[i+j];
			i++;
		}
		cDiskName[i] = 0;
		LOG("\"%s\"\n", cDiskName);
		
		// loop through all registered disks
		pDisk = g_pDiskListRoot;
		while(pDisk)
		{
			// disk name found?
			if(0==strcmp(pDisk->cMsDosName+4, cDiskName))
			{
				return pDisk;
			}
			pDisk = pDisk->pNext;
		}
	}
	
	LOG("Not found.\n");
	return 0;
}

//----------------------------------------------------------------------------
// FreeHandle
// 
// Frees all the memory allocated by the handle
// 
// -> pHandle: pointer to handle
// <- --
//----------------------------------------------------------------------------
void FreeHandle(FIND_HANDLE *pHandle)
{
	// free allocated memory
	free(pHandle);
}

//----------------------------------------------------------------------------
// ReadDirectoryFromPath
// 
// Reads the directory currently selected in pHandle->cPath and stores its
// entries into pHandle->EnsoniqDirectory
// 
// -> pHandle: pointer to valid FIND_HANDLE
//	  ucShowWarning: Show warning about bad file sizes
// <- ERR_OK
//    ERR_DISK_NOT_FOUND
//    ERR_PATH_NOT_FOUND
//    ERR_DIRLEVEL
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//    ERR_SEEK
//----------------------------------------------------------------------------
int ReadDirectoryFromPath(FIND_HANDLE *pHandle, unsigned char ucShowWarning)
{
	char cCurrentPath[260], cPath[260], cNextDir[260];
	DWORD dwDirectoryBlock;
	int i, iResult, j, iDirLevel;
	DISK *pDisk;

	upcase(pHandle->cPath);
	
	LOG("ReadDirectoryFromPath(\""); LOG(pHandle->cPath);
	LOG("\")\n");

	// get pointer to selected disk from path name
	pDisk = GetDiskFromPath(pHandle->cPath);
	if(NULL==pDisk)
	{
		return ERR_DISK_NOT_FOUND;
	}
	pHandle->pDisk = pDisk;
	
	// get path without disk name: scan for 3rd occurence of "\"
	i=0; j=0; while(0x00!=pHandle->cPath[i])
	{
		if('\\'==pHandle->cPath[i]) j++;
		if(j>2) break;
		i++;
	}
	
	strcpy(cPath, pHandle->cPath + i);
	if(0==strlen(cPath)) strcpy(cPath, "\\");
	if('\\'!=cPath[strlen(cPath)-1]) strcat(cPath, "\\");
	
	// start in root
	strcpy(cCurrentPath, "\\");
	dwDirectoryBlock = 3;
	
	// try to follow the given path on disk starting from root
	iDirLevel = 0;
	while(iDirLevel++<256)
	{
		// read the current directory
		// show warnings only if this is the last directory in the
		// chain given by the current path
		pHandle->EnsoniqDir.dwDirectoryBlock = dwDirectoryBlock;
		iResult = ReadDirectory(pDisk, &pHandle->EnsoniqDir, 
			ucShowWarning && (0==strcmp(cCurrentPath, cPath)));
		if(ERR_OK!=iResult)
		{
			LOG("Error reading directory blocks, code=%d\n", iResult);
			return iResult;
		}
		
		// did we reach the path we are looking for?
		if(0==strcmp(cCurrentPath, cPath))
		{
			return ERR_OK;
		}
		
		// get next path element, search it in the currently active dir
		i = strlen(cCurrentPath); j=0;
		while('\\'==cPath[i]) i++;
		
		while(0x00!=cPath[i+j])
		{
			if('\\'==cPath[i+j]) break;
			j++;
		}

		memset(cNextDir, 0, 260);
		strncpy(cNextDir, cPath + i, j);
	
		// look for subdir in current directory	
		for(i=0; i<39; i++)
		{
			if(0==strcmp(cNextDir, pHandle->EnsoniqDir.Entry[i].cLegalName))
			{
				dwDirectoryBlock = pHandle->EnsoniqDir.Entry[i].dwStart;
				strcat(cCurrentPath, cNextDir);
				strcat(cCurrentPath, "\\");
				break;
			}
		}
		
		// subdir not found?
		if(39==i)
		{
			LOG("ReadDirectoryFromPath(): failed, ERR_PATH_NOT_FOUND\n");
			return ERR_PATH_NOT_FOUND;
		}
	}
	if(iDirLevel>=256)
	{
		LOG("Error: Directory nesting more than 255 levels deep.\n");
		return ERR_DIRLEVEL;
	}
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// UpdateParentDirectory
// 
// Reads the current directory, counts the files and directories and re-writes
// the entry for the current directory in the parent directory with the
// appropriate number of files (=size of a directory)
//
// The write cache is not flushed, you have to do this manually!
// 
// -> cCurrentDir = pointer to current directory name
// <- ERR_OK
//----------------------------------------------------------------------------
int UpdateParentDirectory(char *cCurrentDir)
{
	FIND_HANDLE CurrentHandle, ParentHandle;
	unsigned char *ucCurrent, *ucParent;
	char cCurrentName[260];
	int i, iNumFiles = 0, iIndex;
	
	upcase(cCurrentDir);
		
	LOG("UpdateParentDirectory(\""); LOG(cCurrentDir); LOG("\"): ");

	// don't update anything if this is the root directory
	if(GetDirectoryLevel(cCurrentDir)<=3) return ERR_OK;

	// isolate current directory name and parent path from complete path
	memset(&CurrentHandle, 0, sizeof(FIND_HANDLE));
	memset(&ParentHandle, 0, sizeof(FIND_HANDLE));
	strcpy(CurrentHandle.cPath, cCurrentDir);
	for(i=strlen(cCurrentDir); i>0; i--) if('\\'==cCurrentDir[i]) break;
	strncpy(ParentHandle.cPath, cCurrentDir, i);
	strcpy(cCurrentName, cCurrentDir+i+1);
	cCurrentName[12] = 0x00;
	while(strlen(cCurrentName)<12) strcat(cCurrentName, " ");

	// read directory structures
	if(ERR_OK!=ReadDirectoryFromPath(&CurrentHandle, 0))
	{
		LOG("Error: Unable to read current directory.\n");
		return FALSE;
	}
	ucCurrent = CurrentHandle.EnsoniqDir.ucDirectory;
	
	if(ERR_OK!=ReadDirectoryFromPath(&ParentHandle, 0))
	{
		LOG("Error: Unable to read parent directory.\n");
		return FALSE;
	}
	ucParent = ParentHandle.EnsoniqDir.ucDirectory;
	
	// count items in current directory
	for(i=0; i<39; i++)
	{
		if((ucCurrent[i*26+1]!=0x00)&&(ucCurrent[i*26+1]!=0x08)) iNumFiles++;
	}

	// find current directory name in parent directory
	iIndex = -1;
	for(i=0; i<39; i++)
	{
		// is this the current directory name?
		if((0==strncmp(ucParent+i*26+2, cCurrentName, 12))&&
			(0x02==ucParent[i*26+1]))
		{
			iIndex = i; break;
		}
	}
	
	if(-1==iIndex)
	{
		LOG("Error: Current directory name could not be found in parent "
			"directory.\n");
		return ERR_NOT_FOUND;
	}
	
	ucParent[iIndex*26+14] = iNumFiles>>8; 
	ucParent[iIndex*26+15] = iNumFiles & 0xFF;
	if(ERR_OK!=WriteBlocks(ParentHandle.pDisk, 
		ParentHandle.EnsoniqDir.dwDirectoryBlock, 
		2, ParentHandle.EnsoniqDir.ucDirectory))
	{
		LOG("Error writing parent directory blocks.\n");
		return ERR_WRITE;
	}

	LOG("OK.\n");
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// FsInit
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsInit(int PluginNr, tProgressProc pProgressProc,
							   tLogProc pLogProc, tRequestProc pRequestProc)
{
	LOG("FsInit() called.\n");

	// save the parameters submitted by TotalCommander
	g_iPluginNr = PluginNr;
	g_pLogProc = pLogProc;
	g_pRequestProc = pRequestProc;
	g_pProgressProc = pProgressProc;
	
	return 0;
}

//----------------------------------------------------------------------------
// FsFindFirst
//
//----------------------------------------------------------------------------
DLLEXPORT HANDLE __stdcall FsFindFirst(char* cPath, WIN32_FIND_DATA *FindData)
{
	FIND_HANDLE *pHandle, *pTemp;

	LOG("FsFindFirst(\""); LOG(cPath); LOG("\") called.\n");
	
	// allocate new search structure
	pHandle = malloc(sizeof(FIND_HANDLE));
	if(NULL==pHandle)
	{
		LOG("Failed: Unable to allocate handle structure.\n");
		return INVALID_HANDLE_VALUE;
		
	}
	memset(pHandle, 0, sizeof(FIND_HANDLE));
	strcpy(pHandle->cPath, cPath);
	
	// insert new search structure into linked list
	if(NULL==g_pHandleRoot)
	{
		pHandle->pPrevious = NULL;
		g_pHandleRoot = pHandle;
	}
	else
	{
		// find last handle in list
		pTemp = g_pHandleRoot;
		while(pTemp->pNext) pTemp = pTemp->pNext;
		
		// attach newly created handle to last member of list
		pTemp->pNext = pHandle;
		pHandle->pPrevious = pTemp;
	}

	// clear FindData
	memset(FindData, 0, sizeof(WIN32_FIND_DATA));
	
	// is this the root?
	if(0==GetDirectoryLevel(cPath))
	{
		// Rescan devices if necessary
		if((NULL==g_pDiskListRoot)||
			(g_pDiskListRoot&&g_iOptionAutomaticRescan))
		{
			FreeDiskList(1, g_pDiskListRoot);
			g_pDiskListRoot = ScanDevices(0);
		}
		
		FsFindNext(pHandle, FindData);
		return pHandle;
	}
	else
	{
		// rescan disks if not done yet
		if(NULL==g_pDiskListRoot) g_pDiskListRoot = ScanDevices(0);
		
		FsFindNext(pHandle, FindData);
		return pHandle;
	}
	
	return INVALID_HANDLE_VALUE;
}

/*
//----------------------------------------------------------------------------
// FsExtractCustomIcon
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsExtractCustomIcon(char* RemoteName,
											int ExtractFlags,
											HICON* TheIcon)
{
	return FS_ICON_USEDEFAULT;
}
*/


//----------------------------------------------------------------------------
// FsExecuteFile
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsExecuteFile(HWND MainWin, char* RemoteName, 
									  char* Verb)
{
	char cPath[260], cArg[260];
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	
	LOG("FsExecuteFile(MainWin=0x%08X, RemoteName=\"%s\", Verb=\"\")\n", 
		(int)MainWin, RemoteName, Verb);

	if((0==strcmp(RemoteName, "\\Options"))&&(0==strcmp(Verb, "open")))
	{
		CreateOptionsDialogModal(MainWin);
	}
	
	if((0==strcmp(RemoteName, "\\Rescan devices"))&&(0==strcmp(Verb, "open")))
	{
		FreeDiskList(1, g_pDiskListRoot);
		g_pDiskListRoot = ScanDevices(0);
	}

	if((0==strcmp(RemoteName, "\\Run Ensoniq Filesystem Tools"))&&
		(0==strcmp(Verb, "open")))
	{
		// free all devices so ETools can use them
		FreeDiskList(1, g_pDiskListRoot);

		// decrease reference counter
		g_ucSharedMemory[0]--;
		
		// call ETools
		strcpy(cPath, g_cOptionInstallPath);
		strcat(cPath, "\\ETools.exe");
		memset(&pi, 0, sizeof(PROCESS_INFORMATION));
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		
		if(0==CreateProcess(cPath, cArg, 0, 0, 0, 0, 0, 0, &si, &pi))
		{
			MessageBoxA(0, "Could not start ETools.exe.", "EnsoniqFS � Error",
				MB_ICONSTOP);
		}
		else
		{
			// wait for process to terminate
			while(WAIT_TIMEOUT==WaitForSingleObject(pi.hProcess, 100));
		}

		// increase reference counter
		g_ucSharedMemory[0]++;
		
		// claim back device list
		g_pDiskListRoot = ScanDevices(0);
	}

	return FS_EXEC_OK;
}


//----------------------------------------------------------------------------
// FsFindNext
//
//----------------------------------------------------------------------------
DLLEXPORT BOOL __stdcall FsFindNext(HANDLE Handle, WIN32_FIND_DATA *FindData)
{
	FIND_HANDLE *pHandle = (FIND_HANDLE*) Handle;
	int i, iDirectoryLevel;
	DISK *pDisk;
	char cFree[128], cType;
	char cRootDir[] =	"dPhysical disks\n"
						"dCDROMs\n"
						"dImage files\n"
						"fOptions\n"
						"fRescan devices\n"
						"fRun Ensoniq Filesystem Tools\n";
						
	// check pointer
	if(NULL==pHandle)
	{
		LOG("Error: pHandle==NULL\n");
		return FALSE;
	}

	// clear FindData
	memset(FindData, 0, sizeof(WIN32_FIND_DATA));
	
	iDirectoryLevel = GetDirectoryLevel(pHandle->cPath);
	
//............................................................................
// deliver root file names
//............................................................................
	// is this the root?
	if(0==iDirectoryLevel)
	{
		// was the last entry already delivered?
		if(0==cRootDir[pHandle->iNextDirIndex])
		{
			LOG("Reached end of root directory.\n");
			return FALSE;
		}
		
		
		// copy name
		cType = cRootDir[pHandle->iNextDirIndex++];
		i=0; while('\n'!=cRootDir[pHandle->iNextDirIndex])
		{
			FindData->cFileName[i++] = cRootDir[pHandle->iNextDirIndex++];
		}

		// make every file but "Options" a directory
		if('d'==cType) FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

		pHandle->iNextDirIndex++;
		LOG("Returning \""); LOG(FindData->cFileName); LOG("\"\n");
		return TRUE;
	}
	
//............................................................................
// deliver device names (folder physical disks, CDROMs, image files)
//............................................................................
	// is this the device group level?
	if(1==iDirectoryLevel)
	{
		// deliver parent folder first
		if(0==pHandle->iNextDirIndex)
		{
			strcpy(FindData->cFileName, "..");
			FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			pHandle->iNextDirIndex++;
			return TRUE;
		}
		
		// group = "physical disks"?
		if(0==strcmp(pHandle->cPath, "\\Physical disks"))
		{
			pDisk = g_pDiskListRoot; i = 0;
			// loop through all physical disks, 
			// look for CDROM number "iNextDirIndex"
			while(pDisk)
			{
				// only count disks of type physical disk or floppy
				if((TYPE_DISK == pDisk->iType)||(TYPE_FLOPPY == pDisk->iType))
				{
					// current disk found?
					if(++i==pHandle->iNextDirIndex)
					{
						// copy name to FindData and return
						strcpy(FindData->cFileName, pDisk->cMsDosName+4);
						strcat(FindData->cFileName, " '");
						strcat(FindData->cFileName, pDisk->cLegalDiskLabel);
						strcat(FindData->cFileName, "'");
						FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
						pHandle->iNextDirIndex++;
						return TRUE;
					}
				}
				pDisk = pDisk->pNext;
			}
			
			// end of physical drive list
			return FALSE;
		}
		
		// group = "CDROMs"?
		else if(0==strcmp(pHandle->cPath, "\\CDROMs"))
		{
			// loop through all CDROMs, look for CDROM number "iNextDirIndex"
			pDisk = g_pDiskListRoot; i = 0;
			while(pDisk)
			{
				// only count disks of type CDROM
				if(TYPE_CDROM == pDisk->iType)
				{
					// current CDROM found?
					if(++i==pHandle->iNextDirIndex)
					{
						// copy name to FindData and return
						strcpy(FindData->cFileName, pDisk->cMsDosName+4);
						strcat(FindData->cFileName, " '");
						strcat(FindData->cFileName, pDisk->cLegalDiskLabel);
						strcat(FindData->cFileName, "'");
						FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
						pHandle->iNextDirIndex++;
						return TRUE;
					}
				}
				pDisk = pDisk->pNext;
			}
			
			// end CDROM list
			return FALSE;
		}

		// group = "Image files"?
		else if(0==strcmp(pHandle->cPath, "\\Image files"))
		{
			// loop through all images, look for image number "iNextDirIndex"
			pDisk = g_pDiskListRoot; i = 0;
			while(pDisk)
			{
				// only count disks of type TYPE_FILE
				if(TYPE_FILE == pDisk->iType)
				{
					// current CDROM found?
					if(++i==pHandle->iNextDirIndex)
					{
						// copy name to FindData and return
						strcpy(FindData->cFileName, pDisk->cMsDosName+10);
						FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
						pHandle->iNextDirIndex++;
						return TRUE;
					}
				}
				pDisk = pDisk->pNext;
			}
			
			// end image list
			return FALSE;
		}
		
		// unknown group, normally this code should not be executed
		else
		{
			pHandle->iNextDirIndex++;
			return FALSE;
		}
	}

//............................................................................
// if we come to here we are in some disk/cdrom/image folder
//............................................................................
	
	// iNextDirIndex:  0      - parent directory ".."
	//                 1...39 - file 0...38
	//                40...78 - virtual wave file 0...38
	//                     79 - "blocks free" message
	
	// deliver parent folder first
	if(0==pHandle->iNextDirIndex)
	{
		strcpy(FindData->cFileName, "..");
		FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		pHandle->iNextDirIndex++;
		return TRUE;
	}
	
	// if first directory entry, read directory from disk
	if(1==pHandle->iNextDirIndex)
	{
		if(ERR_OK!=ReadDirectoryFromPath(pHandle, 1))
		{
			LOG("Error: Unable to read directory \"");
			LOG(pHandle->cPath); LOG("\"\n");
			return FALSE;
		}
	}

	// skip empty entries and parent directories
	while(((FILE_TYPE_EMPTY==pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].ucType)||
		  (FILE_TYPE_PARENT_DIRECTORY==pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].ucType))&&
		  (pHandle->iNextDirIndex<40))
	{
		pHandle->iNextDirIndex++;
	}
	
	// skip empty virtual wave files
	while((0==pHandle->EnsoniqDir.VirtualWaveEntry[pHandle->iNextDirIndex-40].cName[0])
		&&(pHandle->iNextDirIndex>=40)&&(pHandle->iNextDirIndex<79))
	{
		pHandle->iNextDirIndex++;
	}
	
	// regular files
	if(pHandle->iNextDirIndex < 40)
	{
		// copy current directory entry
		strcpy(FindData->cFileName, 
			pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].cLegalName);
	
		if(FILE_TYPE_DIRECTORY==pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].ucType)
		{
			FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		}
		else
		{
			// reformat filename including file type tag
			sprintf(FindData->cFileName, "%s.[%02i].EFE",
				pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].cLegalName,
				pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].ucType);
			FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			FindData->nFileSizeLow = 
				(pHandle->EnsoniqDir.Entry[pHandle->iNextDirIndex-1].dwLen+1)*512;
	  	}
		pHandle->iNextDirIndex++;
		return TRUE;
	}
	// virtual wave files
	else if(pHandle->iNextDirIndex < 79)
	{
		// reformat filename 
		if(pHandle->EnsoniqDir.VirtualWaveEntry[pHandle->iNextDirIndex-40].ucIsStereo)
		{
			sprintf(FindData->cFileName, "%s_STEREO.wav",
				pHandle->EnsoniqDir.VirtualWaveEntry[pHandle->iNextDirIndex-40].cLegalName);
			FindData->nFileSizeLow = 
				(pHandle->EnsoniqDir.VirtualWaveEntry[pHandle->iNextDirIndex-40].dwLen1)*1024;
		}
		else
		{
			sprintf(FindData->cFileName, "%s.wav",
				pHandle->EnsoniqDir.VirtualWaveEntry[pHandle->iNextDirIndex-40].cLegalName);
			FindData->nFileSizeLow = 
				(pHandle->EnsoniqDir.VirtualWaveEntry[pHandle->iNextDirIndex-40].dwLen1)*512;
		}
		FindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		
		// add size of wave file header
		FindData->nFileSizeLow += 44;
		
		pHandle->iNextDirIndex++;
		return TRUE;
	}
	
	// deliver virtual "xxx blocks free" file
	else if(79==pHandle->iNextDirIndex)
	{
		// only report free space in ROOT directory of each device
		if(2!=GetDirectoryLevel(pHandle->cPath)) return FALSE;
		
		// don't report free space for CDROMs
		if(TYPE_CDROM==pHandle->pDisk->iType) return FALSE;

		sprintf(cFree, "%i", (int)pHandle->pDisk->dwBlocksFree);
		FindData->cFileName[0] = 0;
		for(i=0; i<(int)strlen(cFree); i++)
		{
			strncat(FindData->cFileName, cFree + i, 1);
			if((1==((strlen(cFree)-i)%3))&&(i!=(int)strlen(cFree)-1))
				strcat(FindData->cFileName, ",");
		}
		strcat(FindData->cFileName, " blocks free (");
		
		sprintf(cFree, "%i", (int)pHandle->pDisk->dwBlocksFree/2);
		for(i=0; i<(int)strlen(cFree); i++)
		{
			strncat(FindData->cFileName, cFree + i, 1);
			if((1==((strlen(cFree)-i)%3))&&(i!=(int)strlen(cFree)-1))
				strcat(FindData->cFileName, ",");
		}
		strcat(FindData->cFileName, " kB)");
		
		pHandle->iNextDirIndex++;
		return TRUE;
	}
	
	// this must be the last entry
	return FALSE;
}

//----------------------------------------------------------------------------
// FsFindClose
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsFindClose(HANDLE Handle)
{
	FIND_HANDLE *pHandle = (FIND_HANDLE*) Handle;
	
	// check pointer
	if(NULL==pHandle) return FALSE;

	LOG("FsFindClose() called.\n");
	
	// remove handle from list
	if(pHandle->pPrevious)
	{
		pHandle->pPrevious->pNext = pHandle->pNext;
	}
	else // delete first element of list
	{
		if(NULL==pHandle->pNext)	// is this the first and last?
		{
			g_pHandleRoot = NULL;	// list is now empty
		}
		else
		{
			// make next item the new start of the list
			g_pHandleRoot = pHandle->pNext;
		}
	}

	// free this handle
	FreeHandle(pHandle);

	return 0;
}

//----------------------------------------------------------------------------
// FsGetDefRootName
//
//----------------------------------------------------------------------------
DLLEXPORT void __stdcall FsGetDefRootName(char* cDefRootName, int iMaxLen)
{
	LOG("FsGetDefRootName() called.\n");
	strncpy(cDefRootName, "Ensoniq Filesystems", iMaxLen);
}

//----------------------------------------------------------------------------
// CopyFile
//
// Copies a file from either Ensoniq disk or DOS file to Ensoniq disk
//
// -> pDestDisk     = pointer to destination disk structure
//    iMode         = COPY_DOS: copy from DOS disk using file pointer *f
//                  = COPY_ENSONIQ: copy from Ensoniq disk using pSourceDisk, 
//                    dwSourceStart
//    f             = open DOS file (if copying from DOS disk)
//    pSourceDisk   = pointer to source disk structure if copying from Ensoniq
//    dwSourceStart = first block of file, the FAT chain will be followed for
//                    reading the source file
//    dwLen         = number of blocks to copy (if EOF is reached before
//                    reaching dwLen, the function returns with no error)
//    dwDestStart   = pointer to a DWORD to receive the start of the newly
//	 			  	  created destination file
//    dwContiguous  = pointer to a DWORD to receive the number of contiguous
//                    blocks in the destination file
//    cLocalName, cRemoteName = filenames for TotalCommander progress report
// <- ERR_OK
//    ERR_NOT_OPEN
//    ERR_OUT_OF_BOUNDS
//    ERR_READ
//    ERR_MEM
//    ERR_NOT_SUPPORTED
//    ERR_SEEK
//    ERR_WRITE
//	  ERR_EOF
//    ERR_DISK_FULL
//    ERR_LOCAL_READ
//    ERR_ABORTED
//----------------------------------------------------------------------------
int CopyEnsoniqFile(DISK *pDestDisk, int iMode, FILE *f, DISK *pSourceDisk, 
	DWORD dwSourceStart, DWORD dwLen, DWORD *dwDestStart, DWORD *dwContiguous,
	char *cLocalName, char *cRemoteName)
{
#define READ_COUNT 64

	unsigned char ucBuf[512*READ_COUNT], ucContiguous = 0;
	DWORD dwStart, dwReadBlock, dwWriteBlock = 0, dwBytesRead, dwBlocks, 
		dwNextBlock;
	int i, j, iEOF, iResult, iContiguousFlag = 1;
	
	dwReadBlock = dwSourceStart;
	
	// try to find a contiguous block of free space for the file
	dwStart = GetContiguousBlocks(pDestDisk, dwLen);
	if(dwStart) ucContiguous = 1;
	
	if(ucContiguous)
	{
		// we got <dwLen> contiguous blocks, file can be written as one chunk
		*dwContiguous = dwLen;
		
		// loop through file
		LOG("Writing %d contiguous blocks: ", dwLen);
	}
	else // non-contiguous file
	{
		// get first single free block
		*dwContiguous = 1; iContiguousFlag = 1;
		dwStart = GetContiguousBlocks(pDestDisk, 1);
		dwWriteBlock = dwStart;
		if(0==dwStart)
		{
			MessageBoxA(0, "The disk is full.", 
				"EnsoniqFS � Warning", MB_ICONWARNING);
			return FS_FILE_WRITEERROR;
		}
		
		// loop through file
		LOG("Writing %d non-contiguous blocks: ", dwLen);
	}
	
	iEOF = 0; i = 0;
	while((i<(int)dwLen)&&(!iEOF))
	{
		dwBlocks = READ_COUNT;
		if((dwBlocks+i)>dwLen) dwBlocks = dwLen - i;
		
		if(COPY_DOS==iMode)
		// read blocks from file handle
		{
			// read blocks
			dwBytesRead = fread(ucBuf, 1, 512*dwBlocks, f);
			if(512*dwBlocks!=dwBytesRead)
			{
				LOG("Error reading local file (block %d-%d, tried to read %d, "
					"got %d).\n", i, i+dwBlocks, dwBlocks, dwBytesRead/512);
				return ERR_LOCAL_READ;
			}
		}
		else if(COPY_ENSONIQ==iMode)

		// read blocks from Ensoniq disk
		{
			for(j=0; j<(int)dwBlocks; j++)
			{
				// read block from Ensoniq device
				iResult = ReadBlock(pSourceDisk, dwReadBlock, ucBuf+j*512);
				if(ERR_OK!=iResult)
				{
					LOG("Error reading block from Ensoniq device.\n");
					return iResult;
				}
				
				// get next block from FAT
				dwReadBlock = GetFATEntry(pSourceDisk, dwReadBlock);
				if(-1==(int)dwReadBlock)
				{
					LOG("Error reading FAT entry.\n");
					return ERR_READ;
				}
				
				// empty block, EOF or bad block?
				if(dwReadBlock<3)
				{
					if((DWORD)(i+j)<(dwLen-1))
					{
						LOG("Error: EOF before dwLen was reached.\n");
					}
					iEOF = 1; break;
				}
			}
		}
		else
		{
			LOG("Error: Unsupported copy mode.\n");
			return ERR_NOT_SUPPORTED;
		}

		if(ucContiguous)
		{		
			// write blocks for contiguous file
			iResult = WriteBlocks(pDestDisk, dwStart+i, dwBlocks, ucBuf);
			if(ERR_OK!=iResult)
			{
				LOG("Error writing Ensoniq file, code=%d.\n", iResult);
				return ERR_WRITE;
			}
		}
		else
		{
			// write non-contiguous blocks
			for(j=0; j<(int)dwBlocks; j++)
			{
				LOG("Writing 1 block at %d: ", dwWriteBlock);
				iResult = WriteBlocks(pDestDisk, dwWriteBlock, 1, ucBuf+j*512);
				if(ERR_OK!=iResult)
				{
					LOG("Error writing Ensoniq file, code=%d.\n", iResult);
					return ERR_WRITE;
				}
				
				// mark this block used so next call to GetContiguousBlocks()
				// won't deliver the same block, as a side effect this block
				// gets set to EOF but will be written with the real value if
				// EOF is not yet reached
				SetFATEntry(pDestDisk, dwWriteBlock, 1);
				
				// if not EOF, allocate a new block
				if((i+j)<(int)(dwLen-1))
				{
					// get next free FAT entry
					dwNextBlock = GetContiguousBlocks(pDestDisk, 1);
					if(0==dwNextBlock)
					{
						// undo changes to FAT
						dwNextBlock = dwStart;
						while(dwNextBlock!=dwWriteBlock)
						{
							// mark block as free
							dwNextBlock = SetFATEntry(pDestDisk, 
								dwNextBlock, 0);
							if((-1==(int)dwNextBlock)||(dwNextBlock<3)) break;
						}
						CacheFlush(pDestDisk);
	
						MessageBoxA(0, "The disk is full or not writable.", 
							"EnsoniqFS � Warning", MB_ICONWARNING);
						return FS_FILE_WRITEERROR;
					}
					
					// set current FAT entry to point to next entry
					SetFATEntry(pDestDisk, dwWriteBlock, dwNextBlock);
					
					// count contiguous blocks
					if((dwNextBlock==(dwWriteBlock+1))&&(0!=iContiguousFlag))
					{
						*dwContiguous++;
					}
					else iContiguousFlag = 0;
					
					dwWriteBlock = dwNextBlock;
				}
			}
			LOG("OK.\n");
		}
		
		// notify TotalCmd of progress, check for user abort
		if(1==g_pProgressProc(g_iPluginNr, cLocalName, cRemoteName, 
							  100*i/dwLen))
		{
			// if user wants to abort, undo FAT changes before returning
			LOG("Aborting, undo FAT changes: ");
			if(!ucContiguous)
			{
				// undo non-contiguous FAT changes only, contiguous FAT changes
				// would be done below, so no need to undo them here
				dwNextBlock = dwStart;
				while(dwNextBlock!=dwWriteBlock)
				{
					// mark block as free
					dwNextBlock = SetFATEntry(pDestDisk, dwNextBlock, 0);
					if((-1==(int)dwNextBlock)||(dwNextBlock<3)) break;
				}
			}
			
			LOG("OK.\n");
			CacheFlush(pDestDisk);
			return ERR_ABORTED;
		}
		
		// increase counter
		i += dwBlocks;
	}
	LOG("OK.\n");

	if(ucContiguous) // write all FAT entries in at once for contiguous files
	{
		// set all FAT entries
		LOG("Writing FAT entries: ");
		for(i=0; i<(int)(dwLen-1); i++)
		{
			if(-1==SetFATEntry(pDestDisk, dwStart+i, dwStart+i+1))
			{
				LOG("Error writing FAT entry %d.\n", dwStart+i);
				return ERR_WRITE;
			}
		}
		if(-1==SetFATEntry(pDestDisk, dwStart+dwLen-1, 1)) // EOF
		{
			LOG("Error writing FAT entry %d.\n", dwStart+dwLen-1);
			return ERR_WRITE;
		}
		LOG("OK.\n");
	}

	// set external pointer to starting block
	*dwDestStart = dwStart;
	return ERR_OK;
}


//----------------------------------------------------------------------------
// AddToImageList
//
// Adds a new image file to the image file list (stored in INI)
//
// -> cName = file name to be added
// <- --
//----------------------------------------------------------------------------
void AddToImageList(char *cName)
{
	INI_LINE *pIniFile = 0, *pLine, *pTempLine = 0;
	int iImageFileCounter = 0;
	char cText[512];
	DWORD dwError;
	HANDLE h;
	
	// open image file for type detection
	LOG("Opening file for type detection: ");
	h = CreateFile(cName, FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL, OPEN_EXISTING, 0, NULL);
	if(INVALID_HANDLE_VALUE==h)
	{
		dwError = GetLastError();
		LOG("failed: "); LOG_ERR(dwError);
		
		MessageBoxA(0, "Could not open file.", "EnsoniqFS � Error", 
			MB_OK | MB_ICONSTOP);
		return;
	}
	LOG("OK.\n");

	// check image file format
	if(IMAGE_FILE_UNKNOWN==DetectImageFileType(h, NULL, NULL, NULL))
	{
		LOG("Image format unknown.\n");
		
		MessageBoxA(0, "The format of the image file could not be "
			"determined. Only the \n"
			"following types are supported:\n\n"
			"    � ISO format (plain disk image)\n"
			"    � BIN format (CDROM Mode1)\n"
			"    � GKH format (Epsread/Epswrite)\n"
			"    � Giebler disk image (EDE, EDA, EDT, EDV)\n\n"
			"Additionally, the image file must contain a valid Ensoniq "
			"file\nsystem for EnsoniqFS to be able to mount it.",
			"EnsoniqFS � Error", MB_ICONSTOP);
		CloseHandle(h);
		return;
	}
	
	CloseHandle(h);

	// open ini file, parse, write new entry, rescan devices
	LOG("Reading INI file: ");
	if(ERR_OK!=ReadIniFile(g_DefaultParams.DefaultIniName, &pIniFile))
	{
		LOG("failed.\n");
	}
	else
	{
		LOG("OK.\n");
	}
	LOG("Parsing...\n");
	
	// try to find section header
	pLine = pIniFile;
	while(pLine)
	{	
		pTempLine = pLine;

		if(0==strncmp("[EnsoniqFS]", pLine->cLine, 11))
		{
			LOG("[EnsoniqFS] found.\n");
			break;
		}
		pLine = pLine->pNext;
	}

	sprintf(cText, "image=%s\n", cName);

	// section not found?
	if(NULL==pLine)
	{
		LOG("[EnsoniqFS] section not found. Creating a new one.\n");
		pLine = InsertIniLine("[EnsoniqFS]\n", pTempLine);
		
		// is this the first line in the file (was the file empty before)?
		if(NULL==pIniFile) pIniFile = pLine;
		
		pLine = InsertIniLine(cText, pLine);
	}
	else
	{
		// skip [EnsoniqFS] section name
		pLine = pTempLine->pNext;

		// parse all lines after [EnsoniqFS], search for current file
		while(pLine)
		{
			// next section found?
			if('['==pLine->cLine[0]) break;
		
			if(0==strncmp(pLine->cLine, "image=", 6)) iImageFileCounter++;	
			if(0==strncmp(pLine->cLine, cText, strlen(cText)))
			{
				MessageBoxA(0, "This file is already in the list of image "
					"files.\nIt can not be mounted twice.",
					"EnsoniqFS � Warning", MB_ICONWARNING);
				pTempLine = NULL;
				break;
			}
			
			pLine = pLine->pNext;
		}
		
		// check counter
		if(iImageFileCounter>MAX_IMAGE_FILES)
		{
			MessageBoxA(0, "The maximum mounted image count has been "
				"reached.\n"
				"Please delete at least one image before adding new ones.",
				"EnsoniqFS � Warning", MB_ICONWARNING);
			pTempLine = NULL;
		}
		
		if(NULL!=pTempLine) InsertIniLine(cText, pTempLine);
	}
	
	LOG("Writing INI file: ");
	if(ERR_OK!=WriteIniFile(g_DefaultParams.DefaultIniName, pIniFile))
	{
		LOG("failed.\n");
	}
	else
	{
		LOG("OK.\n");
	}
	FreeIniLines(pIniFile);
}

//----------------------------------------------------------------------------
// FsPutFile
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsPutFile(char* LocalName, char* RemoteName, 
								  int CopyFlags)
{
	unsigned char ucBuf[512], ucType, *ucCurrentDir, ucMultiFileIndex;
	DWORD dwFilesize, dwStart, dwContiguous;
	char cName[17], cName2[13], cText[512];
	int iResult, i, j, iEntry;
	FIND_HANDLE Handle;
	FILE *f;

	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, LocalName, RemoteName, 0))
		return ERR_ABORTED;

	upcase(RemoteName);

	// isolate path from complete name
	memset(&Handle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(RemoteName); i>0; i--) if('\\'==RemoteName[i]) break;
	strncpy(Handle.cPath, RemoteName, i);

	LOG("\nFsPutFile(local='%s', remote='%s', flags=%d)\n", LocalName,
		RemoteName, CopyFlags);

	// open local file
	LOG("Opening local file: ");	
	f = fopen(LocalName, "rb");
	if(NULL==f)
	{
		LOG("failed.\n");
		return FS_FILE_NOTFOUND;

	}

	// read first block (header of EFE or image file)
	LOG("OK.\nReading header: ");
	if(512!=fread(ucBuf, 1, 512, f))
	{
		LOG("failed.\n");
		fclose(f);
		return FS_FILE_READERROR;
	}
	LOG("OK.\n");
	
//............................................................................
// add image file to list
//............................................................................

/*
	This has been disabled because sometimes it was not possible to mount an
	image (unable to open file, file in use). Source of the problem was not
	found, and since using the options dialog to mount/unmount images is not
	that complicated, this code will stay inactive until further notice.

	// should this file be treated as an image file which is to be added to
	// the image file list?
	if(0==strcmp(Handle.cPath, "\\Image files"))
	{
		if(IDNO==MessageBoxA(0, "This will add the file to the list "
			"of mounted\nimages in EnsoniqFS.\n\n"
			"Do you want to continue?", "EnsoniqFS", MB_ICONQUESTION|MB_YESNO))
		{
			fclose(f);
			return FS_FILE_OK;
		}

		AddToImageList(LocalName);
		
		// notify TotalCmd of progress
		if(1==g_pProgressProc(g_iPluginNr, LocalName, RemoteName, 100))
			return ERR_ABORTED;

		fclose(f);
	
		FreeDiskList(1, g_pDiskListRoot);
		g_pDiskListRoot = ScanDevices(0);
		return FS_FILE_OK;
	}
*/

//............................................................................
// put Ensoniq file to disk
//............................................................................
	// check file type
	ucType = ucBuf[0x32];
	LOG("OK.\nChecking file signature: ");
	if((ucBuf[0x00]!=0x0D) || (ucBuf[0x01]!=0x0A) || (ucType>50))
	{
		LOG("Not an EFE file.\n");
		
		sprintf(cText, "Error: The file type of \"%s\" is not supported.", 
			LocalName);
		MessageBoxA(0, cText, "EnsoniqFS � Warning", MB_ICONWARNING);
		fclose(f);
		return FS_FILE_NOTSUPPORTED;
	}
	
	dwFilesize = (ucBuf[0x34]<<8) + ucBuf[0x35];
	ucMultiFileIndex = ucBuf[0x3A];
	memset(cName, 0, 17); strncpy(cName, ucBuf+0x12, 12);
	while(strlen(cName)<12) strcat(cName, " ");
	LOG("OK, name='%s', type=%d, size=%d blocks.\n", cName, ucType, dwFilesize);

	// read directory structure of RemoteName
	LOG("Reading destination directory: ");
	iResult = ReadDirectoryFromPath(&Handle, 0);
	if(ERR_OK!=iResult)
	{
		LOG("failed.\n");
		return FS_FILE_NOTFOUND;
	}

	// scan directory for file
	LOG("OK.\nChecking if file exists: ");
	iEntry = -1;
	for(i=0; i<39; i++)
	{
		// file found?
		if(0==strncmp(cName, Handle.EnsoniqDir.Entry[i].cName, 12))
		{
			if(ucType==Handle.EnsoniqDir.Entry[i].ucType)
			{
				// if overwriting is requested first try to delete the file
				if(CopyFlags&FS_COPYFLAGS_OVERWRITE)
				{
					// construct name to delete
					strncpy(cName2, cName, 12); cName2[12] = 0;
					for(j=strlen(cName2)-1; j>1; j--)
					{
						if(' '==cName2[j]) cName2[j] = 0;
						else break;
					}
					sprintf(cText, "%s\\%s.[%02i].EFE", Handle.cPath, cName2,
						    Handle.EnsoniqDir.Entry[i].ucType);
					if(!FsDeleteFile(cText))
					{
						fclose(f);
						return FS_FILE_WRITEERROR;
					}

					iEntry = i;
					break;
				}
				else
				{
					// back out
					LOG("Destination file already exists.\n");
					fclose(f);
					return FS_FILE_EXISTS;
				}
			}
		}	
	}
	LOG("no.\n");
	
	// if file doesn't exist try to find the first free entry in directory
	if(-1==iEntry)
	{
		for(i=0; i<39; i++)
		{
			if(FILE_TYPE_EMPTY==Handle.EnsoniqDir.Entry[i].ucType)
			{
				iEntry = i;
				break;
			}
		}
	}
	
	// still no entry found to write to? directory must be full
	if(-1==iEntry)
	{
		LOG("Error. Directory is full.\n");
		MessageBoxA(0, "Directory is full (39 entries maximum).",
					"EnsoniqFS � Warning", MB_ICONWARNING);
		fclose(f);
		return FS_FILE_WRITEERROR;
	}
	
	LOG("Writing file: ");
	
	// do the copy with another function
	iResult = CopyEnsoniqFile(Handle.pDisk, COPY_DOS, f, 0, 0, dwFilesize, 
		&dwStart, &dwContiguous, LocalName, RemoteName);
	fclose(f);
		
	if(ERR_OK!=iResult)
	{
		LOG("Error copying file, CopyEnsoniqFile() returned code %d.\n", 
			iResult);
		CacheFlush(Handle.pDisk);
		return FS_FILE_WRITEERROR;
	}

	// prepare directory entry
	ucCurrentDir = Handle.EnsoniqDir.ucDirectory;
	ucCurrentDir[iEntry*26+1] = ucType;
	memcpy(ucCurrentDir+iEntry*26+2, cName, 12);
	ucCurrentDir[iEntry*26+14] = (dwFilesize>>8)&0xFF;
	ucCurrentDir[iEntry*26+15] = dwFilesize&0xFF;
	ucCurrentDir[iEntry*26+16] = (dwContiguous>>8)&0xFF;
	ucCurrentDir[iEntry*26+17] = dwContiguous&0xFF;
	ucCurrentDir[iEntry*26+18] = (dwStart>>24) & 0xFF;
	ucCurrentDir[iEntry*26+19] = (dwStart>>16) & 0xFF;
	ucCurrentDir[iEntry*26+20] = (dwStart>> 8) & 0xFF;
	ucCurrentDir[iEntry*26+21] = (dwStart    ) & 0xFF;
	ucCurrentDir[iEntry*26+22] = ucMultiFileIndex;
	// write current directory
	LOG("OK.\nWriting current directory: ");
	if(ERR_OK!=WriteBlocks(Handle.pDisk, Handle.EnsoniqDir.dwDirectoryBlock,
						   2, ucCurrentDir))
	{
		LOG("Error writing current directory, code=%d.\n", iResult);
		CacheFlush(Handle.pDisk);
		return FS_FILE_WRITEERROR;
	}
	LOG("OK.\n");
	
	UpdateParentDirectory(Handle.cPath);
	
	// adjust free blocks counter
	AdjustFreeBlocks(Handle.pDisk, dwFilesize);

	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, LocalName, RemoteName, 100))
		return ERR_ABORTED;

	// only flush cache if single file was written
	if(0==g_ucMultiple) CacheFlush(Handle.pDisk);

	LOG("FsPutFile(): OK\n");
	
	return FS_FILE_OK;
}

//----------------------------------------------------------------------------
// FsGetFile
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsGetFile(char* RemoteName, char* LocalName,
								  int CopyFlags, RemoteInfoStruct* ri)
{
	FIND_HANDLE Handle;
	int iResult, i, iEntry;
	char cName[17], cLegalName[17];
	unsigned char ucIsVirtualWaveFile = 0;
	
	upcase(RemoteName);
	
	LOG("FsGetFile(remote='%s', local='%s', flags=%d)\n", RemoteName, LocalName,
		CopyFlags);
	
	// does file exist?
	if(0==_access(LocalName, 0))
	{
		// ask the user to overwrite
		if(0==(CopyFlags&FS_COPYFLAGS_OVERWRITE))
		{
			return FS_FILE_EXISTS;
		}
	}
	
	// isolate path from complete name
	memset(&Handle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(RemoteName); i>0; i--)
	{
		if('\\'==RemoteName[i]) break;
	}
	strncpy(Handle.cPath, RemoteName, i);
	strcpy(cName, RemoteName+i+1);
	
	// read directory structure of RemoteName
	iResult = ReadDirectoryFromPath(&Handle, 0);
	if(ERR_OK!=iResult)
	{
		switch(iResult)
		{
			case ERR_NOT_OPEN:
			case ERR_OUT_OF_BOUNDS:
			case ERR_READ:
			case ERR_MEM:
			case ERR_NOT_SUPPORTED:
				return FS_FILE_READERROR; break;
				
			case ERR_DISK_NOT_FOUND:
			case ERR_PATH_NOT_FOUND:
			case ERR_DIRLEVEL:
			default:
				return FS_FILE_NOTFOUND; break;
		}
	}
	
	// scan directory for desired file
	iEntry = -1;
	LOG("cName=\""); LOG(cName); LOG("\"\n");
	for(i=0; i<39; i++)
	{
		sprintf(cLegalName, "%s.[%02i].EFE",
			Handle.EnsoniqDir.Entry[i].cLegalName,
			Handle.EnsoniqDir.Entry[i].ucType);

		// file found?
		if(0==strcmp(cName, cLegalName))
		{
			iEntry = i; break;
		}
	}
	
	if(-1==iEntry)
	{
		for(i=0; i<58; i++)
		{
			if(Handle.EnsoniqDir.VirtualWaveEntry[i].ucIsStereo)
			{
				sprintf(cLegalName, "%s_STEREO.WAV",
					Handle.EnsoniqDir.VirtualWaveEntry[i].cLegalName);
				
				// file found?
				if(0==strcmp(cName, cLegalName))
				{
					iEntry = i; ucIsVirtualWaveFile = 1; break;
				}
			}
			else
			{
				sprintf(cLegalName, "%s.WAV",
					Handle.EnsoniqDir.VirtualWaveEntry[i].cLegalName);
				
				// file found?
				if(0==strcmp(cName, cLegalName))
				{
					iEntry = i; ucIsVirtualWaveFile = 1; break;
				}
			}
		}
	}
	
	if(-1==iEntry) 
	{
		LOG("Error: file not found.\n");
		return FS_FILE_NOTFOUND;
	}
	
	// read file to local disk
	if(!ucIsVirtualWaveFile)
	{
		iResult = ReadEnsoniqFile(Handle.pDisk, 
			&(Handle.EnsoniqDir.Entry[iEntry]), 
			LocalName, RemoteName);
	}
	else
	{
		iResult = ReadWaveFile(Handle.pDisk, 
			&(Handle.EnsoniqDir.VirtualWaveEntry[iEntry]), 
			LocalName, RemoteName);
	}
	
	if(ERR_OK!=iResult)
	{
		switch(iResult)
		{
			case ERR_NOT_OPEN:
				return FS_FILE_NOTFOUND; break;
				
			case ERR_LOCAL_WRITE:
				return FS_FILE_WRITEERROR; break;

			case ERR_ABORTED:
				return FS_FILE_USERABORT; break;

			case ERR_FAT:
			case ERR_SEEK:
			case ERR_OUT_OF_BOUNDS:
			case ERR_READ:
			case ERR_NOT_SUPPORTED:
			case ERR_MEM:
			default:
				return FS_FILE_READERROR; break;
		}
	}
	
	return FS_FILE_OK;
	
	ri=ri;
}

//----------------------------------------------------------------------------
// FsDeleteFile
//
//----------------------------------------------------------------------------
DLLEXPORT BOOL __stdcall FsDeleteFile(char* RemoteName)
{
	FIND_HANDLE Handle;
	int i, iEntry, iSize;
	char cName[17], cLegalName[17];
	DWORD dwBlock;
	
	upcase(RemoteName);
	LOG("\nFsDeleteFile('%s')\n", RemoteName);

	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, RemoteName, RemoteName, 0))
	{
		CacheFlush(Handle.pDisk);
		return ERR_ABORTED;
	}

	// isolate path from complete name
	memset(&Handle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(RemoteName); i>0; i--) if('\\'==RemoteName[i]) break;
	strncpy(Handle.cPath, RemoteName, i);
	strcpy(cName, RemoteName+i+1);

	// read directory structure of RemoteName
	if(ERR_OK!=ReadDirectoryFromPath(&Handle, 0))
	{
		LOG("Error: Unable to read directory.\n");
		return FALSE;
	}
	
	// scan directory for desired file
	iEntry = -1;
	for(i=0; i<39; i++)
	{
		sprintf(cLegalName, "%s.[%02i].EFE",
			Handle.EnsoniqDir.Entry[i].cLegalName,
			Handle.EnsoniqDir.Entry[i].ucType);

		// file found?
		if(0==strcmp(cName, cLegalName))
		{
			iEntry = i;
			break;
		}
		
	}
	if(-1==iEntry)
	{
		LOG("Error: Could not find file to delete.\n");
		return FALSE;
	}

	LOG("File to delete: iEntry=%d, first block=%d, size=%d\n", iEntry,
		Handle.EnsoniqDir.Entry[iEntry].dwStart, 
		Handle.EnsoniqDir.Entry[iEntry].dwLen);

	// mark FAT entries as empty for this file
	dwBlock = Handle.EnsoniqDir.Entry[iEntry].dwStart;
	iSize = 0;
	while(1)
	{
		dwBlock = SetFATEntry(Handle.pDisk, dwBlock, 0);
		iSize++;
		
		// check error
		if(-1==(int)dwBlock) break;
		
		// hit an empty block -> something is wrong here
		if(0==dwBlock) break;
		
		// regular end of file
		if(1==dwBlock) break;
	}

	// delete file from current directory
	Handle.EnsoniqDir.ucDirectory[iEntry*26+1] = 0;
	if(ERR_OK!=WriteBlocks(Handle.pDisk, Handle.EnsoniqDir.dwDirectoryBlock,
						   2, Handle.EnsoniqDir.ucDirectory))
	{
		LOG("Error: Unable to update current directory.\n");
		CacheFlush(Handle.pDisk);
		return FALSE;
	}

	UpdateParentDirectory(Handle.cPath);
	
	// adjust free blocks counter
	AdjustFreeBlocks(Handle.pDisk, -iSize);
	Handle.pDisk->dwLastFreeFATEntry=Handle.EnsoniqDir.Entry[iEntry].dwStart;
	
	// only flush cache if this was a single file to delete
	if(0==g_ucMultiple) CacheFlush(Handle.pDisk);

	// notify TotalCmd of progress
	if(1==g_pProgressProc(g_iPluginNr, RemoteName, RemoteName, 100))
	{
		CacheFlush(Handle.pDisk);
		return ERR_ABORTED;
	}

	return TRUE;
}

//----------------------------------------------------------------------------
// FsRemoveDir
//
//----------------------------------------------------------------------------
DLLEXPORT BOOL __stdcall FsRemoveDir(char* RemoteName)
{
	unsigned char *ucCurrentDir, *ucDeleteDir;
	FIND_HANDLE Handle, DeleteHandle;
	char cDeleteDir[260];
	int i, iResult;
	DWORD dwDeleteDirBlock = 0;
	
	upcase(RemoteName);
	LOG("\nFsRemoveDir('%s')\n", RemoteName);

	// isolate directory to delete from complete name
	memset(&Handle, 0, sizeof(FIND_HANDLE));
	memset(&DeleteHandle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(RemoteName); i>0; i--) if('\\'==RemoteName[i]) break;
	strncpy(Handle.cPath, RemoteName, i);
	strcpy(cDeleteDir, RemoteName+i+1);
	strcpy(DeleteHandle.cPath, RemoteName);
	
	// limit dir name to 12 chars
	cDeleteDir[12] = 0x00;
	while(strlen(cDeleteDir)<12) strcat(cDeleteDir, " ");

	// read directory to delete
	iResult = ReadDirectoryFromPath(&DeleteHandle, 0);
	if(ERR_OK!=iResult)
	{
		LOG("Error reading directory to delete.\n");
		return FALSE;
	}
	ucDeleteDir = DeleteHandle.EnsoniqDir.ucDirectory;
	
	// check if directory is empty
	for(i=0; i<39; i++)
	{
		// skip parent directory pointer and empty entries
		if(0x08==ucDeleteDir[i*26+1]) continue;
		if(0x00==ucDeleteDir[i*26+1]) continue;

		LOG("Error: Directory not empty.\n");
		return FALSE;
	}

	// read current directory
	iResult = ReadDirectoryFromPath(&Handle, 0);
	if(ERR_OK!=iResult)
	{
		LOG("Error reading current directory.\n");
		return FALSE;
	}
	ucCurrentDir = Handle.EnsoniqDir.ucDirectory;
	
	// find index of directory to delete
	for(i=0; i<39; i++)
	{
		if(0==strncmp(ucCurrentDir+i*26+2, cDeleteDir, 12))
		{
			// skip regular files with same name
			if(0x02!=ucCurrentDir[i*26+1]) continue;

			ucCurrentDir[i*26+1] = 0x00; // mark as unused

			dwDeleteDirBlock = 	(ucCurrentDir[i*26+18]<<24) +
								(ucCurrentDir[i*26+19]<<16) +
								(ucCurrentDir[i*26+20]<<8)  +
								(ucCurrentDir[i*26+21]);
			break;
		}
	}
	if(39==i)
	{
		LOG("Error: Directory name not found in current directory.\n");
		return FALSE;
	}

	// write to FAT
	LOG("Writing FAT entries: ");
	SetFATEntry(Handle.pDisk, dwDeleteDirBlock, 0);
	SetFATEntry(Handle.pDisk, dwDeleteDirBlock+1, 0);
	LOG("OK.\n");

	// update current directory on disk
	LOG("Updating current directory %d: ", Handle.EnsoniqDir.dwDirectoryBlock);
	if(ERR_OK!=WriteBlocks(Handle.pDisk, Handle.EnsoniqDir.dwDirectoryBlock,
						   2, ucCurrentDir))
	{
		LOG("failed.\n");
		CacheFlush(Handle.pDisk);
		return FALSE;
	}
	LOG("OK\n");

	UpdateParentDirectory(Handle.cPath);

	// adjust free blocks counter
	AdjustFreeBlocks(Handle.pDisk, -2);

	// only flush cache if this was a single dir to delete
	if(0==g_ucMultiple) CacheFlush(Handle.pDisk);
	
	Handle.pDisk->dwLastFreeFATEntry = dwDeleteDirBlock;

	return TRUE;
}

//----------------------------------------------------------------------------
// FsMkDir
//
//----------------------------------------------------------------------------
DLLEXPORT BOOL __stdcall FsMkDir(char* Path)
{
	unsigned char *ucCurrentDir, ucNewDir[1024];
	char cNewDir[260], cText[512], cCurrentDir[260];
	FIND_HANDLE Handle;
	int i, iResult, iNextFreeEntryInCurrentDir, iNextFreeBlocks, j;
	
	upcase(Path);
	LOG("\nFsMkDir('%s')\n", Path);

	// isolate new path from complete name
	memset(&Handle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(Path); i>0; i--) if('\\'==Path[i]) break;
	strncpy(Handle.cPath, Path, i);
	strcpy(cNewDir, Path+i+1);
	
	for(i=strlen(Handle.cPath); i>0; i--) if('\\'==Handle.cPath[i]) break;
	strcpy(cCurrentDir, Handle.cPath+i+1);
	cCurrentDir[12] = 0x00;
	while(strlen(cCurrentDir)<12) strcat(cCurrentDir, " ");
	
	
	// limit dir name to 12 chars
	cNewDir[12] = 0x00;
	while(strlen(cNewDir)<12) strcat(cNewDir, " ");

	LOG("Path='%s', Dir='%s'\n", Handle.cPath, cNewDir);

	// read current directory
	iResult = ReadDirectoryFromPath(&Handle, 0);
	if(ERR_OK!=iResult)
	{
		LOG("Error reading current directory.\n");
		return FALSE;
	}
	ucCurrentDir = Handle.EnsoniqDir.ucDirectory;
	
	// check if this name already exists
	for(i=0; i<39; i++)
	{
		if(0==strncmp(cNewDir, Handle.EnsoniqDir.Entry[i].cName, 
			strlen(cNewDir)))
		{
			if(FILE_TYPE_DIRECTORY==Handle.EnsoniqDir.Entry[i].ucType) return FALSE;
		}
	}
	
	// check for next free entry
	iNextFreeEntryInCurrentDir = -1;
	for(i=0; i<39; i++)
	{
		if(FILE_TYPE_EMPTY==ucCurrentDir[i*26+1])
		{
			iNextFreeEntryInCurrentDir = i;
			break;
		}
	}
	if(-1==iNextFreeEntryInCurrentDir)
	{
		LOG("Error: Directory is full.\n");
		sprintf(cText, "Directory is full (39 entries maximum).");
		MessageBoxA(0, cText, "EnsoniqFS � Warning", MB_ICONWARNING);
		return FALSE;
	}

	// set up new directory
	memset(ucNewDir, 0, 1024);
	ucNewDir[1] = 0x08;		// type = parent directory
	
	// name = back to current directory
	for(i=0; i<12; i++) ucNewDir[i+2] = cCurrentDir[i];
	ucNewDir[14] = 0x00;
	ucNewDir[15] = 0x00;	// file size of current dir (does not matter)
	ucNewDir[16] = 0x00;
	ucNewDir[17] = 0x02;	// contiguous blocks of current dir
	
	// pointer to parent directory
	ucNewDir[18] = (Handle.EnsoniqDir.dwDirectoryBlock>>24) & 0xFF;
	ucNewDir[19] = (Handle.EnsoniqDir.dwDirectoryBlock>>16) & 0xFF;
	ucNewDir[20] = (Handle.EnsoniqDir.dwDirectoryBlock>>8)  & 0xFF;
	ucNewDir[21] = (Handle.EnsoniqDir.dwDirectoryBlock)     & 0xFF;

	// signature
	ucNewDir[1022] = 'D';
	ucNewDir[1023] = 'R';
	
	// find two free blocks in the FAT
	iNextFreeBlocks = GetContiguousBlocks(Handle.pDisk, 2);
	if(0==iNextFreeBlocks)
	{
		sprintf(cText, "Error: Disk is full.");
		MessageBoxA(0, cText, "EnsoniqFS � Warning", MB_ICONWARNING);
		return FALSE;
	}

	LOG("Allocated 2 free blocks from FAT, starting with %d\n", 
	iNextFreeBlocks);

	// write to FAT
	LOG("Writing FAT entries: ");
	SetFATEntry(Handle.pDisk, iNextFreeBlocks, iNextFreeBlocks+1);
	SetFATEntry(Handle.pDisk, iNextFreeBlocks+1, 1);
	LOG("OK.\n");

	// write new directory to disk
	LOG("Writing new directory: ");
	if(ERR_OK!=WriteBlocks(Handle.pDisk, iNextFreeBlocks, 2, ucNewDir))
	{
		LOG("failed.\n");
		CacheFlush(Handle.pDisk);
		return FALSE;
	}
	LOG("OK.\n");

	// add directory name to current directory
	j = iNextFreeEntryInCurrentDir*26;
	ucCurrentDir[j+0] = 0x00;
	ucCurrentDir[j+1] = 0x02;	// type = dir
	for(i=0; i<12; i++) ucCurrentDir[j+i+2] = cNewDir[i];
	ucCurrentDir[j+14] = 0x00;	//
	ucCurrentDir[j+15] = 0x00;	// number of files in new directory
	ucCurrentDir[j+16] = 0x00;	//
	ucCurrentDir[j+17] = 0x02;	// contiguous blocks

	// pointer to new directory
	ucCurrentDir[j+18] = (iNextFreeBlocks>>24) & 0xFF;
	ucCurrentDir[j+19] = (iNextFreeBlocks>>16) & 0xFF;
	ucCurrentDir[j+20] = (iNextFreeBlocks>>8)  & 0xFF;
	ucCurrentDir[j+21] = (iNextFreeBlocks)     & 0xFF;

	// update current directory on disk
	LOG("Updating current directory %d: ", Handle.EnsoniqDir.dwDirectoryBlock);
	if(ERR_OK!=WriteBlocks(Handle.pDisk, Handle.EnsoniqDir.dwDirectoryBlock, 2,
						   ucCurrentDir))
	{
		LOG("failed.\n");
		CacheFlush(Handle.pDisk);
		return FALSE;
	}
	LOG("OK\n");

	UpdateParentDirectory(Handle.cPath);

	// adjust free blocks counter
	AdjustFreeBlocks(Handle.pDisk, 2);

	// only flush if this is a single action
	if(0==g_ucMultiple) CacheFlush(Handle.pDisk);
	return TRUE;
}


//----------------------------------------------------------------------------
// FsStatusInfo
//
//----------------------------------------------------------------------------
DLLEXPORT void __stdcall FsStatusInfo(char* RemoteDir, int InfoStartEnd,
									  int InfoOperation)
{
	DISK *pDisk = GetDiskFromPath(RemoteDir);

	LOG("\nFsStatusInfo('%s'): ", RemoteDir);
	if(FS_STATUS_START==InfoStartEnd)
	{
		switch(InfoOperation)
		{
			case FS_STATUS_OP_RENMOV_MULTI:
				LOG("FS_STATUS_OP_RENMOV_MULTI start.\n");
				g_ucMultiple = 1;
				break;
			case FS_STATUS_OP_DELETE:
				LOG("FS_STATUS_OP_DELETE start.\n");
				g_ucMultiple = 1;
				break;
			case FS_STATUS_OP_PUT_MULTI:
				LOG("FS_STATUS_OP_PUT_MULTI start.\n");
				g_ucMultiple = 1;
				break;
			default:
				LOG("other FS_STATUS start: %d (0x%04X)\n", InfoOperation, 
					InfoOperation);
				break;
		}
	}
	else
	{
		switch(InfoOperation)
		{
			case FS_STATUS_OP_DELETE:
				LOG("FS_STATUS_OP_DELETE end.\n");
				g_ucMultiple = 0;
				CacheFlush(pDisk);
				break;
			case FS_STATUS_OP_PUT_MULTI:
				LOG("FS_STATUS_OP_PUT_MULTI end.\n");
				g_ucMultiple = 0;
				CacheFlush(pDisk);
				break;
			case FS_STATUS_OP_RENMOV_MULTI:
				LOG("FS_STATUS_OP_RENMOV_MULTI end.\n");
				g_ucMultiple = 0;
				pDisk = g_pDiskListRoot;
				
				// flush all disks because "move" can write to TWO different disks
				// and FsStatusInfo gives us only one of those disks
				while(pDisk)
				{
					CacheFlush(pDisk);
					pDisk = pDisk->pNext;
				}
				break;
			default:
				LOG("other FS_STATUS end: %d (0x%04X)\n", InfoOperation, 
					InfoOperation);
			break;
		}
	}
	
	return;
}

//----------------------------------------------------------------------------
// FsSetDefaultParams
//
//----------------------------------------------------------------------------
DLLEXPORT void __stdcall FsSetDefaultParams(FsDefaultParamStruct* dps)
{
	char *cName, cValue[3];
	
	LOG("FsSetDefaultParams(): DefaultIniName='%s', "
		"PluginInterfaceVersionHi=%d, PluginInterfaceVersionLow=%d, size=%d\n",
		dps->DefaultIniName, dps->PluginInterfaceVersionHi, 
		dps->PluginInterfaceVersionLow, dps->size);
			
	// save default param struct
	memcpy(&g_DefaultParams, dps, sizeof(FsDefaultParamStruct));
	cName = g_DefaultParams.DefaultIniName;

	// parse INI file
	GetIniValue(cName, "[EnsoniqFS]", "InstallPath", g_cOptionInstallPath, 
		260, "?");
	GetIniValue(cName, "[EnsoniqFS]", "EnableFloppy", cValue, 2, "1");
	g_iOptionEnableFloppy = (cValue[0]=='0')?0:1;
	GetIniValue(cName, "[EnsoniqFS]", "EnableCDROM", cValue, 2, "1");
	g_iOptionEnableCDROM = (cValue[0]=='0')?0:1;
	GetIniValue(cName, "[EnsoniqFS]", "EnablePhysicalDisks", cValue, 2, "1");
	g_iOptionEnablePhysicalDisks = (cValue[0]=='0')?0:1;
	GetIniValue(cName, "[EnsoniqFS]", "EnablePhysicalDisks", cValue, 2, "1");
	g_iOptionEnablePhysicalDisks = (cValue[0]=='0')?0:1;
	GetIniValue(cName, "[EnsoniqFS]", "EnableImages", cValue, 2, "1");
	g_iOptionEnableImages = (cValue[0]=='0')?0:1;
	GetIniValue(cName, "[EnsoniqFS]", "AutomaticRescan", cValue, 2, "1");
	g_iOptionAutomaticRescan = (cValue[0]=='0')?0:1;
	GetIniValue(cName, "[EnsoniqFS]", "EnableLogging", cValue, 2, "0");
	g_iOptionEnableLogging = (cValue[0]=='0')?0:1;
}

//----------------------------------------------------------------------------
// FsRenMovFile
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall FsRenMovFile(char* OldName, char* NewName, BOOL Move,
	BOOL OverWrite, RemoteInfoStruct* ri)
{
	unsigned char ucType, *ucOldDir, *ucNewDir, ucMultiFileIndex;
	FIND_HANDLE OldHandle, NewHandle;
	char cOldName[260], cNewName[260], cEOldName[260], cENewName[260], 
		cTemp[260];
	int i, iOldEntry, iNewEntry, iCopyAndDeleteSource = 0, iNewAdjust = 0,
		iOldAdjust = 0, iResult;
	DWORD dwContiguous, dwNewStart, dwFilesize;

	upcase(OldName); upcase(NewName);
	LOG("FsRenMovFile('%s', '%s', %d, %d)\n", OldName, NewName, Move, 
		OverWrite);
	ri = ri; // this is to get rid of the warning "unused parameter"
	
	if(0==strcmp(OldName, NewName)) return FS_FILE_OK;

	if(1==g_pProgressProc(g_iPluginNr, OldName, NewName, 0))
		return ERR_ABORTED;
	
	// isolate path and filename/directory from complete name
	// old dir
	memset(&OldHandle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(OldName); i>0; i--) if('\\'==OldName[i]) break;
	strncpy(OldHandle.cPath, OldName, i);
	strcpy(cOldName, OldName+i+1);
	strcpy(cEOldName, cOldName);
	cEOldName[strlen(cEOldName)-9] = 0; // cut off ".[xx].EFE"
	cEOldName[13] = 0; // limit the size to 12 chars
	while(strlen(cEOldName)<12) strcat(cEOldName, " "); // fill up with spaces

	// new dir
	memset(&NewHandle, 0, sizeof(FIND_HANDLE));
	for(i=strlen(NewName); i>0; i--) if('\\'==NewName[i]) break;
	strncpy(NewHandle.cPath, NewName, i);
	strcpy(cNewName, NewName+i+1);
	strcpy(cENewName, cNewName);
	cENewName[strlen(cNewName)-9] = 0; // cut off ".[xx].EFE"
	cENewName[13] = 0; // limit the size to 12 chars
	while(strlen(cENewName)<12) strcat(cENewName, " "); // fill up with spaces
	
	// read directory structure of OldHandle
	if(ERR_OK!=ReadDirectoryFromPath(&OldHandle, 0))
	{
		LOG("Error: Unable to read old directory.\n");
		return FS_FILE_NOTFOUND;
	}
	ucOldDir = OldHandle.EnsoniqDir.ucDirectory;
	
	// read directory structure of NewHandle
	if(ERR_OK!=ReadDirectoryFromPath(&NewHandle, 0))
	{
		LOG("Error: Unable to read new directory.\n");
		return FS_FILE_NOTFOUND;
	}
	ucNewDir = NewHandle.EnsoniqDir.ucDirectory;
	
	// scan old directory for desired entry
	iOldEntry = -1;
	for(i=0; i<39; i++)
	{
		sprintf(cTemp, "%s.[%02i].EFE",
			OldHandle.EnsoniqDir.Entry[i].cLegalName,
			OldHandle.EnsoniqDir.Entry[i].ucType);

		// file found?
		if(0==strcmp(cOldName, cTemp))
		{
			iOldEntry = i;
			ucMultiFileIndex = OldHandle.EnsoniqDir.Entry[i].ucMultiFileIndex;
			ucType = OldHandle.EnsoniqDir.Entry[i].ucType;
			dwFilesize = OldHandle.EnsoniqDir.Entry[i].dwLen;
			break;
		}	
	}
	if(-1==iOldEntry)
	{
		LOG("Error: Could not find source file.\n");
		return FS_FILE_NOTFOUND;
	}

	// scan new directory for desired entry
	iNewEntry = -1;
	for(i=0; i<39; i++)
	{
		sprintf(cTemp, "%s.[%02i].EFE",
			NewHandle.EnsoniqDir.Entry[i].cLegalName,
			NewHandle.EnsoniqDir.Entry[i].ucType);

		// file found?
		if(0==strcmp(cNewName, cTemp))
		{
			iNewEntry = i;
			break;
		}	
	}
	if(-1!=iNewEntry)
	{
		// return if file exists and OverWrite==FALSE
		LOG("File exists.\n");
		if(!OverWrite) return FS_FILE_EXISTS;
		else
		{
			// delete file (cached)
			g_ucMultiple++;
			if(FALSE==FsDeleteFile(NewName))
			{
				g_ucMultiple--;
				return FS_FILE_WRITEERROR;
			}
			g_ucMultiple--;
		}
	}
	else
	{
		// search for free entry
		for(i=0; i<39; i++)
		{
			if(FILE_TYPE_EMPTY==NewHandle.EnsoniqDir.Entry[i].ucType)
			{
				iNewEntry = i;
				break;
			}
		}
	}
	if(-1==iNewEntry)
	{
		LOG("Error. Directory is full.\n");
		MessageBoxA(0, "Directory is full (39 entries maximum).",
					"EnsoniqFS � Warning", MB_ICONWARNING);
		return FS_FILE_WRITEERROR;
	}

	// if we come to here, the variables are set to the following values:
	//   OldHandle = structure with decoded directory content and with
	//               directory blocks from disk in buffer (source directory)
	//   cOldName  = name of the existing file including ".[xx].EFE"
	//   cEOldName = name in Ensoniq format (without extension, filled up with
	//               spaces to length of 12)
	//   ucOldDir  = pointer to directory blocks from disk
	//   iOldEntry = index of source file in directory [0..38]
	//
	//   NewHandle = structure with decoded directory content and with
	//               directory blocks from disk in buffer (dest directory)
	//   cNewName  = name of the target file including ".[xx].EFE"
	//   cENewName = name in Ensoniq format (without extension, filled up with
	//               spaces to length of 12)
	//   ucNewDir  = pointer to directory blocks from disk
	//   iNewEntry = index of destination file in directory [0..38]; either
	//               points to the next free location or to the already
	//               deleted (overwritten) entry

	LOG("OldPath='%s', OldName='%s', NewPath='%s', NewName='%s'\n", 
		OldHandle.cPath, cOldName, NewHandle.cPath, cNewName);
	
	if(Move) // move file within EnsoniqFS
	{
		
		// move on same disk? -> move only directory pointers (fast!)
		if(OldHandle.pDisk==NewHandle.pDisk)
		{
			LOG("Moving directory entry: ");
			
			// copy directory entry
			memcpy(ucNewDir + iNewEntry*26, ucOldDir + iOldEntry*26, 26);
			
			// delete old entry
			ucOldDir[iOldEntry*26+1] = 0;
			
			// write old directory
			if(ERR_OK!=WriteBlocks(OldHandle.pDisk, 
				OldHandle.EnsoniqDir.dwDirectoryBlock, 2, ucOldDir))
			{
				LOG("Error writing directory blocks.\n");
				CacheFlush(OldHandle.pDisk);
				CacheFlush(NewHandle.pDisk);
				return FS_FILE_WRITEERROR;
			}

			// write new directory
			if(ERR_OK!=WriteBlocks(NewHandle.pDisk, 
				NewHandle.EnsoniqDir.dwDirectoryBlock, 2, ucNewDir))
			{
				LOG("Error writing directory blocks.\n");
				CacheFlush(OldHandle.pDisk);
				CacheFlush(NewHandle.pDisk);
				return FS_FILE_WRITEERROR;
			}

			iCopyAndDeleteSource = 0;
			LOG("OK.\n");
		}
		else
		{
			// copy and delete, will be executed below in the "copy" function
			iCopyAndDeleteSource = 1;
		}
	}
	
	if((0==Move)||(1==iCopyAndDeleteSource))
	// copy file within EnsoniqFS or move file between two disks
	{
		iResult = CopyEnsoniqFile(NewHandle.pDisk, COPY_ENSONIQ, 0, 
			OldHandle.pDisk, OldHandle.EnsoniqDir.Entry[iOldEntry].dwStart,
			OldHandle.EnsoniqDir.Entry[iOldEntry].dwLen, &dwNewStart, 
			&dwContiguous, cOldName, cNewName);

		if(ERR_OK!=iResult)
		{
			LOG("Error copying file, CopyEnsoniqFile() returned code %d.\n", 
				iResult);
			CacheFlush(OldHandle.pDisk);
			CacheFlush(NewHandle.pDisk);
			return FS_FILE_WRITEERROR;
		}
		
		// delete source file if necessary (cached)
		if(1==iCopyAndDeleteSource)
		{
			g_ucMultiple++;
			if(FALSE==FsDeleteFile(OldName))
			{
				g_ucMultiple--;
				CacheFlush(OldHandle.pDisk);
				CacheFlush(NewHandle.pDisk);
				return FS_FILE_WRITEERROR;
			}
			g_ucMultiple--;

			iOldAdjust = -OldHandle.EnsoniqDir.Entry[iOldEntry].dwLen;
		}
		iNewAdjust = OldHandle.EnsoniqDir.Entry[iOldEntry].dwLen;
		
		// prepare directory entry
		ucNewDir[iNewEntry*26+1] = ucType;
		memcpy(ucNewDir+iNewEntry*26+2, cENewName, 12);
		ucNewDir[iNewEntry*26+14] = (dwFilesize>>8)&0xFF;
		ucNewDir[iNewEntry*26+15] = dwFilesize&0xFF;
		ucNewDir[iNewEntry*26+16] = (dwContiguous>>8)&0xFF;
		ucNewDir[iNewEntry*26+17] = dwContiguous&0xFF;
		ucNewDir[iNewEntry*26+18] = (dwNewStart>>24) & 0xFF;
		ucNewDir[iNewEntry*26+19] = (dwNewStart>>16) & 0xFF;
		ucNewDir[iNewEntry*26+20] = (dwNewStart>> 8) & 0xFF;
		ucNewDir[iNewEntry*26+21] = (dwNewStart    ) & 0xFF;
		ucNewDir[iNewEntry*26+21] = ucMultiFileIndex;
		
		// write new directory
		if(ERR_OK!=WriteBlocks(NewHandle.pDisk, 
			NewHandle.EnsoniqDir.dwDirectoryBlock, 2, ucNewDir))
		{
			LOG("Error writing directory blocks.\n");
			CacheFlush(OldHandle.pDisk);
			CacheFlush(NewHandle.pDisk);
			return FS_FILE_WRITEERROR;
		}
	}

	// update parent directories only if source and dest are not the same
	if(NewHandle.EnsoniqDir.dwDirectoryBlock!=
		OldHandle.EnsoniqDir.dwDirectoryBlock)
	{
		UpdateParentDirectory(OldHandle.cPath);
		UpdateParentDirectory(NewHandle.cPath);
	}

	// Adjust free space
	if(iNewAdjust) AdjustFreeBlocks(NewHandle.pDisk, iNewAdjust);
	if(iOldAdjust) AdjustFreeBlocks(OldHandle.pDisk, iOldAdjust);
	
	// flush caches
	if(0==g_ucMultiple) 
	{
		CacheFlush(OldHandle.pDisk);
		if(OldHandle.pDisk!=NewHandle.pDisk)
		{
			CacheFlush(NewHandle.pDisk);
		}
	}
	
	if(1==g_pProgressProc(g_iPluginNr, OldName, NewName, 100))
		return ERR_ABORTED;
	
	return FS_FILE_OK;
}

//----------------------------------------------------------------------------
// GetUsageCount
//
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall GetUsageCount(void)
{
	if(g_ucSharedMemory!=NULL)
	{
		LOG("GetUsageCount() returns %d\n", g_ucSharedMemory[0]); 
		return g_ucSharedMemory[0];
	}
	else return 0;
}

//----------------------------------------------------------------------------
// Cleanup
//
// -> --
// <- --
//----------------------------------------------------------------------------
void Cleanup(void)
{
	FIND_HANDLE *pHandle, *pTemp;
	
	// loop through all open handles
	if(NULL!=g_pHandleRoot)
	{
		// start with first handle
		pHandle = g_pHandleRoot;
		
		// loop through all handles
		while(pHandle)
		{
			pTemp = pHandle->pNext;
			FreeHandle(pHandle);
			pHandle = pTemp;
		}	
	}
	g_pHandleRoot = NULL;
	
	FreeDiskList(0, g_pDiskListRoot);
}

//----------------------------------------------------------------------------
// DllMain
//
// ->
// <-
//----------------------------------------------------------------------------
BOOL APIENTRY DllMain (HINSTANCE hInst /*Library instance handle.*/,
                       DWORD reason    /*Reason why function is being called.*/,
                       LPVOID reserved /*Not used.*/ )
{
	DWORD dwError, dwInitSharedMemory = 0;
	
    switch(reason)
    {
		case DLL_PROCESS_ATTACH:
			LOG("DllMain() DLL_PROCESS_ATTACH called.\n");
			g_hInst = hInst;

			// create memory mapped file
			LOG("Creating memory mapped file: ");
			g_hMemoryMappedFile = CreateFileMapping(
				INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 
				SHARED_MEMORY_SIZE, g_cMemoryFN);
			
			// check result
			dwError = GetLastError();
			if(NULL==g_hMemoryMappedFile)
			{
				LOG("failed ("); LOG_ERR(dwError); LOG(").\n");
				break;
			}
			
			LOG("OK.\n");
			
			// is this the first instance?
			if(dwError!=ERROR_ALREADY_EXISTS)
			{
				dwInitSharedMemory = 1;
				LOG("This is the first instance.\n");
			}
			
			// get the buffer
			g_ucSharedMemory = MapViewOfFile(g_hMemoryMappedFile, 
				FILE_MAP_ALL_ACCESS, 0, 0, 0);
			
			// check result
			LOG("Mapping file: ");
			dwError = GetLastError();
			if(NULL==g_ucSharedMemory)
			{
				CloseHandle(g_hMemoryMappedFile);
				LOG("failed ("); LOG_ERR(dwError); LOG(").\n");
				break;
			}

			LOG("OK.\n");

			// if this is the first instance, delete shared memory block
			if(1==dwInitSharedMemory)
			{
				memset(g_ucSharedMemory, 0, SHARED_MEMORY_SIZE);
			}
			
			// increase reference counter
			g_ucSharedMemory[0]++;

	        break;

		case DLL_PROCESS_DETACH:
			// free all memory
			LOG("DllMain() DLL_PROCESS_DETACH called.\n");
			Cleanup();
			
			// decrease reference counter
			g_ucSharedMemory[0]--;
			
			LOG("Unmapping file.\n");
			UnmapViewOfFile(g_ucSharedMemory);
			CloseHandle(g_hMemoryMappedFile);
						
	        break;

		case DLL_THREAD_ATTACH:
    	    break;

      	case DLL_THREAD_DETACH:
        	break;
    }

    /* Returns TRUE on success, FALSE on failure */
    return TRUE;

	// this is just to get rid of compiler warning "unused parameter" for this
	// function (I do not want to disable this warning for the whole project)
	reserved = reserved;
}
