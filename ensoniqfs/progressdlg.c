//----------------------------------------------------------------------------
// EnsoniqFS plugin for TotalCommander
//
// PROGRESS DIALOG
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
#include "progressdlg.h"
#include "log.h"
#include "error.h"
#include "resource.h"

//----------------------------------------------------------------------------
// global flags and variables
//----------------------------------------------------------------------------
BOOL m_bProgressDone = FALSE; 
HWND m_hProgressWnd = NULL;
extern HINSTANCE g_hInst;

//----------------------------------------------------------------------------
// ProgressDialogThread
// 
// Main thread for progress window
//
// -> dummy = unused pointer
// <- --
//----------------------------------------------------------------------------
void ProgressDialogThread(void *dummy)
{
	HWND hWnd;
	MSG msg; 

	// this is to suppress warning: Unused parameter
	dummy=dummy;
	
	LOG("ProgressDialogThread() started. ");
	
    // create the dialog window
	m_hProgressWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DLG_PROGRESS), 0, 0);
	hWnd = m_hProgressWnd;
	if(hWnd!=NULL)
	{
		// show dialog
		LOG("ShowWindow(): ");
		ShowWindow(hWnd, SW_SHOW);
		LOG("OK. ");
	}
	else
	{
		LOG("CreateDialog() failed.\n");
		m_bProgressDone = TRUE;
		return;
	}
	
	LOG("Entering message loop.\n");
	
	// message loop
	while(1)
	{
		// get next message
		if(GetMessage(&msg, hWnd, 0, 0))
		{
			// abort?
			if(msg.message==WM_USER+1)
			{
				LOG("ProgressDialogThread() stop: ");
				DestroyWindow(hWnd); 
				m_bProgressDone = TRUE; 
				LOG("OK.\n");
				break;
			} 

/* This is only for referencs (from Dev-Cpp example)
			if(msg.message==WM_KEYUP)
			{
				int nVirtKey = (int)msg.wParam;
				// if the user pressed the ESCAPE key, then
				// print the text the user entered and quit
				if(nVirtKey==VK_ESCAPE)
				{
					// get the edit control
					HWND hEdit = GetDlgItem(hWnd, IDC_EDIT);
					if(hEdit)
					{
						// get the input text the user entered
						// and print it to the console window
						char pText[3201];
						int nSize = ::GetWindowText(hEdit, pText, 3200);
						pText[nSize] = 0;
						printf("\nYou have entered the ");
						printf("following text in a second ");
						printf("thread:\n\n%s\n\n",pText);
					}
					else
					{
						printf("Failed to get edit control\n");
					}
					// destroy the dialog and get out of the message loop 
					DestroyWindow(hWnd);
					m_bProgressDone = TRUE;
					break;
				} 
			} 
*/
			// process message 
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		}
		else
		{
			m_bProgressDone = TRUE; 
			break; 
		}
	} 
} 

//----------------------------------------------------------------------------
// UpdateProgressDialog
// 
// Display new text and new progress bar position
//
// -> cText = text to display
//    iProgress = progress bar position (0...100)
// <- --
//----------------------------------------------------------------------------
void UpdateProgressDialog(char *cText, int iProgress)
{
	// check handle
	if(NULL==m_hProgressWnd)
	{
		LOG("UpdateProgressDialog(): Warning: m_hProgressWnd==NULL\n");
		return;
	}

	// update text
	SendMessage(GetDlgItem(m_hProgressWnd, IDC_STATIC_STATUS), (UINT)WM_SETTEXT,
		(WPARAM)0, (LPARAM) cText);

	// update progress bar
	SendMessage(GetDlgItem(m_hProgressWnd, IDC_PGB), (UINT)PBM_SETPOS,
		(WPARAM)iProgress, 0);
	
	Sleep(10);
}

//----------------------------------------------------------------------------
// CreateProgressDialog
// 
// Create a new thread which creates a progress dialog
//
// -> --
// <- ERR_OK
//    ERR_CREATE_DLG
//----------------------------------------------------------------------------
int CreateProgressDialog()
{
	LOG("CreateProgressDialog(): ");
	
	// check handle
	if(NULL!=m_hProgressWnd)
	{
		LOG("Warning: Dialog was already created.\n");
		return ERR_CREATE_DLG;
	}
	
	// create thread
	if(-1==(int)_beginthread(ProgressDialogThread, 0, NULL))
	{
		LOG("Failed to create progress dialog thread.\n");
		return ERR_CREATE_DLG;
	}
	
	LOG("OK.\n");
	Sleep(150);
	return ERR_OK;
}

//----------------------------------------------------------------------------
// GetProgressDialogHwnd
// 
// Reads the window handle of the dialog
//
// -> --
// <- window handle or NULL
//----------------------------------------------------------------------------
HWND GetProgressDialogHwnd(void)
{
	return m_hProgressWnd;
}

//----------------------------------------------------------------------------
// DestroyProgressDialog
// 
// Destroy the progress dialog
//
// -> --
// <- ERR_OK
//    ERR_DESTROY_DLG
//----------------------------------------------------------------------------
int DestroyProgressDialog()
{
	int iCount = 0;
	DWORD dwError;
	
	LOG("DestroyProgressDialog(): ");
	
	// check handle
	if(NULL==m_hProgressWnd)
	{
		LOG("Warning: m_hProgressWnd==NULL\n");
		return ERR_OK;
	}
	
	// post message to thread
	if(0==PostMessage(m_hProgressWnd, WM_USER+1, 0, 0))
	{
		dwError = GetLastError();
		LOG("SendMessage() failed: "); LOG_ERR(dwError);
	}
	
	// wait for thread to end
	while((FALSE==m_bProgressDone)&&(iCount<5))
	{
		Sleep(100);
		iCount++;
	}
	
	m_hProgressWnd = NULL;
	
	if(FALSE==m_bProgressDone)
	{
		LOG("failed (timeout).\n");
		return ERR_DESTROY_DLG;
	}
	else
	{
		LOG("OK.\n");
		return ERR_OK;
	}
}
