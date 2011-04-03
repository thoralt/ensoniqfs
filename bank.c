

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

#include "ensoniqfs.h"

#include "error.h"

#include "bank.h"


//#define LOG_BANK_ADAPT


// parameters for bank adaption
extern int g_iOptionBankDevice;


POST_PROCESS_ITEM *g_pPostProcessItemRoot = NULL;

INDEX_LIST_PAIR *g_pIndexListTranslationRoot = NULL;


INDEX_LIST_PAIR *createIndexListTranslationItem()
{
	static INDEX_LIST_PAIR *pLastItem = NULL;
	
    // create new entry
	INDEX_LIST_PAIR *pItem = malloc(sizeof(INDEX_LIST_PAIR));
	if(NULL==pItem)
	{
		LOG("Unable to allocate new INDEX_LIST_PAIR structure.\n");
		return 0;
	}
	
	// init
	memset(&(pItem->SourceIndexList), 0, sizeof(ITEM_INDEX_LIST));
	memset(&(pItem->TargetIndexList), 0, sizeof(ITEM_INDEX_LIST));
	pItem->pNext = NULL;
	
	if(0==g_pIndexListTranslationRoot)
	{
		g_pIndexListTranslationRoot = pItem;
	}
	else
	{
		pLastItem->pNext = pItem;
	}
	pLastItem = pItem;
	return pItem;
}

void freeIndexListTranslation( void )
{
	INDEX_LIST_PAIR *pItem = g_pIndexListTranslationRoot;

	while(pItem)
	{
		INDEX_LIST_PAIR *pTemp = pItem->pNext;
		free(pItem);
		pItem = pTemp;
	}
	
	g_pIndexListTranslationRoot = 0;
}

void appendIndex( ITEM_INDEX_LIST *pIndexListItem, unsigned char indexEntry )
{
	int found = 0;
	int i;
	for(i=0; i<12; i++)
	{
		if ( pIndexListItem->index[ i ] == 0 )
		{
			//LOG("appendIndex: dir level '%d', index '%d'\n", i, indexEntry);
			pIndexListItem->index[ i ] = indexEntry;
			found = 1;
			break;
        }
	}
	if ( found == 0 )
	{
	   // no free entry found -> invalidate the complete entry
	   memset( pIndexListItem, 0xff, sizeof( ITEM_INDEX_LIST ) );
	}
}

int lengthIndexList( ITEM_INDEX_LIST *pIndexListItem )
{
 	// invalid index list contains only 0xff entries -> length -1
	// test only the first one entry
 	if (pIndexListItem->index[ 0 ] == 0xff)
 	   return -1;

 	// index list is always terminated by an entry 0
	int i;
	for(i=0; i<12; i++)
	{
		if ( pIndexListItem->index[ i ] == 0 )
		{
			return i;
        }
	}
	// full index list 
	return 12;
}


ITEM_INDEX_LIST* getTargetIndexList( ITEM_INDEX_LIST *pSourceIndexList )
{
	// Gets an index list (usually from a FIB of a bank) and searches
	// the matching entry in the translation list. If found, returns
	// the corresponding target index list.
	// This is tricky:
	// The translation table contains e.g. (only three indices in this example)
	//     2 | 2 | 0  -> 2 | 1 | 3
  // The Ensoniq bank references a file with the index list (2nd dir., 2nd file)
	//     2 | 2 | 12
  //   Please note the additional garbage after the terminal node (the file).
	// The Ensoniqs seems not to terminate the list with a special value. While
	// loading a bank, they terminate their search for a file when the
	// current index is not a directory.
	// Since every source index list in the translation table points to a
	// terminal node (i.e. not a directory), we can get the "length" of this
	// index list (these index lists are terminate with the
	// value 0 when not filled completely with 12 entries).
	// The comparison with the Ensoniq's index list is done only for the
	// first n bytes.
	INDEX_LIST_PAIR *pItem = g_pIndexListTranslationRoot;

	while(pItem)
	{
		int sourceIndexListLength = lengthIndexList( &(pItem->SourceIndexList) );
		if ( -1 == sourceIndexListLength )
		{
			// invalid index list, go to next entry
			pItem = pItem->pNext;
			continue;
		}

		if ( memcmp(&(pItem->SourceIndexList), pSourceIndexList, sourceIndexListLength) == 0 )
			return &(pItem->TargetIndexList);
		
		pItem = pItem->pNext;
	}
	return NULL;
}

