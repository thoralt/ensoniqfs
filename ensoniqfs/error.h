//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// ERROR CONSTANTS
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
#ifndef _ERROR_H_
#define _ERROR_H_


//----------------------------------------------------------------------------
// error codes
//----------------------------------------------------------------------------
#define ERR_OK				0	// no error
#define ERR_SCAN			1	// error scanning for devices
#define ERR_NOT_SUPPORTED	2	// this function or type is not supported
#define ERR_MEM				3	// memory could not be allocated
#define ERR_NOT_OPEN		4	// file or device is not open
#define ERR_READ			5	// error during read
#define ERR_SEEK			6	// error during seek
#define ERR_OUT_OF_BOUNDS	7	// block number too big
#define ERR_DISK_NOT_FOUND	8	// disk not found from path name
#define ERR_PATH_NOT_FOUND	9	// directory path not found
#define ERR_DIRLEVEL		10	// recursive directory more than 255 levels
#define ERR_FAT				11	// error reading FAT entry
#define ERR_WRITE			12	// error writing to file or disk (Ensoniq)
#define ERR_ABORTED			13	// user wants to abort the current action
#define ERR_LOCAL_WRITE		14	// error writing local file (Windows)
#define ERR_LOCAL_READ		15	// error reading local file (Windows)
#define ERR_NOT_IN_CACHE	16	// the requested block is not in the cache
#define ERR_DISK_FULL		17	// the destination disk is full
#define ERR_INVALID_PARENT_LINKS	18	// for ETools
#define ERR_NOT_FOUND		19	// file or directory name not found
#define ERR_CREATE_DLG		20	// error creating dialog
#define ERR_DESTROY_DLG		21	// error destroying dialog

#endif
