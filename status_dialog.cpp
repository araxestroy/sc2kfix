// sc2kfix statsu_dialog.cpp: recreation of the DOS/Mac version status dialog
// (c) 2025 github.com/araxestroy - released under the MIT license

#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <psapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <intrin.h>
#include <string>
#include <regex>

#include <sc2kfix.h>
#include "resource.h"

HWND hStatusDialog = NULL;
HFONT hStatusDialogBoldFont = NULL;
HANDLE hWeatherBitmaps[12];
static HCURSOR hDefaultCursor = NULL;
static HWND hwndDesktop;
static RECT rcTemp, rcDlg, rcDesktop;

static COLORREF crToolColor = RGB(0, 0, 0);
static COLORREF crStatusColor = RGB(0, 0, 0);
static HBRUSH hBrushBkg = NULL;

extern "C" int __stdcall Hook_402793(int iStatic, char* szText, int iMaybeAlways1, COLORREF crColor) {
	__asm {
		push ecx
	}
	if (hStatusDialog) {
		if (iStatic == 0) {
			char szCurrentText[200];
			GetDlgItemText(hStatusDialog, IDC_STATIC_SELECTEDTOOL, szCurrentText, 200);
			if (crColor != crToolColor || strcmp(szText, szCurrentText)) {
				SetDlgItemText(hStatusDialog, IDC_STATIC_SELECTEDTOOL, szText);
				crToolColor = crColor;
				InvalidateRect(GetDlgItem(hStatusDialog, IDC_STATIC_SELECTEDTOOL), NULL, TRUE);
			}
		} else if (iStatic == 1) {
			char szCurrentText[200];
			GetDlgItemText(hStatusDialog, IDC_STATIC_STATUSSTRING, szCurrentText, 200);

			// XXX - this is incredibly ugly
			std::string strText = szText;
			strText = std::regex_replace(strText, std::regex("&"), "&&");
			if (crColor != crStatusColor || strcmp(strText.c_str(), szCurrentText)) {
				SetDlgItemText(hStatusDialog, IDC_STATIC_STATUSSTRING, strText.c_str());
				crStatusColor = crColor;
				InvalidateRect(GetDlgItem(hStatusDialog, IDC_STATIC_STATUSSTRING), NULL, TRUE);
			}
		} else if (iStatic == 2) {
			SendMessage(GetDlgItem(hStatusDialog, IDC_STATIC_WEATHERICON), STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hWeatherBitmaps[bWeatherTrend]);
			InvalidateRect(GetDlgItem(hStatusDialog, IDC_STATIC_WEATHERICON), NULL, TRUE);
		} else
			InvalidateRect(hStatusDialog, NULL, FALSE);
	}

	__asm {
		pop ecx
		push crColor
		push iMaybeAlways1
		push szText
		push iStatic
		mov edi, 0x40BD50
		call edi
	}
}

extern "C" int __stdcall Hook_4021A8(int iShow) {
	__asm {
		push ecx
	}

	int iActualShow = iShow;

	if (bSettingsUseStatusDialog)
		iActualShow = 0;

	if (hStatusDialog)
		ShowWindow(hStatusDialog, iShow ? 5 : 0);
	else if (bSettingsUseStatusDialog)
		ShowStatusDialog();

	__asm {
		pop ecx
		push [iActualShow]
		mov edi, 0x40C3E0
		call edi
	}
}

extern "C" int __stdcall Hook_40103C(int iShow) {
	__asm {
		push ecx
	}

	if (hStatusDialog)
		ShowWindow(hStatusDialog, iShow ? 5 : 0);

	__asm {
		pop ecx
		push iShow
		mov edi, 0x40B9E0
		call edi
	}
}

BOOL CALLBACK StatusDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static BOOL bMouseDown = FALSE;
	static POINT ptPreviousCursorPos;
	RECT rectWindow;

	switch (message) {
	case WM_INITDIALOG:
		hStatusDialogBoldFont = CreateFont(-MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "MS Sans Serif");
		SendMessage(GetDlgItem(hwndDlg, IDC_STATIC_SELECTEDTOOL), WM_SETFONT, (WPARAM)hStatusDialogBoldFont, TRUE);
		return TRUE;

	case WM_CTLCOLORSTATIC:
		if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_STATIC_SELECTEDTOOL))
			SetTextColor((HDC)wParam, crToolColor);
		else if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_STATIC_STATUSSTRING))
			SetTextColor((HDC)wParam, crStatusColor);
		
		SetBkColor((HDC)wParam, RGB(240, 240, 240));
		if (!hBrushBkg)
			hBrushBkg = CreateSolidBrush(RGB(240, 240, 240));
		return (LONG)hBrushBkg;

	case WM_MOUSEMOVE:
		if (bMouseDown) {
			POINT ptCurrentCursorPos;
			GetCursorPos(&ptCurrentCursorPos);
			GetWindowRect(hwndDlg, &rectWindow);
			MoveWindow(hwndDlg, ptCurrentCursorPos.x - ptPreviousCursorPos.x, ptCurrentCursorPos.y - ptPreviousCursorPos.y, rectWindow.right - rectWindow.left, rectWindow.bottom - rectWindow.top, FALSE);
		}
		return FALSE;
	case WM_LBUTTONDOWN:
		bMouseDown = TRUE;
		SetCapture(hwndDlg);
		GetCursorPos(&ptPreviousCursorPos);
		GetWindowRect(hwndDlg, &rectWindow);
		ptPreviousCursorPos.x -= rectWindow.left;
		ptPreviousCursorPos.y -= rectWindow.top;
		return FALSE;
	case WM_LBUTTONUP:
		bMouseDown = FALSE;
		ReleaseCapture();
		return FALSE;
	}
	return FALSE;
}

HWND ShowStatusDialog(void) {
	DWORD* CWndMainWindow = (DWORD*)*(DWORD*)0x4C702C;	// god this is awful

	if (hStatusDialog)
		return hStatusDialog;

	hStatusDialog = CreateDialogParam(hSC2KFixModule, MAKEINTRESOURCE(IDD_SIMULATIONSTATUS), (HWND)(CWndMainWindow[7]), StatusDialogProc, NULL);
	if (!hStatusDialog) {
		ConsoleLog(LOG_ERROR, "Couldn't create statuss dialog: 0x%08X\n", GetLastError());
		return NULL;
	}
	hwndDesktop = GetDesktopWindow();
	GetWindowRect(hwndDesktop, &rcDesktop);
	SetWindowPos(hStatusDialog, HWND_TOP, rcDesktop.left + (rcDesktop.right - rcDesktop.left) / 8 + 128, rcDesktop.top + (rcDesktop.bottom - rcDesktop.top) / 8, 0, 0, SWP_NOSIZE);
	ShowWindow(hStatusDialog, SW_HIDE);
	return hStatusDialog;
}