void addItemToPostProcess( char* cMsDosName )
{
	static POST_PROCESS_ITEM *pLastItem = NULL;
	
	// create new disk structure
	POST_PROCESS_ITEM *pItem = malloc(sizeof(POST_PROCESS_ITEM));
	if(NULL==pItem)
	{
		LOG("Unable to allocate new POST_PROCESS_ITEM structure.\n");
		return;
	}
	
	// init
    strcpy(pItem->cMsDosName, cMsDosName);
	pItem->pNext = NULL;
	
	if(0==g_pPostProcessItemRoot)
	{
		g_pPostProcessItemRoot = pItem;
	}
	else
	{
		pLastItem->pNext = pItem;
	}
	pLastItem = pItem;
}

void freePostProcessItems( void )
{
	POST_PROCESS_ITEM *pItem = g_pPostProcessItemRoot;

	while(pItem)
	{
		POST_PROCESS_ITEM *pTemp = pItem->pNext;
		free(pItem);
		pItem = pTemp;
	}
	
	g_pPostProcessItemRoot = 0;
}

int isBankFile( unsigned char type )
{
	if ( ( FILE_TYPE_EPS_BANK == type )
		|| ( FILE_TYPE_EPS16_BANK == type )
		|| ( FILE_TYPE_ASR_BANK == type )
	)
		return 1;
	else
		return 0;
}

int getBankType( unsigned char* pData )
{
	unsigned int bankType = pData[4] + (pData[5] << 8) + (pData[6] << 16) + (pData[7] << 24);
	// observed values for EPS banks: 0xc0ff4089, 0xb0ff40c9, 0xc0ff2079, 0xc0ff807b
	// do detection only by second byte (0xff)
	if ( (bankType & 0x00ff0000) == 0x00ff0000)
		return BANK_TYPE_EPS;
	else if (bankType == 0x000020a8)
		return BANK_TYPE_EPS16;
	else if (bankType == 0x0000c034)
		return BANK_TYPE_ASR;
	
	LOG("getBankType: Unknown machine ID '0x%08X' found. Please report this number to the developers.\n", bankType);	 
	return BANK_TYPE_UNKNOWN;
}

unsigned int getFIBLength( unsigned char* pData )
{
	int bankType = getBankType( pData );
 	if ( (BANK_TYPE_EPS == bankType) || (BANK_TYPE_EPS16 == bankType) )
		return 16;
    else if (BANK_TYPE_ASR == bankType)
    	return 28;
   	else
   		return 0;
}

unsigned char getValidTrackMask( unsigned char* pData )
{
	return pData[0x0020];
}

int getInstrumentCopyFlag( unsigned char* pData, unsigned char trackId, unsigned char* pCopyFlag )
{
 	if (pData == NULL) return ERR_ABORTED;
 	if (trackId > 8) return ERR_ABORTED;
	if (pCopyFlag == NULL) return ERR_ABORTED;
	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   		
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;
	
	//LOG("instrument copy byte: 0x%02X\n", pData[ fibOffset + 0x00 ]);
	// highest bit indicates if the instrument is a copy
	if( (0x80 & pData[ fibOffset + 0x00 ]) == 0x80)
		*pCopyFlag = 1;
	else
		*pCopyFlag = 0;
	
	return ERR_OK;
}

int getDiskLabel( unsigned char* pData, unsigned char trackId, unsigned char* pName )
{
 	if (pData == NULL) return ERR_ABORTED;
 	if (trackId > 8) return ERR_ABORTED;
	if (pName == NULL) return ERR_ABORTED;
	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   		
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;
	pName[0] = pData[ fibOffset + 0x03 ];
	pName[1] = pData[ fibOffset + 0x05 ];
	pName[2] = pData[ fibOffset + 0x07 ];
	pName[3] = pData[ fibOffset + 0x09 ];
	pName[4] = pData[ fibOffset + 0x0b ];
	pName[5] = pData[ fibOffset + 0x0d ];
	pName[6] = pData[ fibOffset + 0x0f ];
	
	return ERR_OK;
}

