//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// OPTIONS DIALOG
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
#include <windows.h>
#include <process.h>
#include <commctrl.h>
#include "fsplugin.h"
#include "optionsdlg.h"
#include "log.h"
#include "error.h"
#include "rsrc.h"
#include "disk.h"

//----------------------------------------------------------------------------
// global flags and variables
//----------------------------------------------------------------------------
extern HINSTANCE g_hInst;
extern FsDefaultParamStruct g_DefaultParams;	// initialization parameters
												// needed to find INI file
// global options
extern int g_iOptionEnableFloppy;
extern int g_iOptionEnableCDROM;
extern int g_iOptionEnableImages;
extern int g_iOptionEnablePhysicalDisks;
extern int g_iOptionAutomaticRescan;

int ParseIni(char *cBuf)
{
	INI_LINE *pLine, *pIniFile;
	int i, j;
	
	// open ini file, parse, add image file entries to QueryList
	LOG("Reading INI file: ");

	if(ERR_OK!=ReadIniFile(g_DefaultParams.DefaultIniName, &pIniFile))
	{
		LOG("failed.\n");
		return 0;
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
			return 0;
		}
		else
		{
			// skip [EnsoniqFS] section name
			pLine = pLine->pNext;

			// parse all lines after [EnsoniqFS]
			i = 0;
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
	
	return 1;
}

void OptionsDlg_OnCancel(HWND hWnd)
{
	LOG("OptionsDlg_OnCancel()\n");
	EndDialog(hWnd, IDCANCEL);
}

void OptionsDlg_OnOK(HWND hWnd)
{
	INI_LINE *pLine, *pLastLine=NULL, *pIniFile;
	int iEnableFloppySaved=0, iEnableCDROMSaved=0, 
		iEnablePhysicalDisksSaved=0, iEnableImagesSaved=0,
		iAutomaticRescanSaved=0;
	char cTemp[512];

	LOG("OptionsDlg_OnOK()\n");
	// save checkbox status
	g_iOptionEnableFloppy =
		(SendMessage(GetDlgItem(hWnd, IDC_CHK_FLOPPY),
		(UINT)BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED)?1:0;
	g_iOptionEnableCDROM =
		(SendMessage(GetDlgItem(hWnd, IDC_CHK_CDROM),
		(UINT)BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED)?1:0;
	g_iOptionEnablePhysicalDisks =
		(SendMessage(GetDlgItem(hWnd, IDC_CHK_PHYSICAL),
		(UINT)BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED)?1:0;
	g_iOptionEnableImages =
		(SendMessage(GetDlgItem(hWnd, IDC_CHK_IMAGE),
		(UINT)BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED)?1:0;
	g_iOptionAutomaticRescan = 
		(SendMessage(GetDlgItem(hWnd, IDC_CHK_RESCAN),
		(UINT)BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED)?1:0;

	// read INI file, change values, write back INI file
	
	// open ini file, parse, add image file entries to QueryList
	LOG("Reading INI file: ");
	if(ERR_OK!=ReadIniFile(g_DefaultParams.DefaultIniName, &pIniFile))
	{
		LOG("failed.\n");
		EndDialog(hWnd, IDOK);
		return;
	}

	LOG("OK.\nParsing: ");

	// try to find section header
	pLine = pIniFile;
	pLastLine = pIniFile;
	while(pLine)
	{	
		if(0==strncmp("[EnsoniqFS]", pLine->cLine, 11))
		{
			LOG("[EnsoniqFS] found.\n");
			break;
		}
		pLastLine = pLine;
		pLine = pLine->pNext;
	}

	// section not found?
	if(NULL==pLine)
	{
		LOG("[EnsoniqFS] section not found, creating a new one.\n");
		pLine = InsertIniLine("[EnsoniqFS]", pLastLine);
		
		if(NULL==pLine)
		{
			FreeIniLines(pIniFile);
			EndDialog(hWnd, IDOK);
			return;
		}
	}
	
	// skip [EnsoniqFS] section name
	pLastLine = pLine;
	pLine = pLine->pNext;

	// parse all lines after [EnsoniqFS]
	while(pLine)
	{
		// next section found?
		if('['==pLine->cLine[0]) break;
		
		if(0==strncmp("EnableFloppy=", pLine->cLine, 13))
		{
			free(pLine->cLine); 
			sprintf(cTemp, "EnableFloppy=%i\n", g_iOptionEnableFloppy);
			pLine->cLine = malloc(strlen(cTemp)+1); 
			strcpy(pLine->cLine, cTemp);
			iEnableFloppySaved=1;
		}
		else if(0==strncmp("EnableCDROM=", pLine->cLine, 12))
		{
			free(pLine->cLine); 
			sprintf(cTemp, "EnableCDROM=%i\n", g_iOptionEnableCDROM);
			pLine->cLine = malloc(strlen(cTemp)+1); 
			strcpy(pLine->cLine, cTemp);
			iEnableCDROMSaved=1;
		}
		else if(0==strncmp("EnablePhysicalDisks=", pLine->cLine, 20))
		{
			free(pLine->cLine); 
			sprintf(cTemp, "EnablePhysicalDisks=%i\n", 
				g_iOptionEnablePhysicalDisks);
			pLine->cLine = malloc(strlen(cTemp)+1); 
			strcpy(pLine->cLine, cTemp);
			iEnablePhysicalDisksSaved=1;
		}
		else if(0==strncmp("EnableImages=", pLine->cLine, 13))
		{
			free(pLine->cLine); 
			sprintf(cTemp, "EnableImages=%i\n", g_iOptionEnableImages);
			pLine->cLine = malloc(strlen(cTemp)+1); 
			strcpy(pLine->cLine, cTemp);
			iEnableImagesSaved=1;
		}
		else if(0==strncmp("AutomaticRescan=", pLine->cLine, 16))
		{
			free(pLine->cLine); 
			sprintf(cTemp, "AutomaticRescan=%i\n", 
				g_iOptionAutomaticRescan);
			pLine->cLine = malloc(strlen(cTemp)+1); 
			strcpy(pLine->cLine, cTemp);
			iAutomaticRescanSaved=1;
		}
		
		pLine = pLine->pNext;
	}
	
	// if we come to here, pLastLine points either to "[EnsoniqFS]"
	// or the last member of the "[EnsoniqFS]" group
	// so we could add other values here if necessary
	if(!iEnableFloppySaved)
	{
		sprintf(cTemp, "EnableFloppy=%i\n", g_iOptionEnableFloppy);
		InsertIniLine(cTemp, pLastLine);
	}
	if(!iEnableCDROMSaved)
	{
		sprintf(cTemp, "EnableCDROM=%i\n", g_iOptionEnableCDROM);
		InsertIniLine(cTemp, pLastLine);
	}
	if(!iEnablePhysicalDisksSaved)
	{
		sprintf(cTemp, "EnablePhysicalDisks=%i\n", 
			g_iOptionEnablePhysicalDisks);
		InsertIniLine(cTemp, pLastLine);
	}
	if(!iEnableImagesSaved)
	{
		sprintf(cTemp, "EnableImages=%i\n", g_iOptionEnableImages);
		InsertIniLine(cTemp, pLastLine);
	}
	if(!iAutomaticRescanSaved)
	{
		sprintf(cTemp, "AutomaticRescan=%i\n", g_iOptionAutomaticRescan);
		InsertIniLine(cTemp, pLastLine);
	}

	LOG("Writing INI file.\n");
	WriteIniFile(g_DefaultParams.DefaultIniName, pIniFile);
	FreeIniLines(pIniFile);
	EndDialog(hWnd, IDOK);
}

void OptionsDlg_OnInitDialog(HWND hWnd)
{
	char cBuf[65536];
	int i;
	
	LOG("OptionsDlg_OnInitDialog()\n");

	// read and parse ini file for mounted image files		
	if(ParseIni(cBuf))
	{
		// clear "File" ComboBox
		SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
			(UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);

		// add all entries to ComboBox				
		i = 0;
		while(cBuf[i])
		{
			SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
				(UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)cBuf+i+6);
			i += strlen(cBuf+i) + 1;
		}

		// select first entry
		SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
			(UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
			
		// set checkboxes
		SendMessage(GetDlgItem(hWnd, IDC_CHK_FLOPPY),
			(UINT)BM_SETCHECK, 
			(WPARAM)(g_iOptionEnableFloppy?BST_CHECKED:BST_UNCHECKED), 
			(LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_CHK_CDROM),
			(UINT)BM_SETCHECK, 
			(WPARAM)(g_iOptionEnableCDROM?BST_CHECKED:BST_UNCHECKED), 
			(LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_CHK_PHYSICAL),
			(UINT)BM_SETCHECK, 
			(WPARAM)(g_iOptionEnablePhysicalDisks?BST_CHECKED:BST_UNCHECKED), 
			(LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_CHK_IMAGE),
			(UINT)BM_SETCHECK, 
			(WPARAM)(g_iOptionEnableImages?BST_CHECKED:BST_UNCHECKED), 
			(LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_CHK_RESCAN),
			(UINT)BM_SETCHECK, 
			(WPARAM)(g_iOptionAutomaticRescan?BST_CHECKED:BST_UNCHECKED), 
			(LPARAM)0);
	}
}

INT_PTR CALLBACK OptionsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	int iRetVal = FALSE;

	// process messages
	switch(uMsg)
	{
		case WM_INITDIALOG:
			OptionsDlg_OnInitDialog(hwndDlg);
			iRetVal = TRUE;
			break;
			
		case WM_CLOSE:
			EndDialog(hwndDlg, IDCANCEL);
			iRetVal = TRUE;
			break;
		
	  	case WM_COMMAND:
			switch(wParam)
			{
				case IDC_BTN_UNMOUNT:
					MessageBoxA(0, "IDC_BTN_UNMOUNT", "", 0);
					iRetVal = TRUE;
					break;

				case IDC_BTN_MOUNT:
					MessageBoxA(0, "IDC_BTN_MOUNT", "", 0);
					iRetVal = TRUE;
					break;

				case IDC_BTN_OK:
					OptionsDlg_OnOK(hwndDlg);
					iRetVal = TRUE;
					break;

				case IDC_BTN_CANCEL:
					OptionsDlg_OnCancel(hwndDlg);
					iRetVal = TRUE;
					break;
			}
			break;

		default:
			break;
	}
	
	return iRetVal;

	lParam=lParam;	// just to satisfy "unused parameter lParam" warning
}

//----------------------------------------------------------------------------
// CreateOptionsDialogModal
// 
// Create a modal options dialog
//
// -> --
// <- ERR_OK
//    ERR_CREATE_DLG
//----------------------------------------------------------------------------
int CreateOptionsDialogModal()
{
	int iResult;
	LOG("CreateOptionsDialog(): ");

	// create dialog from template
	HRSRC hrsrc = FindResource(g_hInst, MAKEINTRESOURCE(IDD_DLG_OPTIONS), 
		RT_DIALOG);
	HGLOBAL hglobal = LoadResource(g_hInst, hrsrc);
	iResult = DialogBoxIndirectParam(g_hInst, (LPCDLGTEMPLATE)hglobal, 0, 
		OptionsDlgProc, 0);	
		
	if(0==iResult)
	{
		LOG("DialogBoxIndirectParam() failed.\n");
		return ERR_CREATE_DLG;
	}
	else
	{
		LOG("Dialog closed, return value="); LOG_INT(iResult);
		LOG(".\n");
	}

	return ERR_OK;
}
