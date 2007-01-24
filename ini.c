//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// INI FILE FUNCTIONS
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
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>
#include "log.h"
#include "error.h"
#include "ini.h"

//----------------------------------------------------------------------------
// ReadIniFile
// 
// Read INI file into an INI_LINE linked list
// 
// -> cFN = name of INI file
// <- ERR_OK
//    ERR_NOT_FOUND
//    ERR_MEM
//    pFirstLine = pointer to first entry of linked list
//----------------------------------------------------------------------------
int ReadIniFile(char *cFN, INI_LINE **pFirstLine)
{
	char cLine[65536], cStop = 0;
	INI_LINE *pLine = 0;
	FILE *f;
	
	*pFirstLine = NULL;
	
	// check if file exists
	if(0==_access(cFN, 0))
	{
		strcpy(cLine, cFN);
	}
	else
	{
		// try to use "system directory\fsplugin.ini" as alternative
		// (this is the case when EnsoniqFS.wfx is loaded with ETools)
		GetWindowsDirectory(cLine, 512);
		strcat(cLine, "\\fsplugin.ini");
	}
	
	// open file
	f = fopen(cLine, "r");
	if(NULL==f) return ERR_NOT_FOUND;
	
	while((!feof(f))||(!cStop))
	{
		if(NULL==fgets(cLine, 65536, f))
		{
			cStop = 1;
		}
		else
		{
			pLine = InsertIniLine(cLine, pLine);
			if(NULL==pLine)
			{
				LOG("InsertIniLine() failed.\n");
				fclose(f);
				return ERR_MEM;
			}
			if(NULL==*pFirstLine) *pFirstLine = pLine;
		}
	}
	
	fclose(f);
	
	return ERR_OK;
}

//----------------------------------------------------------------------------
// WriteIniFile
// 
// Write an INI_LINE linked list to INI file
// 
// -> cFN = name of INI file
//    pFirstLine = pointer to first entry of linked list
// <- ERR_OK
//    ERR_LOCAL_WRITE
//    ERR_NOT_FOUND
//----------------------------------------------------------------------------
int WriteIniFile(char *cFN, INI_LINE *pFirstLine)
{
	INI_LINE *pLine;
	FILE *f;
	
	// open file
	f = fopen(cFN, "w");
	if(NULL==f) return ERR_NOT_FOUND;

	pLine = pFirstLine;
	while(pLine)
	{
		fprintf(f, "%s", pLine->cLine);
		pLine = pLine->pNext;
	}
	
	fclose(f);
	return ERR_OK;
}

//----------------------------------------------------------------------------
// FreeIniLines
// 
// Free memory allocated by an INI_LINE linked list
// 
// -> pFirstLine = pointer to linked list
// <- --
//----------------------------------------------------------------------------
void FreeIniLines(INI_LINE *pFirstLine)
{
	INI_LINE *pTemp, *pLine;
	
	pLine = pFirstLine;
	while(pLine)
	{
		if(pLine->cLine) free(pLine->cLine);
		pTemp = pLine->pNext;
		free(pLine);
		pLine = pTemp;
	}
}

//----------------------------------------------------------------------------
// InsertIniLine
// 
// Inserts a new line into INI_LINE linked list
// 
// -> cLine = text line to insert
//    pInsertAfter = pointer to INI_LINE after which to insert the new line
// <- pointer to newly created entry
//----------------------------------------------------------------------------
INI_LINE* InsertIniLine(char *cLine, INI_LINE *pInsertAfter)
{
	INI_LINE *pLine;
	
	// allocate new INI_LINE
	pLine = malloc(sizeof(INI_LINE));
	if(NULL==pLine) return NULL;
	memset(pLine, 0, sizeof(INI_LINE));
	
	// allocate text buffer
	pLine->cLine = malloc(strlen(cLine)+1);
	if(NULL==pLine->cLine) return NULL;
	
	// copy text
	strcpy(pLine->cLine, cLine);
	
	// set pointer to following
	if(NULL!=pInsertAfter) pLine->pNext = pInsertAfter->pNext;
	
	// set pointer to previous
	pLine->pPrevious = pInsertAfter;
	
	// set pointer to self
	if(NULL!=pInsertAfter) pInsertAfter->pNext = pLine;
	return pLine;
}