int setDiskLabel( unsigned char* pData, unsigned char trackId, unsigned char* pName )
{
 	if (pData == NULL) return ERR_ABORTED;
 	if (trackId > 8) return ERR_ABORTED;
 	if (pName == NULL) return ERR_ABORTED;
	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   		
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;
	pData[ fibOffset + 0x03 ] = pName[0];
	pData[ fibOffset + 0x05 ] = pName[1];
	pData[ fibOffset + 0x07 ] = pName[2];
	pData[ fibOffset + 0x09 ] = pName[3];
	pData[ fibOffset + 0x0b ] = pName[4];
	pData[ fibOffset + 0x0d ] = pName[5];
	pData[ fibOffset + 0x0f ] = pName[6];
	
	return ERR_OK;
}

int getDeviceId( unsigned char* pData, unsigned char trackId, unsigned char* pDeviceId )
{
 	if (pData == NULL) return ERR_ABORTED;
	if (trackId > 8) return ERR_ABORTED;
 	if (pDeviceId == NULL) return ERR_ABORTED;
	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   		
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;
	*pDeviceId = pData[ fibOffset + 0x02 ];
	
	return ERR_OK;
}

int setDeviceId( unsigned char* pData, unsigned char trackId, unsigned char deviceId )
{
 	if (pData == NULL) return ERR_ABORTED;
	if (trackId > 8) return ERR_ABORTED;
 	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   		
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;
	pData[ fibOffset + 0x02 ] = deviceId;
	
	return ERR_OK;
}

int getIndexList( unsigned char* pData, unsigned char trackId, ITEM_INDEX_LIST *pIndexList )
{
 	if (pData == NULL) return ERR_ABORTED;
	if (trackId > 8) return ERR_ABORTED;
 	if (pIndexList == NULL) return ERR_ABORTED;
 	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   	
	memset(pIndexList, 0, sizeof(ITEM_INDEX_LIST));
	
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;
	
	// ASR bank has longer list of file pointers
	unsigned char lastFilePointer = (28 == fibLength) ? 11 : 5;
	unsigned char filePointer;
	for( filePointer = 0; filePointer <= lastFilePointer; filePointer++)
	{
		pIndexList->index[ filePointer ] = pData[ fibOffset + 0x04 + filePointer*2 ];
		// do not read any further after the first zero, there might be garbage following
		if ( 0 == pData[ fibOffset + 0x04 + filePointer*2 ] )
			break;
	}
	return ERR_OK;
}

int setIndexList( unsigned char* pData, unsigned char trackId, ITEM_INDEX_LIST *pIndexList )
{
 	if (pData == NULL) return ERR_ABORTED;
	if (trackId > 8) return ERR_ABORTED;
 	if (pIndexList == NULL) return ERR_ABORTED;
 	
	unsigned int fibLength = getFIBLength( pData );
 	if ( 0 == fibLength )
   		return ERR_ABORTED;
   		
 	// FIB starts at 0x0022
	unsigned int fibOffset = 0x0022 + trackId*fibLength;

	// ASR bank has longer list of file pointers
	unsigned char lastFilePointer = (28 == fibLength) ? 11 : 5;
	unsigned char filePointer;
	for( filePointer = 0; filePointer <= lastFilePointer; filePointer++)
	{
		pData[ fibOffset + 0x04 + filePointer*2 ] = pIndexList->index[ filePointer ];
	}

	return ERR_OK;
}



// based on ReadEnsoniqFile:
//
//----------------------------------------------------------------------------
// ReadEnsoniqBankFile
// 
// Reads a bank file using the FAT to a memory location
// 
// -> pDisk = pointer to disk to use
//    pDirEntry = pointer to directory entry describing the Ensoniq file
//    handleBankData = handle to memory (allocated in this function) to receive data of Ensoniq file
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
int ReadEnsoniqBankFile(DISK *pDisk, ENSONIQDIRENTRY *pDirEntry, unsigned char **handleBankData)
{
	int iResult;
	DWORD dwBlock, dwSize, dwBlockCounter = 0;

	// check pointers
	if(NULL==pDisk) return ERR_NOT_OPEN;
	if(NULL==pDirEntry) return ERR_NOT_OPEN;


	if ( isBankFile( pDirEntry->ucType ) == 0 )
	{
		LOG("ReadEnsoniqBankFile: file is not a bank. File type is '%d'.\n", pDirEntry->ucType);
		return ERR_NOT_OPEN;
	}
	
	LOG("Reading Ensoniq bank file (Start=%d, size=%d blocks, type=%d"
		"\n", pDirEntry->dwStart, pDirEntry->dwLen,
		pDirEntry->ucType);
	

	
	// use length from directory entry as size
	dwSize = pDirEntry->dwLen;

	// set starting block
	dwBlock = pDirEntry->dwStart;
		
	// allocate memory for bank file
	unsigned char *pBankData = malloc( dwSize * 512 );
	

	LOG("ReadEnsoniqBankFile: Extracting file... ");
	
	// loop through all blocks
	while(dwBlockCounter<dwSize)
	{
		// read next block from disk image
		iResult = ReadBlock(pDisk, dwBlock, pBankData + dwBlockCounter*512);
		if(ERR_OK!=iResult)
		{
			LOG("ReadEnsoniqBankFile: Error reading block, code=%d.\n", iResult);
			return iResult;
		}
		
		// get next FAT entry
		dwBlock = GetFATEntry(pDisk, dwBlock);

		// either end of file reached or error getting FAT entry occured
		if(dwBlock<3)
		{
			LOG("ReadEnsoniqBankFile: FAT entry=%d, Len=%d - stopping.\n", dwBlock, 
				pDirEntry->dwLen);
			break; // stop here even if file is too short
		}
		
		dwBlockCounter++;
		
	}
	
	LOG("OK.\n");
	// deliver pointer to read bank file to caller
	*handleBankData = pBankData;
		
	return ERR_OK;
	
}

