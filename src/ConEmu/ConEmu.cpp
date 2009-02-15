#include "Header.h"

CConEmuMain::CConEmuMain()
{
    wcscpy(szConEmuVersion, L"?.?.?.?");
	gnLastProcessCount=0;
	cBlinkNext=0;
	WindowMode=0;
	//hPipe=NULL;
	//hPipeEvent=NULL;
	mb_InSizing = false;
	isWndNotFSMaximized=false;
	isShowConsole=false;
	mb_FullWindowDrag=false;
	isLBDown=false;
	isDragProcessed=false;
	isRBDown=false;
	ibSkilRDblClk=false; 
	dwRBDownTick=0;
	isPiewUpdate = false; //true; --Maximus5
	gbPostUpdateWindowSize = false;
	hPictureView = NULL; 
	bPicViewSlideShow = false; 
	dwLastSlideShowTick = 0;
	gb_ConsoleSelectMode=false;
	setParent = false; setParent2 = false;
	RBDownNewX=0; RBDownNewY=0;
	cursor.x=0; cursor.y=0; Rcursor=cursor;
	lastMMW=-1;
	lastMML=-1;
	DragDrop=NULL;
	ProgressBars=NULL;
	cBlinkShift=0;
	Title[0]=0; TitleCmp[0]=0;
	//mb_InClose = FALSE;
	memset(m_ProcList, 0, 1000*sizeof(DWORD)); m_ProcCount=0;
	mn_TopProcessID = 0; ms_TopProcess[0] = 0; mb_FarActive = FALSE;

	mh_Psapi = NULL;
	GetModuleFileNameEx= NULL;
}

BOOL CConEmuMain::Init()
{
	mh_Psapi = LoadLibrary(_T("psapi.dll"));
	if (mh_Psapi) {
		GetModuleFileNameEx = (FGetModuleFileNameEx)GetProcAddress(mh_Psapi, "GetModuleFileNameExW");
		if (GetModuleFileNameEx)
			return TRUE;
	}

	DWORD dwErr = GetLastError();
	TCHAR szErr[255];
	wsprintf(szErr, _T("Can't initialize psapi!\r\nLastError = 0x%08x"), dwErr);
	MBoxA(szErr);
	return FALSE;
}

CConEmuMain::~CConEmuMain()
{
}

void CConEmuMain::Destroy()
{
	if (ghWnd)
		DestroyWindow(ghWnd);
}

// returns difference between window size and client area size of inWnd in outShift->x, outShift->y
void CConEmuMain::GetCWShift(HWND inWnd, POINT *outShift)
{
    RECT cRect, wRect;
    GetClientRect(inWnd, &cRect); // The left and top members are zero. The right and bottom members contain the width and height of the window.
    GetWindowRect(inWnd, &wRect); // screen coordinates of the upper-left and lower-right corners of the window
    outShift->x = wRect.right  - wRect.left - cRect.right;
    outShift->y = wRect.bottom - wRect.top  - cRect.bottom;
}