//----------------------------------------------------------------------------
// DeleteIniLine
// 
// Deletes one INI_LINE from linked list
// 
// -> pDeleteLine = pointer to INI_LINE to delete
// <- --
//----------------------------------------------------------------------------
void DeleteIniLine(INI_LINE *pDeleteLine)
{
	if(NULL==pDeleteLine) return;
	
	// if previous exists, let previous->next point to current->next
	if(pDeleteLine->pPrevious)
		pDeleteLine->pPrevious->pNext = pDeleteLine->pNext;

	// if next exitst, let next->previous point to current->previous		
	if(pDeleteLine->pNext)
		pDeleteLine->pNext->pPrevious = pDeleteLine->pPrevious;
		
	// delete text buffer and INI_LINE
	if(pDeleteLine->cLine) free(pDeleteLine->cLine);
	free(pDeleteLine);
}

//----------------------------------------------------------------------------
// SetIniValue
// 
// Sets the value of an INI file entry.
//
// INI file structure is as follows:
// [section_name]
// key1=value1
// key2=value2
// ...
// 
// -> cName    = pointer to name of INI file
//    cSection = name of section to look for (including "[" and "]")
//               if not found, section will be created, must not be NULL
//    cKey     = name of the key to look for (without "=")
//               if not found, key will be created, must not be NULL
//    cValue   = content of the key, must not be NULL
// <- ERR_OK
//    ERR_SET_INI
//----------------------------------------------------------------------------
int SetIniValue(char *cName, char *cSection, char *cKey, char *cValue)
{
	INI_LINE *pLine = NULL, *pSection = NULL, *pIniFile = NULL;
	char *cLine;

	// check pointers
	if(NULL==cName) return ERR_SET_INI;
	if(NULL==cSection) return ERR_SET_INI;
	if(NULL==cKey) return ERR_SET_INI;
	if(NULL==cValue) return ERR_SET_INI;

	// read INI file, change values, write back INI file
	LOG("Reading INI file... ");
	if(ERR_OK!=ReadIniFile(cName, &pIniFile))
	{
		LOG("failed.\n");
		return ERR_SET_INI;
	}

	// try to find section header
	pLine = pIniFile;
	while(pLine)
	{	
		pSection = pLine;
		if(0==strncmp(cSection, pLine->cLine, strlen(cSection))) break;
		pLine = pLine->pNext;
	}

	// section not found?
	if(NULL==pLine)
	{
		LOG("Section not found, creating a new one.\n");
		pLine = InsertIniLine(cSection, pSection);
		
		if(NULL==pLine)
		{
			LOG("InsertIniLine() failed.\n");
			FreeIniLines(pIniFile);
			return ERR_SET_INI;
		}
	}
	
	// skip section name, continue with next line
	pSection = pLine;
	pLine = pLine->pNext;

	// allocate new line with length = cKey + "=" + cValue + "\n" + "\0"
	cLine = malloc(strlen(cKey) + strlen(cValue) + 3);
	if(NULL==cLine)
	{
		LOG("Allocating new line failed.\n");
		FreeIniLines(pIniFile);
		return ERR_SET_INI;
	}
	
	sprintf(cLine, "%s=%s\n", cKey, cValue);
	
	// parse all lines after [section name]
	while(pLine)
	{
		// next section found?
		if('['==pLine->cLine[0]) break;
		
		// look for "key="
		if(0==strncmp(cLine, pLine->cLine, strlen(cKey)+1))
		{
			// free old content, assign new content
			free(pLine->cLine);
			pLine->cLine = cLine;

			LOG("Writing INI file.\n");
			WriteIniFile(cName, pIniFile);
			FreeIniLines(pIniFile);

			return ERR_OK;			
		}
		pLine = pLine->pNext;
	}
	
	// if we come to here, pSection points to [section name], but the key has
	// not been found -> create new key

	// insert new line after pSection
	pLine = InsertIniLine(cLine, pSection);
	free(cLine);
	if(NULL==pLine)
	{
		LOG("InsertIniLine() failed.\n");
		FreeIniLines(pIniFile);
		return ERR_SET_INI;
	}

	LOG("Writing INI file.\n");
	WriteIniFile(cName, pIniFile);
	FreeIniLines(pIniFile);
	
	return ERR_OK;	
}