//----------------------------------------------------------------------------
// RewriteEnsoniqBankFile
// 
// rewrites an already existing bank file using the FAT from a memory location
// note: this requires that the file existed already on the same disk on the same location.
// this should only be used when changes are made that do not modify the total length of the file 
// 
// -> pDisk = pointer to disk to use
//    pDirEntry = pointer to directory entry describing the Ensoniq file
//    pBankData =  data of Ensoniq file
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
int RewriteEnsoniqBankFile(DISK *pDisk, ENSONIQDIRENTRY *pDirEntry, unsigned char *pBankData)
{
	int iResult;
	DWORD dwBlock, dwSize, dwBlockCounter = 0;

	// check pointers
	if(NULL==pDisk) return ERR_NOT_OPEN;
	if(NULL==pDirEntry) return ERR_NOT_OPEN;
	if(NULL==pBankData) return ERR_NOT_OPEN;


	if ( isBankFile( pDirEntry->ucType ) == 0 )
	{
		LOG("RewriteEnsoniqBankFile: file is not a bank. File type is '%d'.\n", pDirEntry->ucType);
		return ERR_NOT_OPEN;
	}
	
	LOG("Rewriting Ensoniq bank file (Start=%d, size=%d blocks, type=%d"
		"\n", pDirEntry->dwStart, pDirEntry->dwLen,
		pDirEntry->ucType);
	

	
	// use length from directory entry as size
	dwSize = pDirEntry->dwLen;

	// set starting block
	dwBlock = pDirEntry->dwStart;
		

	LOG("RewritingEnsoniqBankFile: Saving file... ");
	
	// loop through all blocks
	while(dwBlockCounter<dwSize)
	{
		// write next block to disk image
		iResult = WriteBlocks( pDisk, dwBlock, 1, pBankData + dwBlockCounter*512);
		if(ERR_OK!=iResult)
		{
			LOG("RewriteEnsoniqBankFile: Error writing block, code=%d.\n", iResult);
			return iResult;
		}
		
		// get next FAT entry
		dwBlock = GetFATEntry(pDisk, dwBlock);

		// either end of file reached or error getting FAT entry occured
		if(dwBlock<3)
		{
			LOG("RewriteEnsoniqBankFile: FAT entry=%d, Len=%d - stopping.\n", dwBlock, 
				pDirEntry->dwLen);
			break; // stop here even if file is too short
		}
		
		dwBlockCounter++;
		
	}
	
	LOG("OK.\n");
		
	return ERR_OK;
	
}


