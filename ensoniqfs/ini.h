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
#ifndef _INI_H_
#define _INI_H_

//----------------------------------------------------------------------------
// struct to contain an INI file
//----------------------------------------------------------------------------
typedef struct _INI_LINE
{
	char *cLine;
	struct _INI_LINE *pPrevious, *pNext;
} INI_LINE;

//----------------------------------------------------------------------------
// Prototypes
//----------------------------------------------------------------------------
int ReadIniFile(char *cFN, INI_LINE **pFirstLine);
int WriteIniFile(char *cFN, INI_LINE *pFirstLine);
INI_LINE* InsertIniLine(char *cLine, INI_LINE *pInsertAfter);
void DeleteIniLine(INI_LINE *pDeleteLine);
void FreeIniLines(INI_LINE *pFirstLine);
int SetIniValue(char *cName, char *cSection, char *cKey, char *cValue);
int SetIniValueInt(char *cName, char *cSection, char *cKey, int iValue);
int GetIniValue(char *cName, char *cSection, char *cKey, char *cValue,
	int iMaxLen, char *cDefault);

#endif