//----------------------------------------------------------------------------
// SetIniValueInt
// 
// Sets the value of an INI file entry to a specified integer value.
//
// -> cName    = pointer to name of INI file
//    cSection = name of section to look for (including "[" and "]")
//               if not found, section will be created, must not be NULL
//    cKey     = name of the key to look for (without "=")
//               if not found, key will be created, must not be NULL
//    iValue   = content of the key
// <- ERR_OK
//    ERR_SET_INI
//----------------------------------------------------------------------------
int SetIniValueInt(char *cName, char *cSection, char *cKey, int iValue)
{
	char cValue[512];
	sprintf(cValue, "%i", iValue);
	return SetIniValue(cName, cSection, cKey, cValue);
}

//----------------------------------------------------------------------------
// GetIniValue
// 
// Gets the value of an INI file entry.
//
// -> cName    = pointer to name of INI file
//    cSection = name of section to look for (including "[" and "]")
//               if not found, function returns cDefault
//    cKey     = name of the key to look for (without "=")
//               if not found, function returns cDefault
//    cValue   = buffer to receive the key (size must be iMaxLen+1)
//    iMaxLen  = maximum string length of the destination buffer
//               for string of length iMaxLen=n buffer must be n+1 bytes!
// <- ERR_OK
//    ERR_GET_INI
//----------------------------------------------------------------------------
int GetIniValue(char *cName, char *cSection, char *cKey, char *cValue,
				 int iMaxLen, char *cDefault)
{
	INI_LINE *pLine = NULL, *pSection = NULL, *pIniFile = NULL;
	char *cLine;
	int iKeyLen;

	// check pointers
	if(NULL==cName) return ERR_GET_INI;
	if(NULL==cSection) return ERR_GET_INI;
	if(NULL==cKey) return ERR_GET_INI;
	if(NULL==cValue) return ERR_GET_INI;
	if(iMaxLen<0) iMaxLen = 0;
	
	// read INI file
	LOG("Reading INI file... ");
	if(ERR_OK!=ReadIniFile(cName, &pIniFile))
	{
		LOG("failed.\n");
		return ERR_GET_INI;
	}

	// fill buffer with zeroes
	memset(cValue, 0, iMaxLen+1);	

	// try to find section header
	pLine = pIniFile;
	while(pLine)
	{	
		pSection = pLine;
		if(0==strncmp(cSection, pLine->cLine, strlen(cSection))) break;
		pLine = pLine->pNext;
	}

	// section not found? -> return default value
	if(NULL==pLine)
	{
		LOG("Section not found, returning default value.\n");
		strncpy(cValue, cDefault, iMaxLen);
		FreeIniLines(pIniFile);
		return ERR_OK;
	}
	
	// skip section name, continue with next line
	pSection = pLine;
	pLine = pLine->pNext;

	// parse all lines after [section name]
	iKeyLen = strlen(cKey) + 1;
	cLine = malloc(iKeyLen+1);
	if(NULL==cLine)
	{
		FreeIniLines(pIniFile);
		return ERR_GET_INI;
	}
	sprintf(cLine, "%s=", cKey);

	while(pLine)
	{
		// next section found? -> return default value
		if('['==pLine->cLine[0])
		{
			break;
		}
		
		// look for "key="
		if(0==strncmp(cLine, pLine->cLine, iKeyLen))
		{
			strncpy(cValue, pLine->cLine + iKeyLen, 
				strlen(pLine->cLine) - iKeyLen - 1);
			free(cLine);
			FreeIniLines(pIniFile);
			return ERR_OK;			
		}
		pLine = pLine->pNext;
	}
	
	// if we come to here, the key has not been found
	LOG("Key not found, returning default value.\n");
	strncpy(cValue, cDefault, iMaxLen);
	free(cLine);
	FreeIniLines(pIniFile);
	return ERR_OK;
}