void adaptBank( unsigned char* pBankData, unsigned char* targetDiskLabel )
{
	int iResult;
 	
	// get track mask of instrument tracks 
	unsigned char instrumentTrackMask = getValidTrackMask( pBankData );
	LOG("adaptBank: track mask 0x%02X\n", instrumentTrackMask);
	
	unsigned char trackId;
	// 8 instrument tracks and 1 song
	for (trackId = 0; trackId < 9; trackId++)
	{
	 	// handle instrument tracks (first eight tracks)
	 	if (trackId < 8)
	 	{
			unsigned char trackBit = 1 << trackId;
			//LOG("adaptBank: track bit 0x%02X\n", trackBit);
			
			// is current track in instrument track mask? no -> try next track
			if ( (trackBit & instrumentTrackMask) != trackBit )
				continue;
		}
		
		
		// has current track an instrument copy? yes -> try next track
		unsigned char copyFlag;
		iResult = getInstrumentCopyFlag( pBankData, trackId, &copyFlag );
		if (ERR_OK != iResult)
			continue;
		if (copyFlag == 1)
			continue;
		
		if (trackId < 8)
			LOG("adaptBank: processing instrument track: '%d'\n", trackId+1);
		else
			LOG("adaptBank: processing song entry\n");
		
		// now we have a valid track of the bank which is not a copy
		unsigned char diskLabel[8];
		memset( &diskLabel, 0, sizeof(diskLabel) );
		iResult = getDiskLabel( pBankData, trackId, diskLabel );
		if (ERR_OK != iResult)
			continue;
		
#ifdef LOG_BANK_ADAPT
		LOG("adaptBank: disk label: '%s'\n", diskLabel);
#endif		
		
		unsigned char deviceId;
		iResult = getDeviceId( pBankData, trackId, &deviceId );
		if (ERR_OK != iResult)
			continue;
		
#ifdef LOG_BANK_ADAPT
		LOG("adaptBank: device id: '%d'\n", deviceId);
#endif		
		
		ITEM_INDEX_LIST sourceIndexList;
		iResult = getIndexList( pBankData, trackId, &sourceIndexList );
		if (ERR_OK != iResult)
			continue;
		
#ifdef LOG_BANK_ADAPT
		LOG("adaptBank: source index list: ");
		int index;
		for (index = 0; index < 12; index++)
		{
			LOG("0x%02X ", sourceIndexList.index[index]);
		}
		LOG("\n");
#endif		
		
		// ////////////// //
		// set new values //
		// ////////////// //
		
		// set index list

		ITEM_INDEX_LIST *pTargetIndexList = NULL;

	 	// translate index list
	 	pTargetIndexList = getTargetIndexList( &sourceIndexList );

#ifdef LOG_BANK_ADAPT
		LOG("adaptBank: target index list: ");
		if ( pTargetIndexList != NULL )
		{
			int index;
			for (index = 0; index < 12; index++)
			{
				LOG("0x%02X ", pTargetIndexList->index[index]);
			}
		}
		LOG("\n");
#endif		

		if ( pTargetIndexList == NULL )
		{
		   LOG("adaptBank: error: no target index list found!\n");
		   continue;
		}
		
		// found a matching target list -> then set target index list
		
		// check size of target index list
		int lengthTargetIndexList = lengthIndexList( pTargetIndexList );
		if ( -1 == lengthTargetIndexList )
		{
		   // invalid target index list
		   LOG("adaptBank: error: detected invalid target index list!\n");
		   continue;
		}
		
		// check if EPS / EPS16 bank can handle the index list 
		// note: ASR can always handle index lists (with max. 12 entries, otherwise the index list would be invalid)
		int bankType = getBankType( pBankData );
		if ( ( (BANK_TYPE_EPS == bankType) || (BANK_TYPE_EPS16 == bankType) )
			&& ( lengthTargetIndexList > 6) )
		{
			LOG("adaptBank: error: detected invalid target index list!\n");
			continue;
		}
			
		iResult = setIndexList( pBankData, trackId, pTargetIndexList );
		if (ERR_OK != iResult)
		{
			LOG("adaptBank: error: setting index list not successful!\n");
			continue;
		}
		
		
		// set disk label:
		iResult = setDiskLabel( pBankData, trackId, targetDiskLabel );
		if (ERR_OK != iResult)
		{
			LOG("adaptBank: error: setting disk name not successful!\n");
			continue;
		}
				
		
		// set device id:
		iResult = setDeviceId( pBankData, trackId, g_iOptionBankDevice );
		if (ERR_OK != iResult)
		{
			LOG("adaptBank: error: setting device id not successful!\n");
			continue;
		}
		
	} // for (unsigned char trackId = 0; trackId < 9; trackId++)
}

