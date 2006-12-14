//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// DISK READ AND WRITE CACHE FUNCTIONS
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
#include "cache.h"
#include "disk.h"

//----------------------------------------------------------------------------
// CacheReadBlock
// 
// Try to read a block from cache. If found, the block is refreshed so it
// will stay in cache longer.
//
// -> pDisk = pointer to valid disk structure
//    dwBlock = block to read
//    ucBuf = pointer to buffer to receive the content of requested block
// <- ERR_OK
//    ERR_NOT_IN_CACHE
//----------------------------------------------------------------------------
int CacheReadBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf)
{
	int i;

	pDisk->dwReadCounter++;

	// check if block is in cache
	for(i=0; i<CACHE_SIZE; i++)
	{
		if(dwBlock==pDisk->dwCacheTable[i])
		{
			memcpy(ucBuf, pDisk->ucCache + i*512, 512);
			
			// refresh this block
			pDisk->dwCacheAge[i] = pDisk->dwReadCounter;
			pDisk->dwCacheHits++;
			return ERR_OK;
		}
	}
	
	pDisk->dwCacheMisses++;
	return ERR_NOT_IN_CACHE;
}

//----------------------------------------------------------------------------
// CacheInsertReadBlock
// 
// Insert a freshly read block into Cache (read cache entry). The oldest
// block in the cache is overwritten.
//
// -> pDisk = pointer to valid disk structure
//    dwBlock = block to insert
//    ucBuf = pointer to buffer to be inserted
// <- ERR_OK
//----------------------------------------------------------------------------
int CacheInsertReadBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf)
{
	DWORD dwOldestBlock, dwOldestAge;
	int i;

	// check if block is already in cache
	for(i=0; i<CACHE_SIZE; i++)
	{
		if(dwBlock==pDisk->dwCacheTable[i])
		{
			// overwrite block if not marked as dirty
			if(0==(pDisk->ucCacheFlags[i]&CACHE_FLAG_DIRTY))
			{
				memcpy(pDisk->ucCache + i*512, ucBuf, 512);
			}

			// mark block as new
			pDisk->dwCacheAge[i] = pDisk->dwReadCounter;
			return ERR_OK;
		}
	}
	
	// if we come to here, block is not in cache and has to be inserted

	// find the oldest non-dirty block to overwrite it
	dwOldestBlock = 0; dwOldestAge = pDisk->dwReadCounter+1;
	for(i=0; i<CACHE_SIZE; i++)
	{
		if(pDisk->dwCacheAge[i]<dwOldestAge)
		{
			dwOldestAge = pDisk->dwCacheAge[i];
			dwOldestBlock = i;
		}
	}

	// copy new block over oldest block		
	pDisk->dwCacheTable[dwOldestBlock] = dwBlock;
	pDisk->dwCacheAge[dwOldestBlock] = pDisk->dwReadCounter;
	memcpy(pDisk->ucCache + dwOldestBlock*512, ucBuf, 512);

	return ERR_OK;
}

//----------------------------------------------------------------------------
// CacheWriteBlock
// 
// Writes a block to the write cache. The new block gets a "dirty" tag and
// should be written with a CacheFlush() afterwards, otherwise it will be
// lost. If 3/4 of the cache blocks (CACHE_SIZE*3/4) are marked "dirty", a
// CacheFlush() is automatically issued.
//
// -> pDisk = pointer to valid disk structure
//    dwBlock = block to write
//    ucBuf = pointer to buffer to be written
// <- ERR_OK
//    ERR_SEEK
//    ERR_WRITE
//    ERR_NOT_OPEN
//    ERR_MEM
//----------------------------------------------------------------------------
int CacheWriteBlock(DISK *pDisk, DWORD dwBlock, unsigned char *ucBuf)
{
	DWORD dwOldestBlock, dwOldestAge;
	int i, iDirtyBlocks = 0, iResult;
	
	// count dirty blocks
	for(i=0; i<CACHE_SIZE; i++)
	{
		if(pDisk->ucCacheFlags[i]&CACHE_FLAG_DIRTY) iDirtyBlocks++;
	}
	
	// flush cache, if necessary
	if(iDirtyBlocks>=(CACHE_SIZE*3/4))
	{
		iResult = CacheFlush(pDisk);
		if(ERR_OK!=iResult) return iResult;
	}
	
	// search for block in cache; if found, overwrite it and mark it dirty
	for(i=0; i<CACHE_SIZE; i++)
	{
		// current block already in cache?
		if(pDisk->dwCacheTable[i]==dwBlock)
		{
			// refresh cache entry
			pDisk->dwCacheAge[i] = pDisk->dwReadCounter;
			memcpy(pDisk->ucCache + i*512, ucBuf, 512);
			pDisk->ucCacheFlags[i] |= CACHE_FLAG_DIRTY;
			return ERR_OK;
		}
	}

	// find the oldest block to overwrite it
	dwOldestBlock = 0; dwOldestAge = pDisk->dwReadCounter+1;
	for(i=0; i<CACHE_SIZE; i++)
	{
		// skip dirty cache blocks
		if(pDisk->ucCacheFlags[i]&CACHE_FLAG_DIRTY) continue;

		if(pDisk->dwCacheAge[i]<dwOldestAge)
		{
			dwOldestAge = pDisk->dwCacheAge[i];
			dwOldestBlock = i;
		}
	}
	
	// copy block to cache
	pDisk->dwCacheTable[dwOldestBlock] = dwBlock;
	pDisk->dwCacheAge[dwOldestBlock] = pDisk->dwReadCounter;
	memcpy(pDisk->ucCache + dwOldestBlock*512, ucBuf, 512);

	// tag this block as dirty
	pDisk->ucCacheFlags[dwOldestBlock] |= CACHE_FLAG_DIRTY;

	return ERR_OK;
}

