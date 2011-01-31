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
#include "disk.h"
#include "choosediskdlg.h"
#include "log.h"
#include "error.h"
#include "resource.h"
#include "ini.h"
#include "EnsoniqFS.h"

//----------------------------------------------------------------------------
// global flags and variables
//----------------------------------------------------------------------------
extern HINSTANCE g_hInst;
extern DISK *g_pDiskListRoot;
DISK **m_pResult;
DISK *m_pCurrentDisk;

//----------------------------------------------------------------------------
// ChooseDiskDlg_OnCancel
// 
// Gets called when the Cancel button was clicked, ends the dialog handling
//
// -> hWnd = window handle
// <- --
//----------------------------------------------------------------------------
void ChooseDiskDlg_OnCancel(HWND hWnd)
{
	LOG("ChooseDiskDlg_OnCancel()\n");
	*m_pResult = NULL;
	EndDialog(hWnd, IDCANCEL);
}

//----------------------------------------------------------------------------
// ChooseDiskDlg_OnOK
// 
// Gets called when the OK button was clicked
//
// -> hWnd = window handle
// <- --
//----------------------------------------------------------------------------
void ChooseDiskDlg_OnOK(HWND hWnd)
{
	int i, iSelection;

	// get combo box selection
	iSelection = SendMessage(GetDlgItem(hWnd, IDC_COMBO_DISK), 
		(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
	LOG("ChooseDiskDlg_OnOK(): Selection = %d\n", iSelection);

	if(CB_ERR==iSelection)
	{
		EndDialog(hWnd, IDCANCEL);
	}	

	// iterate through list to find selected disk
	DISK *pDisk = g_pDiskListRoot; i = 0;
	while(pDisk)
	{
		if(pDisk->iIsEnsoniq)
		{
			if(i==iSelection)
			{
				*m_pResult = pDisk;
				break;
			}
			i++;
		}
		pDisk = pDisk->pNext;
	}
	
	EndDialog(hWnd, IDOK);
}

//----------------------------------------------------------------------------
// WM_INITDIALOG
// 
// Set up dialog, fill dropdown list
//
// -> hWnd = window handle
// <- --
//----------------------------------------------------------------------------
void ChooseDiskDlg_OnInitDialog(HWND hWnd)
{
	LOG("ChooseDiskDlg_OnInitDialog()\n");

	DISK *pDisk;
	char cLine[1024];
	int i = 0;
	
	// clear combo box
	SendMessage(GetDlgItem(hWnd, IDC_COMBO_DISK), 
		(UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);

	// iterate through all available disks and add them to the combo box
	pDisk = g_pDiskListRoot;
	while(pDisk)
	{
		if(pDisk->iIsEnsoniq)
		{
			strcpy(cLine, pDisk->cMsDosName);
			strcat(cLine, " [");
			strcat(cLine, pDisk->cDiskLabel);
			strcat(cLine, "]");
			
			// add a line to the combo box
			SendMessage(GetDlgItem(hWnd, IDC_COMBO_DISK), 
				(UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)cLine);
				
			if(pDisk == m_pCurrentDisk)
			{
				LOG("Selecting index %d from combo box\n", i);
				SendMessage(GetDlgItem(hWnd, IDC_COMBO_DISK), 
					(UINT)CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
			}

			i++;
		}
		pDisk = pDisk->pNext;
	}
}

//----------------------------------------------------------------------------
// ChooseDiskDlgProc
// 
// This is the message processing function for the dialog thread
//
// -> --
// <- TRUE, if the message was processed
//    FALSE, if not
//----------------------------------------------------------------------------
INT_PTR CALLBACK ChooseDiskDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	int iRetVal = FALSE;

	// process messages
	switch(uMsg)
	{
		case WM_INITDIALOG:
			ChooseDiskDlg_OnInitDialog(hwndDlg);
			iRetVal = TRUE;
			break;
			
		case WM_CLOSE:
			EndDialog(hwndDlg, IDCANCEL);
			iRetVal = TRUE;
			break;
		
	  	case WM_COMMAND:
			switch(wParam)
			{
				case IDC_BTN_DISK_OK:
					ChooseDiskDlg_OnOK(hwndDlg);
					iRetVal = TRUE;
					break;

				case IDC_BTN_DISK_CANCEL:
					ChooseDiskDlg_OnCancel(hwndDlg);
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
// CreateChooseDiskDialogModal
// 
// Create a modal dialog
//
// -> --
// <- ERR_OK
//    ERR_CREATE_DLG
//----------------------------------------------------------------------------
int CreateChooseDiskDialogModal(HWND hParent, DISK **pResult, DISK *pCurrent)
{
	int iResult;
	LOG("CreateChooseDiskDialogModal(): ");

	m_pResult = pResult;
	*m_pResult = NULL;
	m_pCurrentDisk = pCurrent;
	
	// create dialog from template
	HRSRC hrsrc = FindResource(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_DISK), 
		RT_DIALOG);
	HGLOBAL hglobal = LoadResource(g_hInst, hrsrc);
	iResult = DialogBoxIndirectParam(g_hInst, (LPCDLGTEMPLATE)hglobal, hParent, 
		ChooseDiskDlgProc, 0);	
		
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