void doPostProcess( void )
{

	POST_PROCESS_ITEM *pItem = g_pPostProcessItemRoot;

	while(pItem)
	{
		LOG( "doPostProcess: Processing item '%s'.\n", pItem->cMsDosName);

		// get handle to file
		upcase(pItem->cMsDosName);
		// isolate path and filename/directory from complete name
		// dir
		int i;
		char cName[260], cTemp[260];
		unsigned char ucType, *ucDir, ucMultiFileIndex;
		DWORD dwFilesize;
		FIND_HANDLE FileHandle;
		
		memset(&FileHandle, 0, sizeof(FIND_HANDLE));
		for(i=strlen(pItem->cMsDosName); i>0; i--) if('\\'==pItem->cMsDosName[i]) break;
		strncpy(FileHandle.cPath, pItem->cMsDosName, i);
		strcpy(cName, pItem->cMsDosName+i+1);

		// read directory structure of FileHandle
		if(ERR_OK!=ReadDirectoryFromPath(&FileHandle, 0))
		{
			LOG("Error: Unable to read directory.\n");
			// TODO: return FS_FILE_NOTFOUND;
			break;
		}
		ucDir = FileHandle.EnsoniqDir.ucDirectory;
		
		//LOG("doPostProcess: diskLabel: '%s'\n", FileHandle.pDisk->cDiskLabel);
	
		// scan directory for desired entry
		int iEntry = -1;
		for(i=0; i<39; i++)
		{
			sprintf(cTemp, "%s.[%02i].EFE",
				FileHandle.EnsoniqDir.Entry[i].cLegalName,
				FileHandle.EnsoniqDir.Entry[i].ucType);

			// file found?
			if(0==strcmp(cName, cTemp))
			{
				iEntry = i;
				ucMultiFileIndex = FileHandle.EnsoniqDir.Entry[i].ucMultiFileIndex;
				ucType = FileHandle.EnsoniqDir.Entry[i].ucType;
				dwFilesize = FileHandle.EnsoniqDir.Entry[i].dwLen;
				break;
			}	
		}
		if(-1==iEntry)
		{
			LOG("Error: Could not find file.\n");
			// TODO : return FS_FILE_NOTFOUND;
			break;
		}

		unsigned char* pBankData = NULL;
		// read bank data
		int iResult = ReadEnsoniqBankFile(FileHandle.pDisk, 
			&(FileHandle.EnsoniqDir.Entry[iEntry]), 
			&pBankData);
		
		if ( (iResult != ERR_OK) || (pBankData == NULL) )
		{
			LOG("doPostProcess: Error: Could not read Ensoniq bank file.\n");
			pItem = pItem->pNext;
			continue;
		}

	  	// adapt bank data, pass on label of target disk
		adaptBank( pBankData, FileHandle.pDisk->cDiskLabel );
		
		// write translated bank data
		// note: we write the file at its original position on the disk since its size has not changed!
		iResult = RewriteEnsoniqBankFile(FileHandle.pDisk, &(FileHandle.EnsoniqDir.Entry[iEntry]), pBankData);
		if (iResult != ERR_OK)
		{
			LOG("doPostProcess: Error: Could not write Ensoniq bank file.\n");
		}
		
		
		pItem = pItem->pNext;
	} // while(pItem)
}


// very similar to ReadDirectoryFromPath
int GetIndexListFromPath(FIND_HANDLE *pHandle, ITEM_INDEX_LIST *pIndexList)
{
	char cCurrentPath[260], cPath[260], cNextDir[260];
	DWORD dwDirectoryBlock;
	int i, iResult, j, iDirLevel;
	DISK *pDisk;

	upcase(pHandle->cPath);
	
	LOG("GetIndexListFromPath(\""); LOG(pHandle->cPath);
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
		iResult = ReadDirectory(pDisk, &pHandle->EnsoniqDir, 0);
		if(ERR_OK!=iResult)
		{
			LOG("GetIndexListFromPath: Error reading directory blocks, code=%d\n", iResult);
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
			LOG("GetIndexListFromPath(): failed, ERR_PATH_NOT_FOUND\n");
			return ERR_PATH_NOT_FOUND;
		}
		
		//LOG("GetIndexListFromPath: dir level '%d', directory index '%d'\n", iDirLevel, i);
		if (iDirLevel <= 12)
			pIndexList->index[ iDirLevel - 1 ] = i;
		else if (iDirLevel == 13)
		{
			// index list can only store maximum of 12 entries
			// invalidate the index list when more than 12 levels are found
			memset( pIndexList, 0xff, sizeof( ITEM_INDEX_LIST ) );
		}
	}
	if(iDirLevel>=256)
	{
		LOG("Error: Directory nesting more than 255 levels deep.\n");
		return ERR_DIRLEVEL;
	}
	
	return ERR_OK;
}