//----------------------------------------------------------------------------
// CacheFlush
// 
// Flush the write cache to disk. Find all dirty cache entries and write them
// back to disk. Try to find contiguous blocks to be written in one write
// call.
//
// The read cache is not affected (cached sectors stay cached)
//
// -> pDisk = pointer to valid disk structure
// <- ERR_OK
//    ERR_SEEK
//    ERR_WRITE
//    ERR_NOT_OPEN
//    ERR_MEM
//----------------------------------------------------------------------------
DLLEXPORT int __stdcall CacheFlush(DISK *pDisk)
{
	DWORD dwBytesWritten, dwTemp, dwCacheTable[CACHE_SIZE], 
		dwCacheAge[CACHE_SIZE], dwLow, dwHigh, dwError;
	unsigned char *ucCache, ucCacheFlags[CACHE_SIZE];
	__int64 iiFilepointer;
	int i, j, k;
	
	if(NULL==pDisk) return ERR_NOT_OPEN;
	
	// check if there is something to write
	for(i=0; i<CACHE_SIZE; i++)
	{
		if(0!=(pDisk->ucCacheFlags[i]&CACHE_FLAG_DIRTY)) break;
	}
	if(CACHE_SIZE==i) return ERR_OK;
	
	LOG("CacheFlush(): Sorting cache ");

	// TODO: sorting is slow!
	memcpy(dwCacheTable, pDisk->dwCacheTable, CACHE_SIZE*sizeof(DWORD));
	ucCache = malloc(CACHE_SIZE*512);
	if(NULL==ucCache)
	{
		LOG("failed, ERR_MEM.\n");
		return ERR_MEM;
	}

	// sort cache in ascending order (block number)
	for(i=0; i<CACHE_SIZE; i++)
	{
		for(j=i+1; j<CACHE_SIZE; j++)
		{
			if(dwCacheTable[i]>dwCacheTable[j])
			{
				// swap two cache entries
				dwTemp = dwCacheTable[i];
				dwCacheTable[i] = dwCacheTable[j];
				dwCacheTable[j] = dwTemp;
			}
		}
	}
	
	// copy the cache blocks to temp locations (this didn't happen in the
	// above sorting function because bubblesort is too slow for that)
	for(i=0; i<CACHE_SIZE; i++)
	{
		for(j=0; j<CACHE_SIZE; j++)
		{
			if(dwCacheTable[i]==pDisk->dwCacheTable[j])
			{
				memcpy(ucCache+i*512, pDisk->ucCache+j*512, 512);
				dwCacheAge[i] = pDisk->dwCacheAge[j];
				ucCacheFlags[i] = pDisk->ucCacheFlags[j];
				
				break;
			}
		}
	}

	// copy temp cache structure to cache
	free(pDisk->ucCache); pDisk->ucCache = ucCache;
	memcpy(pDisk->dwCacheAge, dwCacheAge, CACHE_SIZE*sizeof(DWORD));
	memcpy(pDisk->dwCacheTable, dwCacheTable, CACHE_SIZE*sizeof(DWORD));
	memcpy(pDisk->ucCacheFlags, ucCacheFlags, CACHE_SIZE);

	LOG("OK.\n");
	
	// loop through all cache entries
	for(i=0; i<CACHE_SIZE; i++)
	{
		// skip non-dirty blocks
		if(0==(pDisk->ucCacheFlags[i]&CACHE_FLAG_DIRTY)) continue;

		// set file pointer
		iiFilepointer = pDisk->dwCacheTable[i]; iiFilepointer *= 512;
		dwLow = iiFilepointer & 0xFFFFFFFF;
		dwHigh = (iiFilepointer >> 32) & 0xFFFFFFFF;
		if(0xFFFFFFFF==SetFilePointer(pDisk->hHandle, dwLow, &dwHigh, 
			FILE_BEGIN))
		{
			dwError = GetLastError();
			if(NO_ERROR != dwError)
			{
				LOG("CacheFlush() failed: ERR_SEEK: "); LOG_ERR(dwError);
				return ERR_SEEK;
			}
		}

		// try to find contiguous blocks
		j = 1;
		while((i+j)<=CACHE_SIZE)
		{
			// only check dirty blocks
			if(pDisk->ucCacheFlags[i+j]&CACHE_FLAG_DIRTY)
			{
				// allow only blocks where the successor points to next block
				if((pDisk->dwCacheTable[i+j-1]+1)==(pDisk->dwCacheTable[i+j]))
				{
					j++;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}

		// write to disk
		if(0==WriteFile(pDisk->hHandle, pDisk->ucCache+i*512, 512*j, 
			&dwBytesWritten, NULL))
		{
			LOG("CacheFlush(): WriteFile() failed.\n");
			return ERR_WRITE;
		}
		if((DWORD)(512*j)!=dwBytesWritten)
		{
			LOG("CacheFlush(): WriteFile() failed.\n");
			return ERR_WRITE;
		}

		LOG("  -> "); LOG_INT(j); LOG(" contiguous blocks written at block ");
		LOG_INT(pDisk->dwCacheTable[i]); LOG(".\n");
		
		// mark cache blocks as non-dirty
		for(k=0; k<j; k++) pDisk->ucCacheFlags[i+k] = CACHE_FLAG_NONE;
		
		i += j - 1;
	}

	LOG("OK, cache hits="); LOG_INT(pDisk->dwCacheHits);
	LOG(", cache misses="); LOG_INT(pDisk->dwCacheMisses);
	LOG(", FAT hits="); LOG_INT(pDisk->dwFATHit);
	LOG(", FAT misses="); LOG_INT(pDisk->dwFATMiss);
	LOG("\n");
	
	return ERR_OK;
}
