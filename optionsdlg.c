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
#include "resource.h"
#include "disk.h"
#include "ini.h"
#include "EnsoniqFS.h"

//----------------------------------------------------------------------------
// global flags and variables
//----------------------------------------------------------------------------
extern HINSTANCE g_hInst;
extern DISK *g_pDiskListRoot;
extern HINSTANCE g_hInst;
extern FsDefaultParamStruct g_DefaultParams;	// initialization parameters
												// needed to find INI file
// global options
extern int g_iOptionEnableFloppy;
extern int g_iOptionEnableCDROM;
extern int g_iOptionEnableImages;
extern int g_iOptionEnablePhysicalDisks;
extern int g_iOptionAutomaticRescan;
extern int g_iOptionEnableLogging;

int m_iDeviceListChanged = 0;

//----------------------------------------------------------------------------
// OptionsDlg_OnCancel
// 
// Gets called when the Cancel button was clicked, ends the dialog handling
//
// -> hWnd = window handle of options dialog
// <- --
//----------------------------------------------------------------------------
void OptionsDlg_OnCancel(HWND hWnd)
{
	LOG("OptionsDlg_OnCancel()\n");
	EndDialog(hWnd, IDCANCEL);
}

//----------------------------------------------------------------------------
// OptionsDlg_OnOK
// 
// Gets called when the OK button was clicked
// Saves the status of the checkboxes to the global variables, writes them
// to the INI file, rescans device list if necessary
//
// -> hWnd = window handle of options dialog
// <- --
//----------------------------------------------------------------------------
void OptionsDlg_OnOK(HWND hWnd)
{
	char *cName = g_DefaultParams.DefaultIniName;
	
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
	g_iOptionEnableLogging = 
		(SendMessage(GetDlgItem(hWnd, IDC_CHK_LOGGING),
		(UINT)BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED)?1:0;

	SetIniValueInt(cName, "[EnsoniqFS]", "EnableFloppy", 
		g_iOptionEnableFloppy);

	SetIniValueInt(cName, "[EnsoniqFS]", "EnableCDROM", 
		g_iOptionEnableCDROM);

	SetIniValueInt(cName, "[EnsoniqFS]", "EnablePhysicalDisks", 
		g_iOptionEnablePhysicalDisks);

	SetIniValueInt(cName, "[EnsoniqFS]", "EnableImages", 
		g_iOptionEnableImages);

	SetIniValueInt(cName, "[EnsoniqFS]", "AutomaticRescan", 
		g_iOptionAutomaticRescan);

	SetIniValueInt(cName, "[EnsoniqFS]", "EnableLogging", 
		g_iOptionEnableLogging);

	if(m_iDeviceListChanged)
	{
		// rescan device list	
		FreeDiskList(1, g_pDiskListRoot);
		g_pDiskListRoot = ScanDevices(0);
	}

	EndDialog(hWnd, IDOK);
}

//----------------------------------------------------------------------------
// OptionsDlg_ParseImageFiles
// 
// Reads the INI file, parses for image files, adds all entries to ComboBox
//
// -> hWnd = window handle of options dialog
// <- --
//----------------------------------------------------------------------------
void OptionsDlg_ParseImageFiles(HWND hWnd)
{
	INI_LINE *pLine, *pIniFile;
	char *cLine;

	// clear "File" ComboBox
	SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
		(UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);

	// open ini file, parse, add image file entries to List
	LOG("Reading INI file: ");

	if(ERR_OK!=ReadIniFile(g_DefaultParams.DefaultIniName, &pIniFile))
	{
		LOG("failed.\n");
		return;
	}
	LOG("OK.\nParsing: ");

	// try to find section header
	pLine = pIniFile;
	while(pLine)
	{	
		if(0==strncmp("[EnsoniqFS]", pLine->cLine, 11)) break;
		pLine = pLine->pNext;
	}

	// section not found?
	if(NULL==pLine)
	{
		LOG("[EnsoniqFS] section not found.\n");
		FreeIniLines(pIniFile);
		return;
	}

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
			cLine = malloc(strlen(pLine->cLine)-5);
			if(NULL==cLine)
			{
				LOG("Error allocating line buffer.\n"); LOG(pLine->cLine);
				FreeIniLines(pIniFile);
				return;
			}
			memset(cLine, 0, strlen(pLine->cLine)-5);
			strncpy(cLine, pLine->cLine+6, strlen(pLine->cLine)-7);
			SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
				(UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)cLine);
			free(cLine);
		}
		
		pLine = pLine->pNext;
	}

	// select first entry
	SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
		(UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

	FreeIniLines(pIniFile);
}
//----------------------------------------------------------------------------
// OptionsDlg_MountImage
// 
// Adds a new file to the image file list
//
// -> hWnd = window handle of options dialog
// <- --
//----------------------------------------------------------------------------
void OptionsDlg_MountImage(HWND hWnd)
{
	char cFilter[] = "All supported image files (*.iso;*.bin;*.gkh;*.ed?)|"
		"*.iso;*.bin;*.gkh;*.ed?;*.img|"
		"ISO images (*.iso;*.img)|*.iso;*.img|"
		"BIN images (*.bin)|*.bin|"
		"Epsread/Epswrite images (*.gkh)|*.gkh|"
		"Giebler images (*.ed?)|*.ed?|"
		"All files (*.*)|*.*||",
		cFN[512], cTitle[] = "Open image file";
	OPENFILENAME ofn;
	int i, iLen;
	
	// replace the separators with zeroes
	iLen = strlen(cFilter);
	for(i=0; i<iLen; i++) if('|'==cFilter[i]) cFilter[i] = 0;
	memset(cFN, 0, 512); memset(&ofn, 0, sizeof(OPENFILENAME));

	// construct the open file dialog
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = g_hInst;
	ofn.lpstrFilter = cFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = cFN;
	ofn.nMaxFile = 512;
	ofn.lpstrTitle = cTitle;
	ofn.Flags = OFN_ENABLESIZING|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|
		OFN_HIDEREADONLY;
	if(GetOpenFileName(&ofn))
	{
		AddToImageList(ofn.lpstrFile);
		OptionsDlg_ParseImageFiles(hWnd);
		m_iDeviceListChanged = 1;
	}
}

