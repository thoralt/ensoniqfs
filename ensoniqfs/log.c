//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// LOGGING FUNCTIONS
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
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "log.h"

#ifdef LOGGING

//----------------------------------------------------------------------------
// LOG
// 
// Append entry to logfile
// logfile is created if it doesn't exist
// 
// -> *c = pointer to null terminated string to append to log file
// <- --
//----------------------------------------------------------------------------
void LOG(char *c)
{
	FILE *pDebug;
	pDebug = fopen(LOGFILE, "a+");
	if(NULL==pDebug) return;
	fprintf(pDebug, c);
	fclose(pDebug);
}

//----------------------------------------------------------------------------
// LOG_HEX8
// 
// Append hexadecimal number to logfile (format: 0x12345678)
// logfile is created if it doesn't exist
// 
// -> i = number to log as hex
// <- --
//----------------------------------------------------------------------------
void LOG_HEX8(int i)
{
	char cBuf[16];
	sprintf(cBuf, "0x%08X", i);
	LOG(cBuf);
}

//----------------------------------------------------------------------------
// LOG_HEX2
// 
// Append hexadecimal number to logfile (format: 0x12)
// logfile is created if it doesn't exist
// 
// -> i = number to log as hex
// <- --
//----------------------------------------------------------------------------
void LOG_HEX2(int i)
{
	char cBuf[16];
	sprintf(cBuf, "0x%02X", i);
	LOG(cBuf);
}

//----------------------------------------------------------------------------
// LOG_INT
// 
// Append decimal number to logfile
// logfile is created if it doesn't exist
// 
// -> i = number to log
// <- --
//----------------------------------------------------------------------------
void LOG_INT(int i)
{
	char cBuf[16];
	sprintf(cBuf, "%i", i);
	LOG(cBuf);
}

//----------------------------------------------------------------------------
// LOG_ERR
// 
// Append decoded error to logfile
// logfile is created if it doesn't exist
// 
// -> dwError = error code from GetLastError()
// <- --
//----------------------------------------------------------------------------
void LOG_ERR(unsigned int dwError)
{
	char *lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf, 0, NULL);
	LOG(lpMsgBuf); LocalFree(lpMsgBuf);
	LOG(" (code="); LOG_HEX8(dwError); LOG(")\n");
}
#endif
