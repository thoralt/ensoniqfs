//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// LOGGING FUNCTIONS header file
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
#ifndef _LOG_H_
#define _LOG_H_

//----------------------------------------------------------------------------
// logging on/off
//----------------------------------------------------------------------------
// If you want to have logging built in, uncomment the line below
#define LOGGING 1

//----------------------------------------------------------------------------
// logfile
//----------------------------------------------------------------------------
#define LOGFILE	"C:\\EnsoniqFS-LOG.txt"

//----------------------------------------------------------------------------
// Prototypes
//----------------------------------------------------------------------------
#ifdef LOGGING
	void LOG(char *c);
	void LOG_HEX8(int i);
	void LOG_HEX2(int i);
	void LOG_INT(int i);
	void LOG_ERR(unsigned int dwError);
#else
	#define LOG(x)		{}
	#define LOG_HEX8(x) {}
	#define LOG_HEX2(x) {}
	#define LOG_INT(x) 	{}
	#define LOG_ERR(x) 	{}
#endif

#endif