void CConEmuMain::ShowSysmenu(HWND Wnd, HWND Owner, int x, int y)
{
    bool iconic = IsIconic(Wnd);
    bool zoomed = IsZoomed(Wnd);
    bool visible = IsWindowVisible(Wnd);
    int style = GetWindowLong(Wnd, GWL_STYLE);

    HMENU systemMenu = GetSystemMenu(Wnd, false);
    if (!systemMenu)
        return;

    EnableMenuItem(systemMenu, SC_RESTORE, MF_BYCOMMAND | (iconic || zoomed ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, SC_MOVE, MF_BYCOMMAND | (!(iconic || zoomed) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, SC_SIZE, MF_BYCOMMAND | (!(iconic || zoomed) && (style & WS_SIZEBOX) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, SC_MINIMIZE, MF_BYCOMMAND | (!iconic && (style & WS_MINIMIZEBOX)? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, SC_MAXIMIZE, MF_BYCOMMAND | (!zoomed && (style & WS_MAXIMIZEBOX) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, ID_TOTRAY, MF_BYCOMMAND | (visible ? MF_ENABLED : MF_GRAYED));

    SendMessage(Wnd, WM_INITMENU, (WPARAM)systemMenu, 0);
    SendMessage(Wnd, WM_INITMENUPOPUP, (WPARAM)systemMenu, MAKELPARAM(0, true));
    SetActiveWindow(Owner);

    int command = TrackPopupMenu(systemMenu, TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, x, y, 0, Owner, NULL);

    if (Icon.isWindowInTray)
        switch(command)
        {
        case SC_RESTORE:
        case SC_MOVE:
        case SC_SIZE:
        case SC_MINIMIZE:
        case SC_MAXIMIZE:
            SendMessage(Wnd, WM_TRAYNOTIFY, 0, WM_LBUTTONDOWN);
            break;
        }

    if (command)
        PostMessage(Wnd, WM_SYSCOMMAND, (WPARAM)command, 0);
}

RECT CConEmuMain::ConsoleOffsetRect()
{
    RECT rect; memset(&rect, 0, sizeof(rect));

	if (TabBar.IsActive())
		rect = TabBar.GetMargins();

	/*rect.top = TabBar.IsActive()?TabBar.Height():0;
    rect.left = 0;
    rect.bottom = 0;
    rect.right = 0;*/

	return rect;
}

RECT CConEmuMain::DCClientRect(RECT* pClient/*=NULL*/)
{
    RECT rect;
	if (pClient)
		rect = *pClient;
	else
		GetClientRect(ghWnd, &rect);
	if (TabBar.IsActive()) {
		RECT mr = TabBar.GetMargins();
		//rect.top += TabBar.Height();
		rect.top += mr.top;
		rect.left += mr.left;
		rect.right -= mr.right;
		rect.bottom -= mr.bottom;
	}

	if (pClient)
		*pClient = rect;
    return rect;
}

void CConEmuMain::SyncWindowToConsole()
{
    DEBUGLOGFILE("SyncWindowToConsole\n");
    
    RECT wndR;
    GetWindowRect(ghWnd, &wndR);
    POINT p;
    GetCWShift(ghWnd, &p);
    RECT consoleRect = ConsoleOffsetRect();

    #ifdef MSGLOGGER
        char szDbg[100]; wsprintfA(szDbg, "   pVCon:Size={%i,%i}\n", pVCon->Width,pVCon->Height);
        DEBUGLOGFILE(szDbg);
    #endif
    
    MOVEWINDOW(ghWnd, wndR.left, wndR.top, 
		pVCon->Width + p.x + consoleRect.left + consoleRect.right, 
		pVCon->Height + p.y + consoleRect.top + consoleRect.bottom, 
		1);
}

// returns console size in columns and lines calculated from current window size
// rectInWindow - ���� true - � ������, false - ������ ������
COORD CConEmuMain::ConsoleSizeFromWindow(RECT* arect /*= NULL*/, bool frameIncluded /*= false*/, bool alreadyClient /*= false*/)
{
    COORD size;

	if (!gSet.LogFont.lfWidth || !gSet.LogFont.lfHeight) {
		// ������ ������ ��� �� ���������������! ������ ������� ������ �������! TODO:
		CONSOLE_SCREEN_BUFFER_INFO inf; memset(&inf, 0, sizeof(inf));
		GetConsoleScreenBufferInfo(pVCon->hConOut(), &inf);
		size = inf.dwSize;
		return size; 
	}

    RECT rect, consoleRect;
    if (arect == NULL)
    {
		frameIncluded = false;
        GetClientRect(ghWnd, &rect);
	    consoleRect = ConsoleOffsetRect();
    } 
    else
    {
        rect = *arect;
		if (alreadyClient)
			memset(&consoleRect, 0, sizeof(consoleRect));
		else
			consoleRect = ConsoleOffsetRect();
    }
    
    size.X = (rect.right - rect.left - (frameIncluded ? cwShift.x : 0) - consoleRect.left - consoleRect.right)
		/ gSet.LogFont.lfWidth;
    size.Y = (rect.bottom - rect.top - (frameIncluded ? cwShift.y : 0) - consoleRect.top - consoleRect.bottom)
		/ gSet.LogFont.lfHeight;
    #ifdef MSGLOGGER
        char szDbg[100]; wsprintfA(szDbg, "   ConsoleSizeFromWindow={%i,%i}\n", size.X, size.Y);
        DEBUGLOGFILE(szDbg);
    #endif
    return size;
}

// return window size in pixels calculated from console size
RECT CConEmuMain::WindowSizeFromConsole(COORD consoleSize, bool rectInWindow /*= false*/, bool clientOnly /*= false*/)
{
    RECT rect;
    rect.top = 0;   
    rect.left = 0;
    RECT offsetRect;
	if (clientOnly)
		memset(&offsetRect, 0, sizeof(RECT));
	else
		offsetRect = ConsoleOffsetRect();
    rect.bottom = consoleSize.Y * gSet.LogFont.lfHeight + (rectInWindow ? cwShift.y : 0) + offsetRect.top + offsetRect.bottom;
    rect.right = consoleSize.X * gSet.LogFont.lfWidth + (rectInWindow ? cwShift.x : 0) + offsetRect.left + offsetRect.right;
    #ifdef MSGLOGGER
        char szDbg[100]; wsprintfA(szDbg, "   WindowSizeFromConsole={%i,%i}\n", rect.right,rect.bottom);
        DEBUGLOGFILE(szDbg);
    #endif
    return rect;
}

// size in columns and lines
void CConEmuMain::SetConsoleWindowSize(const COORD& size, bool updateInfo)
{
    #ifdef MSGLOGGER
        static COORD lastSize1;
        if (lastSize1.Y>size.Y)
            lastSize1.Y=size.Y; //DEBUG
        lastSize1 = size;
        char szDbg[100]; wsprintfA(szDbg, "SetConsoleWindowSize({%i,%i},%i)\n", size.X, size.Y, updateInfo);
        DEBUGLOGFILE(szDbg);
    #endif

    if (isPictureView()) {
        isPiewUpdate = true;
        return;
    }

    // update size info
    if (updateInfo && !gSet.isFullScreen && !IsZoomed(ghWnd))
    {
        gSet.wndWidth = size.X;
        wsprintf(temp, _T("%i"), gSet.wndWidth);
        SetDlgItemText(ghOpWnd, tWndWidth, temp);

        gSet.wndHeight = size.Y;
        wsprintf(temp, _T("%i"), gSet.wndHeight);
        SetDlgItemText(ghOpWnd, tWndHeight, temp);
    }

    // case: simple mode
    if (gSet.BufferHeight == 0)
    {
        MOVEWINDOW(ghConWnd, 0, 0, 1, 1, 1);
        SETCONSOLESCREENBUFFERSIZE(pVCon->hConOut(), size);
        MOVEWINDOW(ghConWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 1);
        return;
    }

    // global flag of the first call which is:
    // *) after getting all the settings
    // *) before running the command
    static bool s_isFirstCall = true;

    // case: buffer mode: change buffer
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(pVCon->hConOut(), &csbi))
        return;
    csbi.dwSize.X = size.X;
    if (s_isFirstCall)
    {
        // first call: buffer height = from settings
        s_isFirstCall = false;
        csbi.dwSize.Y = max(gSet.BufferHeight, size.Y);
    }
    else
    {
        if (csbi.dwSize.Y == csbi.srWindow.Bottom - csbi.srWindow.Top + 1)
            // no y-scroll: buffer height = new window height
            csbi.dwSize.Y = size.Y;
        else
            // y-scroll: buffer height = old buffer height
            csbi.dwSize.Y = max(csbi.dwSize.Y, size.Y);
    }
    MOVEWINDOW(ghConWnd, 0, 0, 1, 1, 1);
    SETCONSOLESCREENBUFFERSIZE(pVCon->hConOut(), csbi.dwSize);
    
    // set console window
    if (!GetConsoleScreenBufferInfo(pVCon->hConOut(), &csbi))
        return;
    SMALL_RECT rect;
    rect.Top = csbi.srWindow.Top;
    rect.Left = csbi.srWindow.Left;
    rect.Right = rect.Left + size.X - 1;
    rect.Bottom = rect.Top + size.Y - 1;
    if (rect.Right >= csbi.dwSize.X)
    {
        int shift = csbi.dwSize.X - 1 - rect.Right;
        rect.Left += shift;
        rect.Right += shift;
    }
    if (rect.Bottom >= csbi.dwSize.Y)
    {
        int shift = csbi.dwSize.Y - 1 - rect.Bottom;
        rect.Top += shift;
        rect.Bottom += shift;
    }
    SetConsoleWindowInfo(pVCon->hConOut(), TRUE, &rect);
}

// �������� ������ ������� �� ������� ���� (��������)
void CConEmuMain::SyncConsoleToWindow()
{
	DEBUGLOGFILE("SyncConsoleToWindow\n");

	// ��������� ������ ������ �������
	COORD newConSize = ConsoleSizeFromWindow();
	// �������� ������� ������ ����������� ����
    CONSOLE_SCREEN_BUFFER_INFO inf; memset(&inf, 0, sizeof(inf));
    GetConsoleScreenBufferInfo(pVCon->hConOut(), &inf);

	// ���� ����� ������ - ...
	if (newConSize.X != (inf.srWindow.Right-inf.srWindow.Left+1) ||
		newConSize.Y != (inf.srWindow.Bottom-inf.srWindow.Top+1))
	{
		SetConsoleWindowSize(newConSize, true);
		if (pVCon)
			pVCon->InitDC();
	}
}

bool CConEmuMain::SetWindowMode(uint inMode)
{
    static RECT wndNotFS;
    switch(inMode)
    {
    case rNormal:
        DEBUGLOGFILE("SetWindowMode(rNormal)\n");
    case rMaximized:
        DEBUGLOGFILE("SetWindowMode(rMaximized)\n");
        if (gSet.isFullScreen)
        {
         LONG style = gSet.BufferHeight ? WS_VSCROLL : 0; // NightRoman
            style |= WS_OVERLAPPEDWINDOW | WS_VISIBLE;
            SetWindowLongPtr(ghWnd, GWL_STYLE, style);
            SETWINDOWPOS(ghWnd, HWND_TOP, wndNotFS.left, wndNotFS.top, wndNotFS.right - wndNotFS.left, wndNotFS.bottom - wndNotFS.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
        gSet.isFullScreen = false;
        SendMessage(ghWnd, WM_SYSCOMMAND, inMode == rNormal ? SC_RESTORE : SC_MAXIMIZE, 0);
        break;

    case rFullScreen:
        DEBUGLOGFILE("SetWindowMode(rFullScreen)\n");
        if (!gSet.isFullScreen)
        {
            gSet.isFullScreen = true;
            isWndNotFSMaximized = IsZoomed(ghWnd);
         if (isWndNotFSMaximized) // � ������ NightRoman ��� ��� ������ ��������������
                SendMessage(ghWnd, WM_SYSCOMMAND, SC_RESTORE, 0);

            GetWindowRect(ghWnd, &wndNotFS);

            LONG style = gSet.BufferHeight ? WS_VSCROLL : 0;
            style |= WS_POPUP | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE;
            SetWindowLongPtr(ghWnd, GWL_STYLE, style);
            SETWINDOWPOS(ghWnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            CheckRadioButton(ghOpWnd, rNormal, rFullScreen, rFullScreen);
        }
        break;
    }

    bool canEditWindowSizes = inMode == rNormal;
    EnableWindow(GetDlgItem(ghOpWnd, tWndWidth), canEditWindowSizes);
    EnableWindow(GetDlgItem(ghOpWnd, tWndHeight), canEditWindowSizes);
    SyncConsoleToWindow();
    return true;
}

// ��������� ���� ��� PictureView ������ ����� ������������� �������������, ��� ���
// ������������ �� ���� ��� ����������� "���������" - ������
bool CConEmuMain::isPictureView()
{
    bool lbRc = false;
    
	if (hPictureView && !IsWindow(hPictureView)) {
		InvalidateAll();
	    hPictureView = NULL;
	}
	
	if (!hPictureView) {
		hPictureView = FindWindowEx(ghWnd, NULL, L"FarPictureViewControlClass", NULL);
		if (!hPictureView)
			hPictureView = FindWindowEx(ghWndDC, NULL, L"FarPictureViewControlClass", NULL);
		if (!hPictureView) { // FullScreen?
			hPictureView = FindWindowEx(NULL, NULL, L"FarPictureViewControlClass", NULL);
			// ������ �� ������� ������� ���� ���������... �� �� ����� �� ��������� � ����� ����������
		}
	}

	lbRc = hPictureView!=NULL;

    // ���� �������� Help (F1) - ������ PictureView ��������
    if (!IsWindowVisible(hPictureView)) {
        lbRc = false;
        hPictureView = NULL;
    }
    if (bPicViewSlideShow && !hPictureView) {
        bPicViewSlideShow=false;
    }

    return lbRc;
}

bool CConEmuMain::isFilePanel()
{
    TCHAR* pszTitle=Title;
    if (Title[0]==_T('[') && isdigit(Title[1]) && (Title[2]==_T('/') || Title[3]==_T('/'))) {
	    // ConMan
	    pszTitle = _tcschr(Title, _T(']'));
	    if (!pszTitle)
		    return false;
		while (*pszTitle && *pszTitle!=_T('{'))
			pszTitle++;
    }
    
    if ((_tcsncmp(pszTitle, _T("{\\\\"), 3)==0) ||
	    (pszTitle[0] == _T('{') && isalpha(pszTitle[1]) && pszTitle[2] == _T(':') && pszTitle[3] == _T('\\')))
    {
	    TCHAR *Br = _tcsrchr(pszTitle, _T('}'));
	    if (Br && _tcscmp(Br, _T("} - Far"))==0)
		    return true;
    }
    //TCHAR *BrF = _tcschr(Title, '{'), *BrS = _tcschr(Title, '}'), *Slash = _tcschr(Title, '\\');
    //if (BrF && BrS && Slash && BrF == Title && (Slash == Title+1 || Slash == Title+3))
    //    return true;
    return false;
}

bool CConEmuMain::isConSelectMode()
{
    //TODO: �� �������, ���-�� ����������� ����������?
    return gb_ConsoleSelectMode;
}

void CConEmuMain::ForceShowTabs(BOOL abShow)
{
	if (!pVCon)
		return;

	BOOL lbTabsAllowed = abShow && TabBar.IsAllowed();

    if (abShow /*&& !TabBar.IsActive()*/ && gSet.isTabs && lbTabsAllowed)
    {
        TabBar.Activate();
		ConEmuTab tab; memset(&tab, 0, sizeof(tab));
		tab.Pos=0;
		tab.Current=1;
		tab.Type = 1;
		TabBar.Update(&tab, 1);
		gbPostUpdateWindowSize = true;
	} else if (!abShow) {
		TabBar.Deactivate();
		gbPostUpdateWindowSize = true;
	}

	if (gbPostUpdateWindowSize) { // ������ �� ���-�� ��������
		ReSize();
        /*RECT rcNewCon; GetClientRect(ghWnd, &rcNewCon);
		DCClientRect(&rcNewCon);
        MoveWindow(ghWndDC, rcNewCon.left, rcNewCon.top, rcNewCon.right - rcNewCon.left, rcNewCon.bottom - rcNewCon.top, 0);
        dcWindowLast = rcNewCon;
		
	    if (gSet.LogFont.lfWidth)
	    {
	        SyncConsoleToWindow();
		}*/
    }
}

void CConEmuMain::PaintGaps(HDC hDC/*=NULL*/)
{
	BOOL lbOurDc = (hDC==NULL);
    
	if (hDC==NULL)
		hDC = GetDC(ghWnd); // ������� ����!

	HBRUSH hBrush = CreateSolidBrush(gSet.Colors[0]);

	RECT rcClient;
	GetClientRect(ghWnd, &rcClient); // ���������� ����� �������� ����

	WINDOWPLACEMENT wpl; memset(&wpl, 0, sizeof(wpl)); wpl.length = sizeof(wpl);
	GetWindowPlacement(ghWndDC, &wpl); // ��������� ����, � ������� ���� ���������

	RECT offsetRect = ConsoleOffsetRect(); // �������� � ������ �����

	// paint gaps between console and window client area with first color

	RECT rect;

	//TODO:!!!
	// top
	rect = rcClient;
	rect.top += offsetRect.top;
	rect.bottom = wpl.rcNormalPosition.top;
	if (!IsRectEmpty(&rect))
		FillRect(hDC, &rect, hBrush);
#ifdef _DEBUG
	GdiFlush();
#endif

	// right
	rect.left = wpl.rcNormalPosition.right;
	rect.bottom = rcClient.bottom;
	if (!IsRectEmpty(&rect))
		FillRect(hDC, &rect, hBrush);
#ifdef _DEBUG
	GdiFlush();
#endif

	// left
	rect.left = 0;
	rect.right = wpl.rcNormalPosition.left;
	rect.bottom = rcClient.bottom;
	if (!IsRectEmpty(&rect))
		FillRect(hDC, &rect, hBrush);
#ifdef _DEBUG
	GdiFlush();
#endif

	// bottom
	rect.left = 0;
	rect.right = rcClient.right;
	rect.top = wpl.rcNormalPosition.bottom;
	rect.bottom = rcClient.bottom;
	if (!IsRectEmpty(&rect))
		FillRect(hDC, &rect, hBrush);
#ifdef _DEBUG
	GdiFlush();
#endif

	DeleteObject(hBrush);

	if (lbOurDc)
		ReleaseDC(ghWnd, hDC);
}

DWORD CConEmuMain::CheckProcesses()
{
	// ������ ������ ����� ���������� ����� ����� ������ ���������
	BOOL  lbProcessChanged = FALSE;
    m_ProcCount = GetConsoleProcessList(m_ProcList,1000);
	if (m_ProcCount && (!gnLastProcessCount || m_ProcCount!=gnLastProcessCount))
		lbProcessChanged = TRUE;
	else if (m_ProcCount && m_ProcList[0]!=mn_TopProcessID)
		lbProcessChanged = TRUE;
	gnLastProcessCount = m_ProcCount;
	
	if (lbProcessChanged) {
		if (m_ProcList[0]==mn_TopProcessID) {
			// �� ��������
			mb_FarActive = _tcscmp(ms_TopProcess, _T("far.exe"))==0;
		} else
		if (m_ProcList[0]!=GetCurrentProcessId())
		{
			// �������� ���������� � ������� ��������
			DWORD dwErr = 0;
			HANDLE hProcess = OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION, FALSE, m_ProcList[0]);
			if (!hProcess)
				dwErr = GetLastError();
			else
			{
				TCHAR szFilePath[MAX_PATH+1];
				if (!GetModuleFileNameEx(hProcess, 0, szFilePath, MAX_PATH))
					dwErr = GetLastError();
				else
				{
					TCHAR* pszSlash = _tcsrchr(szFilePath, _T('\\'));
					if (pszSlash) pszSlash++; else pszSlash=szFilePath;
					int nLen = _tcslen(pszSlash);
					if (nLen>MAX_PATH) pszSlash[MAX_PATH]=0;
					_tcscpy(ms_TopProcess, pszSlash);
					_tcslwr(ms_TopProcess);
					mb_FarActive = _tcscmp(ms_TopProcess, _T("far.exe"))==0;
					mn_TopProcessID = m_ProcList[0];
				}
				CloseHandle(hProcess); hProcess = NULL;
			}

		}
		TabBar.Refresh(mb_FarActive);
    }

	return m_ProcCount;
}

void CConEmuMain::CheckBufferSize()
{
    CONSOLE_SCREEN_BUFFER_INFO inf; memset(&inf, 0, sizeof(inf));
    GetConsoleScreenBufferInfo(pVCon->hConOut(), &inf);
    if (inf.dwSize.X>(inf.srWindow.Right-inf.srWindow.Left+1)) {
        DEBUGLOGFILE("Wrong screen buffer width\n");
		// ������ ������� ������-�� ����������� �� �����������
        MOVEWINDOW(ghConWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 1);
    } else if ((gSet.BufferHeight == 0) && (inf.dwSize.Y>(inf.srWindow.Bottom-inf.srWindow.Top+1))) {
        DEBUGLOGFILE("Wrong screen buffer height\n");
		// ������ ������� ������-�� ����������� �� ���������
        MOVEWINDOW(ghConWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 1);
    }

	// ��� ������ �� FAR -> CMD � BufferHeight - ����� QuickEdit ������
	DWORD mode = 0;
	BOOL lb = FALSE;
	if (gSet.BufferHeight) {
		lb = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);

		if (inf.dwSize.Y>(inf.srWindow.Bottom-inf.srWindow.Top+1)) {
			// ����� ������ ������ ����
			mode |= ENABLE_QUICK_EDIT_MODE|ENABLE_INSERT_MODE|ENABLE_EXTENDED_FLAGS;
		} else {
			// ����� ����� ������ ���� (������ ��� ����������)
			mode &= ~(ENABLE_QUICK_EDIT_MODE|ENABLE_INSERT_MODE);
			mode |= ENABLE_EXTENDED_FLAGS;
		}

		lb = SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);
	}
}

LRESULT CALLBACK CConEmuMain::WndProc(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

	if (messg == WM_SYSCHAR) {
        #ifdef _DEBUG
        /*{
	        TCHAR szDbg[32];
	        wsprintf(szDbg, _T("SysChar - %c (%i)"),
		        (TCHAR)wParam, wParam);
		    SetWindowText(ghWnd, szDbg);
        }*/
        #endif
		return TRUE;
    } else if (messg == WM_CHAR) {
        #ifdef _DEBUG
        /*{
	        TCHAR szDbg[32];
	        wsprintf(szDbg, _T("Char - %c (%i)"),
		        (TCHAR)wParam, wParam);
		    SetWindowText(ghWnd, szDbg);
        }*/
        #endif
    }

    switch (messg)
    {
    case WM_NOTIFY:
    {
        result = TabBar.OnNotify((LPNMHDR)lParam);
        break;
    }

    case WM_COPYDATA:
    {
        PCOPYDATASTRUCT cds = PCOPYDATASTRUCT(lParam);
		result = OnCopyData(cds);
        break;
    }
    
    case WM_ERASEBKGND:
		return 0;
		
	case WM_PAINT:
		result = gConEmu.OnPaint(wParam, lParam);
	
    case WM_TIMER:
		result = gConEmu.OnTimer(wParam, lParam);
        break;

    case WM_SIZING:
		result = gConEmu.OnSizing(wParam, lParam);
        break;

	case WM_SIZE:
		result = gConEmu.OnSize(wParam, LOWORD(lParam), HIWORD(lParam));
        break;

	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO pInfo = (LPMINMAXINFO)lParam;
			result = OnGetMinMaxInfo(pInfo);
			break;
		}

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
		result = OnKeyboard(hWnd, messg, wParam, lParam);
		break;

    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_KILLFOCUS:
    case WM_SETFOCUS:
		result = OnFocus(hWnd, messg, wParam, lParam);
		break;

    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
		result = OnMouse(hWnd, messg, wParam, lParam);
		break;

    case WM_CLOSE:
		result = OnClose(hWnd);
        break;

    case WM_CREATE:
		result = OnCreate(hWnd);
        break;

    case WM_SYSCOMMAND:
		result = OnSysCommand(hWnd, wParam, lParam);
        break;

	case WM_NCLBUTTONDOWN:
		gConEmu.mb_InSizing = (messg==WM_NCLBUTTONDOWN);
		result = DefWindowProc(hWnd, messg, wParam, lParam);
		break;

    case WM_NCRBUTTONUP:
        Icon.HideWindowToTray();
        break;

    case WM_TRAYNOTIFY:
		result = Icon.OnTryIcon(hWnd, messg, wParam, lParam);
        break; 


    case WM_DESTROY:
		result = OnDestroy(hWnd);
        break;
    
    /*case WM_INPUTLANGCHANGE:
    case WM_INPUTLANGCHANGEREQUEST:
    case WM_IME_NOTIFY:*/
    case WM_VSCROLL:
        POSTMESSAGE(ghConWnd, messg, wParam, lParam, FALSE);
        
    default:
        if (messg) result = DefWindowProc(hWnd, messg, wParam, lParam);
    }
    return result;
}


LRESULT CConEmuMain::OnFocus(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	/*
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_KILLFOCUS:
    case WM_SETFOCUS:
	*/


    /*if (messg == WM_SETFOCUS) {
	    //TODO: �������� Options
		if (ghOpWnd) {
			TCHAR szClass[128], szTitle[255], szMsg[1024];

			GetClassName(hWnd, szClass, 64);
			GetWindowText(hWnd, szTitle, 255);

			wsprintf(szMsg, _T("WM_SETFOCUS to (HWND=0x%08x)\r\n%s - %s\r\n"),
				(DWORD)hWnd, szClass, szTitle);

			if (!wParam)
				wParam = (WPARAM)GetFocus();
			if (wParam) {
				GetClassName((HWND)wParam, szClass, 64);
				GetWindowText((HWND)wParam, szTitle, 255);
				wsprintf(szMsg+_tcslen(szMsg), _T("from (HWND=0x%08x)\r\n%s - %s\r\n"),
					(DWORD)hWnd, szClass, szTitle);
			}
			MBoxA(szMsg);
		}
		else if (ghWndDC && IsWindow(ghWndDC)) {
			SetFocus(ghWndDC);
		}
	}*/

    /*if (messg == WM_SETFOCUS || messg == WM_KILLFOCUS) {
        if (hPictureView && IsWindow(hPictureView)) {
            break; // � FAR �� ����������
        }
    }*/

    POSTMESSAGE(ghConWnd, messg, wParam, lParam, FALSE);
	return 0;
}

LRESULT CConEmuMain::OnSysCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
    switch(LOWORD(wParam))
    {
    case ID_SETTINGS:
        if (ghOpWnd && IsWindow(ghOpWnd)) {
	        ShowWindow ( ghOpWnd, SW_SHOWNORMAL );
	        SetFocus ( ghOpWnd );
	        break; // � �� ����������� ��������� ���� �������� :)
	    }
		DialogBox((HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), 0, CSettings::wndOpProc);
        break;
    case ID_HELP:
        {
	        WCHAR szTitle[255];
	        wsprintf(szTitle, L"About ConEmu (%s)...", gConEmu.szConEmuVersion);
            MessageBox(ghOpWnd, pHelp, szTitle, MB_ICONQUESTION);
        }
        break;
    case ID_TOTRAY:
        Icon.HideWindowToTray();
        break;
    case ID_CONPROP:
        #ifdef _DEBUG
        {
            HMENU hMenu = GetSystemMenu(ghConWnd, FALSE);
            MENUITEMINFO mii; TCHAR szText[255];
            for (int i=0; i<15; i++) {
                memset(&mii, 0, sizeof(mii));
                mii.cbSize = sizeof(mii); mii.dwTypeData=szText; mii.cch=255;
                mii.fMask = MIIM_ID|MIIM_STRING;
                if (GetMenuItemInfo(hMenu, i, TRUE, &mii)) {
                    mii.cbSize = sizeof(mii);
                } else
                    break;
            }
        }
        #endif
        POSTMESSAGE(ghConWnd, WM_SYSCOMMAND, 65527, 0, TRUE);
        break;
    }

    switch(wParam)
    {
    case SC_MAXIMIZE_SECRET:
        gConEmu.SetWindowMode(rMaximized);
        break;
    case SC_RESTORE_SECRET:
        gConEmu.SetWindowMode(rNormal);
        break;
    case SC_CLOSE:
        Icon.Delete();
        SENDMESSAGE(ghConWnd, WM_CLOSE, 0, 0);
        break;

    case SC_MAXIMIZE:
        if (wParam == SC_MAXIMIZE)
            CheckRadioButton(ghOpWnd, rNormal, rFullScreen, rMaximized);
    case SC_RESTORE:
        if (wParam == SC_RESTORE)
            CheckRadioButton(ghOpWnd, rNormal, rFullScreen, rNormal);

    default:
        if (wParam != 0xF100)
        {
            POSTMESSAGE(ghConWnd, WM_SYSCOMMAND, wParam, lParam, FALSE);
            result = DefWindowProc(hWnd, WM_SYSCOMMAND, wParam, lParam);
        }
    }
	return result;
}

LRESULT CConEmuMain::OnClose(HWND hWnd)
{
    //Icon.Delete(); - ������� � WM_DESTROY
	//gConEmu.mb_InClose = TRUE;
    SENDMESSAGE(ghConWnd, WM_CLOSE, 0, 0);
	//gConEmu.mb_InClose = FALSE;
	return 0;
}

LRESULT CConEmuMain::OnDestroy(HWND hWnd)
{
    Icon.Delete();
    if (gConEmu.DragDrop) {
        delete gConEmu.DragDrop;
        gConEmu.DragDrop = NULL;
    }
    if (gConEmu.ProgressBars) {
        delete gConEmu.ProgressBars;
        gConEmu.ProgressBars = NULL;
    }
    PostQuitMessage(0);

	return 0;
}

LRESULT CConEmuMain::OnKeyboard(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

    //#ifdef _DEBUG
    //{
    //    TCHAR szDbg[32];
    //    wsprintf(szDbg, _T("%s - %c (%i)"),
	//        ((messg == WM_KEYDOWN) ? _T("Dn") : _T("Up")),
	//        (TCHAR)wParam, wParam);
	//    SetWindowText(ghWnd, szDbg);
    //}
    //#endif

	if (messg == WM_KEYDOWN || messg == WM_KEYUP)
	{
		if (wParam == VK_PAUSE && !isPressed(VK_CONTROL)) {
			if (gConEmu.isPictureView()) {
				if (messg == WM_KEYUP) {
					gConEmu.bPicViewSlideShow = !gConEmu.bPicViewSlideShow;
					if (gConEmu.bPicViewSlideShow) {
						if (gSet.nSlideShowElapse<=500) gSet.nSlideShowElapse=500;
						//SetTimer(hWnd, 3, gSet.nSlideShowElapse, NULL);
						gConEmu.dwLastSlideShowTick = GetTickCount() - gSet.nSlideShowElapse;
					//} else {
					//  KillTimer(hWnd, 3);
					}
				}
				return 0;
			}
		} else if (gConEmu.bPicViewSlideShow) {
			//KillTimer(hWnd, 3);
			if (wParam==0xbd/* -_ */ || wParam==0xbb/* =+ */) {
				if (messg == WM_KEYDOWN) {
					if (wParam==0xbb)
						gSet.nSlideShowElapse = 1.2 * gSet.nSlideShowElapse;
					else {
						gSet.nSlideShowElapse = gSet.nSlideShowElapse / 1.2;
						if (gSet.nSlideShowElapse<=500) gSet.nSlideShowElapse=500;
					}
				}
				return 0;
			} else {
				gConEmu.bPicViewSlideShow = false; // ������ ��������
			}
		}
	}


  // buffer mode: scroll with keys  -- NightRoman
    if (gSet.BufferHeight && messg == WM_KEYDOWN && isPressed(VK_CONTROL))
    {
        switch(wParam)
        {
        case VK_DOWN:
            POSTMESSAGE(ghConWnd, WM_VSCROLL, SB_LINEDOWN, NULL, FALSE);
            return 0;
        case VK_UP:
            POSTMESSAGE(ghConWnd, WM_VSCROLL, SB_LINEUP, NULL, FALSE);
            return 0;
        case VK_NEXT:
            POSTMESSAGE(ghConWnd, WM_VSCROLL, SB_PAGEDOWN, NULL, FALSE);
            return 0;
        case VK_PRIOR:
            POSTMESSAGE(ghConWnd, WM_VSCROLL, SB_PAGEUP, NULL, FALSE);
            return 0;
        }
    }

    if (messg == WM_KEYDOWN && wParam == VK_SPACE && isPressed(VK_CONTROL) && isPressed(VK_LWIN) && isPressed(VK_MENU))
    {
        if (!IsWindowVisible(ghConWnd))
        {
            gConEmu.isShowConsole = true;
            ShowWindow(ghConWnd, SW_SHOWNORMAL);
            //if (gConEmu.setParent) SetParent(ghConWnd, 0);
            EnableWindow(ghConWnd, true);
        }
        else
        {
            gConEmu.isShowConsole = false;
            if (!gSet.isConVisible) ShowWindow(ghConWnd, SW_HIDE);
            //if (gConEmu.setParent) SetParent(ghConWnd, HDCWND);
            if (!gSet.isConVisible) EnableWindow(ghConWnd, false);
        }
        return 0;
    }

    if (gConEmu.gb_ConsoleSelectMode && messg == WM_KEYDOWN && ((wParam == VK_ESCAPE) || (wParam == VK_RETURN)))
        gConEmu.gb_ConsoleSelectMode = false; //TODO: ����� ���-�� �� ������� ����������?

    // �������� ��������� 
    {
        if (messg == WM_SYSKEYDOWN) 
            if (wParam == VK_INSERT && lParam & 29)
                gConEmu.gb_ConsoleSelectMode = true;

        static bool isSkipNextAltUp = false;
        if (messg == WM_SYSKEYDOWN && wParam == VK_RETURN && lParam & 29)
        {
            if (gSet.isSentAltEnter)
            {
                POSTMESSAGE(ghConWnd, WM_KEYDOWN, VK_MENU, 0, TRUE);
                POSTMESSAGE(ghConWnd, WM_KEYDOWN, VK_RETURN, 0, TRUE);
                POSTMESSAGE(ghConWnd, WM_KEYUP, VK_RETURN, 0, TRUE);
                POSTMESSAGE(ghConWnd, WM_KEYUP, VK_MENU, 0, TRUE);
            }
            else
            {
                if (isPressed(VK_SHIFT))
                    return 0;

                if (!gSet.isFullScreen)
                    gConEmu.SetWindowMode(rFullScreen);
                else
                    gConEmu.SetWindowMode(gConEmu.isWndNotFSMaximized ? rMaximized : rNormal);

                isSkipNextAltUp = true;
                //POSTMESSAGE(ghConWnd, messg, wParam, lParam);
            }
        }
        else if (messg == WM_SYSKEYDOWN && wParam == VK_SPACE && lParam & 29 && !isPressed(VK_SHIFT))
        {
            RECT rect, cRect;
            GetWindowRect(ghWnd, &rect);
            GetClientRect(ghWnd, &cRect);
            WINDOWINFO wInfo;   GetWindowInfo(ghWnd, &wInfo);
            gConEmu.ShowSysmenu(ghWnd, ghWnd, rect.right - cRect.right - wInfo.cxWindowBorders, rect.bottom - cRect.bottom - wInfo.cyWindowBorders);
        }
        else if (messg == WM_KEYUP && wParam == VK_MENU && isSkipNextAltUp) isSkipNextAltUp = false;
        else if (messg == WM_SYSKEYDOWN && wParam == VK_F9 && lParam & 29 && !isPressed(VK_SHIFT))
            gConEmu.SetWindowMode(IsZoomed(ghWnd) ? rNormal : rMaximized);
        else
            POSTMESSAGE(ghConWnd, messg, wParam, lParam, FALSE);
    }

	return 0;
}

LRESULT CConEmuMain::OnMouse(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
    short winX = GET_X_LPARAM(lParam);
    short winY = GET_Y_LPARAM(lParam);

    RECT conRect, consoleRect;
	POINT ptCur;

	// ����� � ������� ������������� ������ �� ���������� ���� ����...
	if (messg==WM_LBUTTONDOWN || messg==WM_RBUTTONDOWN || messg==WM_MBUTTONDOWN || 
		messg==WM_LBUTTONDBLCLK || messg==WM_RBUTTONDBLCLK || messg==WM_MBUTTONDBLCLK)
	{
		GetCursorPos(&ptCur);
		GetWindowRect(ghWndDC, &consoleRect);
		if (!PtInRect(&consoleRect, ptCur))
			return 0;
	}

    GetClientRect(ghConWnd, &conRect);

	memset(&consoleRect, 0, sizeof(consoleRect));
    winX -= consoleRect.left;
    winY -= consoleRect.top;
    short newX = MulDiv(winX, conRect.right, klMax<uint>(1, pVCon->Width));
    short newY = MulDiv(winY, conRect.bottom, klMax<uint>(1, pVCon->Height));

    if (newY<0 || newX<0)
        return 0;

    if (gSet.BufferHeight)
    {
       // buffer mode: cheat the console window: adjust its position exactly to the cursor
       RECT win;
       GetWindowRect(ghWnd, &win);
       short x = win.left + winX - newX;
       short y = win.top + winY - newY;
       RECT con;
       GetWindowRect(ghConWnd, &con);
       if (con.left != x || con.top != y)
          MOVEWINDOW(ghConWnd, x, y, con.right - con.left + 1, con.bottom - con.top + 1, TRUE);
    }
    else if (messg == WM_MOUSEMOVE)
    {
        // WM_MOUSEMOVE ����� �� ������� ����� ���������� ���� ��� ������� ��� ������ �� ���������...
        if (wParam==gConEmu.lastMMW && lParam==gConEmu.lastMML) {
            return 0;
        }
        gConEmu.lastMMW=wParam; gConEmu.lastMML=lParam;

        /*if (isLBDown &&   (cursor.x-LOWORD(lParam)>DRAG_DELTA || 
                           cursor.x-LOWORD(lParam)<-DRAG_DELTA || 
                           cursor.y-HIWORD(lParam)>DRAG_DELTA || 
                           cursor.y-HIWORD(lParam)<-DRAG_DELTA))*/
        if (gConEmu.isLBDown && !PTDIFFTEST(gConEmu.cursor,DRAG_DELTA) 
			&& !gConEmu.isDragProcessed && !gConEmu.mb_InSizing)
        {
            // ���� ������� ����� ��� �� �������� ������, �� ����� LClick �� ����� �� �� �������� - �������� ShellDrag
            if (!gConEmu.isFilePanel()) {
	            gConEmu.isLBDown = false;
	            return 0;
            }
            // ����� ������ ����������� FAR'������ D'n'D
            //SENDMESSAGE(ghConWnd, WM_LBUTTONUP, wParam, MAKELPARAM( newX, newY ));     //�������� ������� ����������
			if (gConEmu.DragDrop)
				gConEmu.DragDrop->Drag(); //���������� ��� ������� �����
			//isDragProcessed=false; -- �����, ����� ��� �������� � ��������� ������ ������� ������ ���� ����� ��������� ��� ���???
            POSTMESSAGE(ghConWnd, WM_LBUTTONUP, wParam, MAKELPARAM( newX, newY ), TRUE);     //�������� ������� ����������
            return 0;
        }
        else if (gSet.isRClickSendKey && gConEmu.isRBDown)
        {
            //���� ������� ������, � ���� �������� ����� RClick - �� ��������
            //����������� ���� - ������ ������� ������ ����
            if (!PTDIFFTEST(gConEmu.Rcursor, RCLICKAPPSDELTA))
            {
                gConEmu.isRBDown=false;
                POSTMESSAGE(ghConWnd, WM_RBUTTONDOWN, 0, MAKELPARAM( gConEmu.RBDownNewX, gConEmu.RBDownNewY ), TRUE);
            }
            return 0;
        }
        /*if (!isRBDown && (wParam==MK_RBUTTON)) {
            // ����� ��� ��������� ������ ������� ����� �� ������������
            if ((newY-RBDownNewY)>5) {// ���� ��������� ��� ������ ������ ����
                for (short y=RBDownNewY;y<newY;y+=5)
                    POSTMESSAGE(ghConWnd, WM_MOUSEMOVE, wParam, MAKELPARAM( RBDownNewX, y ), TRUE);
            }
            RBDownNewX=newX; RBDownNewY=newY;
        }*/
    } else {
        gConEmu.lastMMW=-1; gConEmu.lastMML=-1;

        if (messg == WM_LBUTTONDOWN)
        {
            if (gConEmu.isLBDown) ReleaseCapture(); // ����� ��������?
            gConEmu.isLBDown=false;
            if (!gConEmu.isConSelectMode() && gConEmu.isFilePanel())
            {
                SetCapture(ghWndDC);
                gConEmu.cursor.x = LOWORD(lParam);
                gConEmu.cursor.y = HIWORD(lParam); 
                gConEmu.isLBDown=true;
                gConEmu.isDragProcessed=false;
                POSTMESSAGE(ghConWnd, messg, wParam, MAKELPARAM( newX, newY ), FALSE); // ���� SEND
                return 0;
            }
        }
        else if (messg == WM_LBUTTONUP)
        {
            if (gConEmu.isLBDown) {
                gConEmu.isLBDown=false;
                ReleaseCapture();
            }
        }
        else if (messg == WM_RBUTTONDOWN)
        {
            gConEmu.Rcursor.x = LOWORD(lParam);
            gConEmu.Rcursor.y = HIWORD(lParam);
            gConEmu.RBDownNewX=newX;
            gConEmu.RBDownNewY=newY;
            gConEmu.isRBDown=false;

            // ���� ������ ������� �� ������!
            if (gSet.isRClickSendKey && !(wParam&(MK_CONTROL|MK_LBUTTON|MK_MBUTTON|MK_SHIFT|MK_XBUTTON1|MK_XBUTTON2)))
            {
                //TCHAR *BrF = _tcschr(Title, '{'), *BrS = _tcschr(Title, '}'), *Slash = _tcschr(Title, '\\');
                //if (BrF && BrS && Slash && BrF == Title && (Slash == Title+1 || Slash == Title+3))
                if (gConEmu.isFilePanel()) // Maximus5
                {
                    //������� ������ �� .3
                    //���� ������ - ������ apps
                    gConEmu.isRBDown=true; gConEmu.ibSkilRDblClk=false;
                    //SetTimer(hWnd, 1, 300, 0); -- Maximus5, ��������� �� �������
                    gConEmu.dwRBDownTick = GetTickCount();
                    return 0;
                }
            }
        }
        else if (messg == WM_RBUTTONUP)
        {
            if (gSet.isRClickSendKey && gConEmu.isRBDown)
            {
                gConEmu.isRBDown=false; // ����� �������!
                if (PTDIFFTEST(gConEmu.Rcursor,RCLICKAPPSDELTA))
                {
                    //������� ������� <.3
                    //����� ������, �������� ������ �������
                    //KillTimer(hWnd, 1); -- Maximus5, ������ ����� �� ������������
                    DWORD dwCurTick=GetTickCount();
                    DWORD dwDelta=dwCurTick-gConEmu.dwRBDownTick;
                    // ���� ������� ������ .3�, �� �� ������� ����� :)
                    if ((gSet.isRClickSendKey==1) ||
                        (dwDelta>RCLICKAPPSTIMEOUT && dwDelta<10000))
                    {
                        // ������� �������� ���� ��� ��������
                        POSTMESSAGE(ghConWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM( gConEmu.RBDownNewX, gConEmu.RBDownNewY ), TRUE);
                        POSTMESSAGE(ghConWnd, WM_LBUTTONUP, 0, MAKELPARAM( gConEmu.RBDownNewX, gConEmu.RBDownNewY ), TRUE);
                    
                        pVCon->Update(true);
                        INVALIDATE(); //InvalidateRect(HDCWND, NULL, FALSE);

                        // � ������ ����� � Apps ������
						gConEmu.ibSkilRDblClk=true; // ����� ���� FAR ������ � ������� �� ���������� ������� ���������
                        POSTMESSAGE(ghConWnd, WM_KEYDOWN, VK_APPS, 0, TRUE);
                        return 0;
                    }
                }
                // ����� ����� ������� ������� WM_RBUTTONDOWN
                POSTMESSAGE(ghConWnd, WM_RBUTTONDOWN, wParam, MAKELPARAM( newX, newY ), TRUE);
            }
            gConEmu.isRBDown=false; // ����� �� �������� ��������
        }
		else if (messg == WM_RBUTTONDBLCLK) {
			if (gConEmu.ibSkilRDblClk) {
				gConEmu.ibSkilRDblClk = false;
				return 0; // �� ������������, ������ ����� ����������� ����
			}
		}
    }

	// ������� �������� ������ �������
    POSTMESSAGE(ghConWnd, messg == WM_RBUTTONDBLCLK ? WM_RBUTTONDOWN : messg, wParam, MAKELPARAM( newX, newY ), FALSE);
    return 0;
}

BOOL WINAPI CConEmuMain::HandlerRoutine(DWORD dwCtrlType)
{
    return (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT ? true : false);
}

bool CConEmuMain::LoadVersionInfo(wchar_t* pFullPath)
{
    LPBYTE pBuffer=NULL;
    wchar_t* pVersion=NULL;
    wchar_t* pDesc=NULL;
    
    const wchar_t WSFI[] = L"StringFileInfo";

    DWORD size = GetFileVersionInfoSizeW(pFullPath, &size);
    if(!size) return false;
    pBuffer = new BYTE[size];
    GetFileVersionInfoW((wchar_t*)pFullPath, 0, size, pBuffer);

    //Find StringFileInfo
    DWORD ofs;
    for(ofs = 92; ofs < size; ofs += *(WORD*)(pBuffer+ofs) )
        if(!lstrcmpiW((wchar_t*)(pBuffer+ofs+6), WSFI))
            break;
    if(ofs >= size) {
        delete pBuffer;
        return false;
    }
    TCHAR *langcode;
    langcode = (TCHAR*)(pBuffer + ofs + 42);

    TCHAR blockname[48];
    unsigned dsize;

    wsprintf(blockname, _T("\\%s\\%s\\FileVersion"), WSFI, langcode);
    if(!VerQueryValue(pBuffer, blockname, (void**)&pVersion, &dsize))
        pVersion = 0;
    else {
       if (dsize>=31) pVersion[31]=0;
       wcscpy(szConEmuVersion, pVersion);
       pVersion = wcsrchr(szConEmuVersion, L',');
       if (pVersion && wcscmp(pVersion, L", 0")==0)
	       *pVersion = 0;
    }
    
    delete pBuffer;
    
    return true;
}

// ����� �������� ����� �������� ��������!
void CConEmuMain::LoadIcons()
{
    if (hClassIcon)
	    return; // ��� ���������
	    
    if (GetModuleFileName(0, szIconPath, MAX_PATH))
    {
        this->LoadVersionInfo(szIconPath);
        
	    TCHAR *lpszExt = _tcsrchr(szIconPath, _T('.'));
	    if (!lpszExt)
		    szIconPath[0] = 0;
		else {
			_tcscpy(lpszExt, _T(".ico"));
	        DWORD dwAttr = GetFileAttributes(szIconPath);
	        if (dwAttr==-1 || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
	            szIconPath[0]=0;
	    }
    } else {
        szIconPath[0]=0;
    }
    
    if (szIconPath[0]) {
	    hClassIcon = (HICON)LoadImage(0, szIconPath, IMAGE_ICON, 
		    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR|LR_LOADFROMFILE);
	    hClassIconSm = (HICON)LoadImage(0, szIconPath, IMAGE_ICON, 
		    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR|LR_LOADFROMFILE);
	}
    if (!hClassIcon) {
	    szIconPath[0]=0;
	    
	    hClassIcon = (HICON)LoadImage(GetModuleHandle(0), 
		    MAKEINTRESOURCE(gSet.nIconID), IMAGE_ICON, 
		    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
		    
	    if (hClassIconSm) DestroyIcon(hClassIconSm);
	    hClassIconSm = (HICON)LoadImage(GetModuleHandle(0), 
		    MAKEINTRESOURCE(gSet.nIconID), IMAGE_ICON, 
		    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    }
}

LRESULT CConEmuMain::OnPaint(WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

    PAINTSTRUCT ps;
    HDC hDc = BeginPaint(ghWnd, &ps);

	PaintGaps(hDc);

	EndPaint(ghWnd, &ps);
	result = DefWindowProc(ghWnd, WM_PAINT, wParam, lParam);

	return result;
}

void CConEmuMain::ReSize()
{
	if (IsIconic(ghWnd))
		return;

	RECT client; GetClientRect(ghWnd, &client);

	OnSize(IsZoomed(ghWnd) ? SIZE_MAXIMIZED : SIZE_RESTORED,
		client.right, client.bottom);
}

LRESULT CConEmuMain::OnSize(WPARAM wParam, WORD newClientWidth, WORD newClientHeight)
{
	LRESULT result = 0;

	if (TabBar.IsActive())
        TabBar.UpdateWidth();
	m_Back.Resize();

	if (!gConEmu.mb_InSizing || gConEmu.mb_FullWindowDrag) {
		BOOL lbIsPicView = isPictureView();

		SyncConsoleToWindow();
	    
		RECT client; memset(&client, 0, sizeof(client));
		client.right = newClientWidth;
		client.bottom = newClientHeight;
		DCClientRect(&client);

		RECT rcNewCon; memset(&rcNewCon,0,sizeof(rcNewCon));
		if (pVCon && pVCon->Width && pVCon->Height) {
			if (gSet.isTryToCenter && (IsZoomed(ghWnd) || gSet.isFullScreen)) {
				rcNewCon.left = (client.right+client.left-(int)pVCon->Width)/2;
				rcNewCon.top = (client.bottom+client.top-(int)pVCon->Height)/2;
			}

			if (rcNewCon.left<client.left) rcNewCon.left=client.left;
			if (rcNewCon.top<client.top) rcNewCon.top=client.top;

			rcNewCon.right = rcNewCon.left + pVCon->Width;
				if (rcNewCon.right>client.right) rcNewCon.right=client.right;
			rcNewCon.bottom = rcNewCon.top + pVCon->Height;
				if (rcNewCon.bottom>client.bottom) rcNewCon.bottom=client.bottom;
		} else {
			rcNewCon = client;
		}
		MoveWindow(ghWndDC, rcNewCon.left, rcNewCon.top, rcNewCon.right - rcNewCon.left, rcNewCon.bottom - rcNewCon.top, 1);
		//PaintGaps();
		dcWindowLast = rcNewCon;
		//InvalidateRect(ghWnd, NULL, FALSE);
	    
		if (!lbIsPicView)
		{
			/*if (mb_FullWindowDrag)
			{
				// ������ ���������� �����
				RECT pRect = {0, 0, newClientWidth, newClientHeight};

				COORD srctWindow = ConsoleSizeFromWindow(&pRect);

				if ((srctWindowLast.X != srctWindow.X 
					|| srctWindowLast.Y != srctWindow.Y))
				{
					SetConsoleWindowSize(srctWindow, true);
					srctWindowLast = srctWindow;
				}
			}*/

			{
				/*static bool wPrevSizeMax = false;
				if ((wParam == SIZE_MAXIMIZED || (wParam == SIZE_RESTORED && wPrevSizeMax)) && ghConWnd)
				{
					wPrevSizeMax = wParam == SIZE_MAXIMIZED;

					RECT pRect;
					GetWindowRect(ghWnd, &pRect);
					pRect.right = newClientWidth + pRect.left + cwShift.x;
					pRect.bottom = newClientHeight + pRect.top + cwShift.y;

					//TODO: ���� ����������, ��� ���� ���� �������� ���� � ����� ������ �� �����...
					// fake WM_SIZING event to adjust console size to new window size after Maximize or Restore Down 
					//SendMessage(ghWnd, WM_SIZING, WMSZ_TOP, (LPARAM)&pRect);
					//SendMessage(ghWnd, WM_SIZING, WMSZ_RIGHT, (LPARAM)&pRect);
					//������� �� WndProc!
					WndProc ( ghWnd, WM_SIZING, WMSZ_TOP, (LPARAM)&pRect );
					WndProc ( ghWnd, WM_SIZING, WMSZ_RIGHT, (LPARAM)&pRect );
				}*/

			}
		}
	}

	return result;
}

LRESULT CConEmuMain::OnSizing(WPARAM wParam, LPARAM lParam)
{
	LRESULT result = true;

	if (!gSet.isFullScreen && !IsZoomed(ghWnd)) {
		COORD srctWindow;
		RECT wndSizeRect, restrictRect;
		RECT *pRect = (RECT*)lParam; // � ������

		wndSizeRect = *pRect;
		// ��� ���������� ����� ��� ������
		if (gSet.LogFont.lfWidth && gSet.LogFont.lfHeight) {
			wndSizeRect.right += (gSet.LogFont.lfWidth-1)/2;
			wndSizeRect.bottom += (gSet.LogFont.lfHeight-1)/2;
		}

		// ���������� �������� ������ �������
		srctWindow = ConsoleSizeFromWindow(&wndSizeRect, true /* frameIncluded */);

		// ���������� ���������� ������� �������
		if (srctWindow.X<28) srctWindow.X=28;
		if (srctWindow.Y<9)  srctWindow.Y=9;

		/*if ((srctWindowLast.X != srctWindow.X 
			|| srctWindowLast.Y != srctWindow.Y) 
			&& !mb_FullWindowDrag)
		{
			SetConsoleWindowSize(srctWindow, true);
			srctWindowLast = srctWindow;
		}*/

		//RECT consoleRect = ConsoleOffsetRect();
		wndSizeRect = WindowSizeFromConsole(srctWindow, true /* rectInWindow */);

		restrictRect.right = pRect->left + wndSizeRect.right;
		restrictRect.bottom = pRect->top + wndSizeRect.bottom;
		restrictRect.left = pRect->right - wndSizeRect.right;
		restrictRect.top = pRect->bottom - wndSizeRect.bottom;
	    

		switch(wParam)
		{
		case WMSZ_RIGHT:
		case WMSZ_BOTTOM:
		case WMSZ_BOTTOMRIGHT:
			pRect->right = restrictRect.right;
			pRect->bottom = restrictRect.bottom;
			break;
		case WMSZ_LEFT:
		case WMSZ_TOP:
		case WMSZ_TOPLEFT:
			pRect->left = restrictRect.left;
			pRect->top = restrictRect.top;
			break;
		case WMSZ_TOPRIGHT:
			pRect->right = restrictRect.right;
			pRect->top = restrictRect.top;
			break;
		case WMSZ_BOTTOMLEFT:
			pRect->left = restrictRect.left;
			pRect->bottom = restrictRect.bottom;
			break;
		}
	}

	return result;
}

LRESULT CConEmuMain::OnTimer(WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

    switch (wParam)
    {
    case 0:
        HWND foreWnd = GetForegroundWindow();
        if (!isShowConsole && !gSet.isConVisible)
        {
            /*if (foreWnd == ghConWnd)
                SetForegroundWindow(ghWnd);*/
            if (IsWindowVisible(ghConWnd))
                ShowWindow(ghConWnd, SW_HIDE);
        }

		//Maximus5. Hack - ���� �����-�� ������ ����������� ����
		DWORD dwStyle = GetWindowLong(ghWnd, GWL_STYLE);
		if (dwStyle & WS_DISABLED)
			EnableWindow(ghWnd, TRUE);

		CheckProcesses();
		if (gnLastProcessCount == 1) {
			DestroyWindow(ghWnd);
			break;
		}


		BOOL lbIsPicView = isPictureView();
        if (bPicViewSlideShow) {
            DWORD dwTicks = GetTickCount();
            DWORD dwElapse = dwTicks - dwLastSlideShowTick;
            if (dwElapse > gSet.nSlideShowElapse)
            {
                if (IsWindow(hPictureView)) {
                    //
                    bPicViewSlideShow = false;
                    SendMessage(ghConWnd, WM_KEYDOWN, VK_NEXT, 0x01510001);
                    SendMessage(ghConWnd, WM_KEYUP, VK_NEXT, 0xc1510001);

                    // ���� ����� ����������?
                    isPictureView();

                    dwLastSlideShowTick = GetTickCount();
                    bPicViewSlideShow = true;
                } else {
                    hPictureView = NULL;
                    bPicViewSlideShow = false;
                }
            }
        }

        if (cBlinkNext++ >= cBlinkShift)
        {
            cBlinkNext = 0;
            if (foreWnd == ghWnd || foreWnd == ghOpWnd)
                // switch cursor
                pVCon->Cursor.isVisible = !pVCon->Cursor.isVisible;
            else
                // turn cursor off
                pVCon->Cursor.isVisible = false;
        }

        /*DWORD ProcList[2];
        if(GetConsoleProcessList(ProcList,2)==1)
        {
          DestroyWindow(ghWnd);
          break;
        }*/

        GetWindowText(ghConWnd, TitleCmp, 1024);
        if (wcscmp(Title, TitleCmp))
        {
            wcscpy(Title, TitleCmp);
            SetWindowText(ghWnd, Title);
        }

        TabBar.OnTimer();
        ProgressBars->OnTimer();

        
		if (mb_InSizing && !isPressed(VK_LBUTTON)) {
			mb_InSizing = FALSE;
			if (!mb_FullWindowDrag)
				ReSize();
		}

        if (lbIsPicView)
        {
            bool lbOK = true;
            if (!setParent) {
                // ��������, ����� PictureView �������� � �������, � �� � ConEmu?
                HWND hPicView = FindWindowEx(ghConWnd, NULL, L"FarPictureViewControlClass", NULL);
                if (!hPicView) {
                    lbOK = false; // ������ ���, ��� ����� ������ �� ������
                }
            }
            if (lbOK) {
                isPiewUpdate = true;
                if (pVCon->Update(false))
                    INVALIDATE(); //InvalidateRect(HDCWND, NULL, FALSE);
                break;
            }
        } else 
        if (isPiewUpdate)
        {	// ����� �������/�������� PictureView ����� ����������� ������� - �� ������ ��� ����������
            isPiewUpdate = false;
            SyncConsoleToWindow();
            //INVALIDATE(); //InvalidateRect(HDCWND, NULL, FALSE);
			InvalidateAll();
        }

        // ���������, ����� � ������� ������ ������? (���� ��� ����� ��-�� ����...)
		CheckBufferSize();

        //if (!gbInvalidating && !gbInPaint)
        if (pVCon->Update(false/*gbNoDblBuffer*/))
        {
            COORD c = ConsoleSizeFromWindow();
            if ((mb_FullWindowDrag || !mb_InSizing) &&
				(gbPostUpdateWindowSize || c.X != pVCon->TextWidth || c.Y != pVCon->TextHeight))
            {
				gbPostUpdateWindowSize = false;
                if (!gSet.isFullScreen && !IsZoomed(ghWnd))
                    SyncWindowToConsole();
                else
                    SyncConsoleToWindow();
            }

            INVALIDATE(); //InvalidateRect(HDCWND, NULL, FALSE);

            // update scrollbar
            if (gSet.BufferHeight)
            {
                SCROLLINFO si;
                ZeroMemory(&si, sizeof(si));
                si.cbSize = sizeof(si);
                si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
                if (GetScrollInfo(ghConWnd, SB_VERT, &si))
                    SetScrollInfo(HDCWND, SB_VERT, &si, true);
            }
      //} -- � ������ NightRoman (isPiewUpdate) ����������� ������

            /*if (!lbIsPicView && isPiewUpdate)
            {
                isPiewUpdate = false;
                SyncConsoleToWindow();
                INVALIDATE(); //InvalidateRect(ghWnd, NULL, FALSE);
            }*/
        }
    }

	return result;
}


LRESULT CConEmuMain::OnCopyData(PCOPYDATASTRUCT cds)
{
	LRESULT result = 0;
    
	if (cds->dwData == 0) {
		BOOL lbInClose = FALSE;
		DWORD ProcList[2], ProcCount=0;
	    ProcCount = GetConsoleProcessList(ProcList,2);
		if (ProcCount<=2)
			lbInClose = TRUE;

		// ���� � ��������� ������� � �� ��������...
		if (!lbInClose) { // ����� ���� �� ������� ��� ������ �� ��������
			// �������� �� ������� �� ExitFAR
			ForceShowTabs(FALSE);

			//CONSOLE_SCREEN_BUFFER_INFO inf; memset(&inf, 0, sizeof(inf));
			//GetConsoleScreenBufferInfo(pVCon->hConOut(), &inf);
			if ((gSet.BufferHeight > 0) /*&& (inf.dwSize.Y==(inf.srWindow.Bottom-inf.srWindow.Top+1))*/)
			{
				DWORD mode = 0;
				BOOL lb = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);
				mode |= ENABLE_QUICK_EDIT_MODE|ENABLE_INSERT_MODE|ENABLE_EXTENDED_FLAGS;
				lb = SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);
			}
		}
	} else {
		BOOL lbNeedInval=FALSE;
		ConEmuTab* tabs = (ConEmuTab*)cds->lpData;
		// ���� �� ������� ��� ���������� edit/view ��� ���� ������� ���������� ������
		// ��� ��������� �������� ������ ConEmu ��� ���������� � ����� � FAR
		if (!TabBar.IsActive() && gSet.isTabs && (cds->dwData>1 || tabs[0].Type!=1/*WTYPE_PANELS*/ || gSet.isTabs==1))
		{
			if ((gSet.BufferHeight > 0) /*&& (inf.dwSize.Y==(inf.srWindow.Bottom-inf.srWindow.Top+1))*/)
			{
				//CONSOLE_SCREEN_BUFFER_INFO inf; memset(&inf, 0, sizeof(inf));
				//GetConsoleScreenBufferInfo(pVCon->hConOut(), &inf);
				DWORD mode = 0;
				BOOL lb = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);
				mode &= ~(ENABLE_QUICK_EDIT_MODE|ENABLE_INSERT_MODE);
				mode |= ENABLE_EXTENDED_FLAGS;
				lb = SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);
			}

			TabBar.Activate();
			lbNeedInval = TRUE;
		}
		TabBar.Update(tabs, cds->dwData);
		if (lbNeedInval)
		{
			SyncConsoleToWindow();
			//RECT rc = ConsoleOffsetRect();
			//rc.bottom = rc.top; rc.top = 0;
			InvalidateRect(ghWnd, NULL/*&rc*/, FALSE);
			if (!gSet.isFullScreen && !IsZoomed(ghWnd)) {
				//SyncWindowToConsole(); -- ��� ������ ������, �.�. FAR ��� �� ��������� ��������� �������!
				gbPostUpdateWindowSize = true;
			}
		}
	}

	return result;
}

LRESULT CConEmuMain::OnGetMinMaxInfo(LPMINMAXINFO pInfo)
{
	LRESULT result = 0;

    POINT p = cwShift;
    RECT shiftRect = ConsoleOffsetRect();

    // ���������� ���������� ������� �������
    COORD srctWindow; srctWindow.X=28; srctWindow.Y=9;

	pInfo->ptMinTrackSize.x = srctWindow.X * (gSet.LogFont.lfWidth ? gSet.LogFont.lfWidth : 4)
		+ p.x + shiftRect.left + shiftRect.right;

	pInfo->ptMinTrackSize.y = srctWindow.Y * (gSet.LogFont.lfHeight ? gSet.LogFont.lfHeight : 6)
		+ p.y + shiftRect.top + shiftRect.bottom;

	return result;
}

LRESULT CConEmuMain::OnCreate(HWND hWnd)
{
	ghWnd = hWnd; // ������ �����, ����� ������� ����� ������������
	Icon.LoadIcon(hWnd, gSet.nIconID/*IDI_ICON1*/);
	
	// ����� ����� ���� ����� ����� ���� �� ������ �������
	SetWindowLong(hWnd, GWL_USERDATA, (LONG)ghConWnd);

	gConEmu.m_Back.Create();

	if (!gConEmu.m_Child.Create())
		return -1;

	return 0;
}

void CConEmuMain::SetConParent()
{
    // set parent window of the console window:
    // *) it is used by ConMan and some FAR plugins, set it for standard mode or if /SetParent switch is set
    // *) do not set it by default for buffer mode because it causes unwanted selection jumps
    // WARP ItSelf ������� ����� �������, ��� SetParent ����� ConEmu � Windows7
    //if (!setParentDisabled && (setParent || gSet.BufferHeight == 0))
    
    //TODO: ConMan? ��������� �� ������������ ���� SetParent ������
    if (setParent)
        SetParent(ghConWnd, setParent2 ? ghWnd : ghWndDC);
}

void CConEmuMain::InvalidateAll()
{
	InvalidateRect(ghWnd, NULL, TRUE);
	InvalidateRect(ghWndDC, NULL, TRUE);
	InvalidateRect(m_Back.mh_Wnd, NULL, TRUE);
	TabBar.Invalidate();
}