//----------------------------------------------------------------------------
// OptionsDlg_UnmountImage
// 
// Removes the currently selected image file from the list and rescans devices
//
// -> hWnd = window handle of options dialog
// <- --
//----------------------------------------------------------------------------
void OptionsDlg_UnmountImage(HWND hWnd)
{
	INI_LINE *pLine, *pIniFile;
	int iLen, iSelectedItem;
	LRESULT lResult;
	char *cName;
	
	// get selected entry
	lResult = SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
		(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
	iSelectedItem = lResult;
	if(CB_ERR==lResult)
	{
		MessageBoxA(0, "Please select an image file first.", 
			"EnsoniqFS · Warning", MB_OK|MB_ICONEXCLAMATION);
		return;
	}

	// get text length
	lResult = SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
		(UINT)CB_GETLBTEXTLEN, (WPARAM)iSelectedItem, (LPARAM)0);
	iLen = lResult + 7;
	cName = malloc(iLen);
	if(NULL==cName) return;
	
	// get selected text
	memset(cName, 0, iLen);
	strcpy(cName, "image=");
	lResult = SendMessage(GetDlgItem(hWnd, IDC_CBO_FILES), 
		(UINT)CB_GETLBTEXT, (WPARAM)iSelectedItem, (LPARAM)cName+6);
	
	LOG("Unmounting image \""); LOG(cName); LOG("\"\n");
	
	// open ini file, parse, remove image file from list
	LOG("Reading INI file: ");

	if(ERR_OK!=ReadIniFile(g_DefaultParams.DefaultIniName, &pIniFile))
	{
		LOG("failed.\n");
		return;
	}
	LOG("OK.\nParsing: ");

	// try to find section header
	pLine = pIniFile;
	while(pLine)
	{	
		if(0==strncmp("[EnsoniqFS]", pLine->cLine, 11)) break;
		pLine = pLine->pNext;
	}

	// section not found?
	if(NULL==pLine)
	{
		LOG("[EnsoniqFS] section not found.\n");
		free(cName);
		FreeIniLines(pIniFile);
		return;
	}

	// skip [EnsoniqFS] section name
	pLine = pLine->pNext;

	// parse all lines after [EnsoniqFS]
	while(pLine)
	{
		// next section found?
		if('['==pLine->cLine[0]) break;
		
		if(0==strncmp(cName, pLine->cLine, strlen(cName)))
		{
			DeleteIniLine(pLine);
			break;
		}
		
		pLine = pLine->pNext;
	}

	// write back INI file
	WriteIniFile(g_DefaultParams.DefaultIniName, pIniFile);
	FreeIniLines(pIniFile);
	free(cName);
	
	// reload image list
	OptionsDlg_ParseImageFiles(hWnd);
	m_iDeviceListChanged = 1;
	
}

//----------------------------------------------------------------------------
// WM_INITDIALOG
// 
// reads settings from global variables, reads image file list from INI file
//
// -> hWnd = window handle of options dialog
// <- --
//----------------------------------------------------------------------------
void OptionsDlg_OnInitDialog(HWND hWnd)
{
	LOG("OptionsDlg_OnInitDialog()\n");

	// read and parse ini file for mounted image files
	OptionsDlg_ParseImageFiles(hWnd);
	m_iDeviceListChanged = 0;
			
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
	SendMessage(GetDlgItem(hWnd, IDC_CHK_LOGGING),
		(UINT)BM_SETCHECK, 
		(WPARAM)(g_iOptionEnableLogging?BST_CHECKED:BST_UNCHECKED), 
		(LPARAM)0);
}

//----------------------------------------------------------------------------
// OptionsDlgProc
// 
// This is the message processing function for the options dialog thread
//
// -> --
// <- TRUE, if the message was processed
//    FALSE, if not
//----------------------------------------------------------------------------
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
					OptionsDlg_UnmountImage(hwndDlg);
					iRetVal = TRUE;
					break;

				case IDC_BTN_MOUNT:
					OptionsDlg_MountImage(hwndDlg);
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
int CreateOptionsDialogModal(HWND hParent)
{
	int iResult;
	LOG("CreateOptionsDialog(): ");

	// create dialog from template
	HRSRC hrsrc = FindResource(g_hInst, MAKEINTRESOURCE(IDD_DLG_OPTIONS), 
		RT_DIALOG);
	HGLOBAL hglobal = LoadResource(g_hInst, hrsrc);
	iResult = DialogBoxIndirectParam(g_hInst, (LPCDLGTEMPLATE)hglobal, hParent, 
		OptionsDlgProc, 0);	
		
	if(0==iResult)
	{
		LOG("DialogBoxIndirectParam() failed.\n");
		return ERR_CREATE_DLG;
	}
	else
	{
		LOG("Dialog closed, return value=%d.\n", iResult);
	}

	return ERR_OK;
}
