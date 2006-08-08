#include <stdio.h>
#include "DisAsm.h"
#include "resource.h"
#include "../PS2VM.h"
#include "win32/InputBox.h"
#include "WinUtils.h"

#define CLSNAME		_X("CDisAsm")
#define YSPACE		3
#define YMARGIN		1

#define ID_DISASM_GOTOPC		40001
#define ID_DISASM_GOTOADDRESS	40002
#define ID_DISASM_GOTOEA		40003
#define ID_DISASM_EDITCOMMENT	40004
#define ID_DISASM_FINDCALLERS	40005
#define ID_DISASM_GOTOPREV		40006
#define ID_DISASM_GOTONEXT		40007

using namespace Framework;

CDisAsm::CDisAsm(HWND hParent, RECT* pR, CMIPS* pCtx)
{
	SCROLLINFO si;

	HistoryReset();

	m_nArrow = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_ARROW));
	m_nArrowMask = WinUtils::CreateMask(m_nArrow, 0xFF00FF);
	
	m_nBPoint = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BREAKPOINT));
	m_nBPointMask = WinUtils::CreateMask(m_nBPoint, 0xFF00FF);

	if(!DoesWindowClassExist(CLSNAME))
	{
		WNDCLASSEX w;
		memset(&w, 0, sizeof(WNDCLASSEX));
		w.cbSize		= sizeof(WNDCLASSEX);
		w.lpfnWndProc	= CWindow::WndProc;
		w.lpszClassName	= CLSNAME;
		w.hbrBackground	= (HBRUSH)GetStockObject(WHITE_BRUSH);
		w.hInstance		= GetModuleHandle(NULL);
		w.hCursor		= LoadCursor(NULL, IDC_ARROW);
		w.style			= CS_DBLCLKS | CS_OWNDC;
		RegisterClassEx(&w);
	}

	Create(WS_EX_CLIENTEDGE, CLSNAME, _X(""), WS_VISIBLE | WS_VSCROLL | WS_CHILD, pR, hParent, NULL);
	SetClassPtr();

	m_pOnMachineStateChangeHandler = new CEventHandlerMethod<CDisAsm, int>(this, &CDisAsm::OnMachineStateChange);
	m_pOnRunningStateChangeHandler = new CEventHandlerMethod<CDisAsm, int>(this, &CDisAsm::OnRunningStateChange);

	CPS2VM::m_OnMachineStateChange.InsertHandler(m_pOnMachineStateChangeHandler);
	CPS2VM::m_OnRunningStateChange.InsertHandler(m_pOnRunningStateChangeHandler);

	m_pCtx = pCtx;

	memset(&si, 0, sizeof(SCROLLINFO));
	si.cbSize = sizeof(SCROLLINFO);
	si.nMin = 0;
	si.nMax = 0x4000;
	si.nPos = 0x2000;
	si.fMask = SIF_RANGE | SIF_POS;
	SetScrollInfo(m_hWnd, SB_VERT, &si, FALSE);

	m_nSelected = 0;
	m_nAddress = 0;

}

CDisAsm::~CDisAsm()
{
	DeleteObject(m_nArrow);
	DeleteObject(m_nArrowMask);
	DeleteObject(m_nBPoint);
	DeleteObject(m_nBPointMask);

	CPS2VM::m_OnMachineStateChange.RemoveHandler(m_pOnMachineStateChangeHandler);
	CPS2VM::m_OnRunningStateChange.RemoveHandler(m_pOnRunningStateChangeHandler);
}

void CDisAsm::SetAddress(uint32 nAddress)
{
	m_nAddress = nAddress;
	Redraw();
}

void CDisAsm::OnMachineStateChange(int nNothing)
{
	if(!IsAddressVisible(m_pCtx->m_State.nPC))
	{
		m_nAddress = m_pCtx->m_State.nPC;
	}
	Redraw();
}

void CDisAsm::OnRunningStateChange(int nNothing)
{
	Redraw();
}

HFONT CDisAsm::GetFont()
{
	//return (HFONT)GetStockObject(ANSI_FIXED_FONT);
	return CreateFont(-11, 0, 0, 0, 400, 0, 0, 0, 0, 1, 2, 1, 49, _X("Courier New"));
}

void CDisAsm::GotoAddress()
{
	xchar sTemp[256];
	const xchar* sValue;
	uint32 nAddress;
	
	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}

	xsnprintf(sTemp, countof(sTemp), _X("%0.8X"), m_nAddress);

	Win32::CInputBox i(_X("Goto Address"), _X("Enter new address:"), sTemp);

	sValue = i.GetValue(m_hWnd);
	if(sValue != NULL)
	{
		xsscanf(sValue, _X("%x"), &nAddress);
		if(nAddress & 0x03)
		{
			MessageBox(m_hWnd, _X("Invalid address"), NULL, 16);
			return;
		}

		if(m_nAddress != nAddress)
		{
			HistorySave(m_nAddress);
		}

		m_nAddress = nAddress;
		Redraw();
	}
}

void CDisAsm::GotoPC()
{
	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}

	m_nAddress = m_pCtx->m_State.nPC;
	Redraw();
}

void CDisAsm::GotoEA()
{
	uint32 nOpcode, nAddress;

	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}
	nOpcode = GetInstruction(m_nSelected);
	if(m_pCtx->m_pArch->IsInstructionBranch(m_pCtx, m_nSelected, nOpcode))
	{
		nAddress = m_pCtx->m_pArch->GetInstructionEffectiveAddress(m_pCtx, m_nSelected, nOpcode);

		if(m_nAddress != nAddress)
		{
			HistorySave(m_nAddress);
		}

		m_nAddress = nAddress;
		Redraw();
	}
}

void CDisAsm::EditComment()
{
	xchar sTemp[256];
	const xchar* sValue;
	char sConvert[256];
	const char* sComment;

	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}

	sComment = m_pCtx->m_Comments.Find(m_nSelected);

	if(sComment != NULL)
	{
		xconvert(sTemp, sComment, 256);
	}
	else
	{
		xstrcpy(sTemp, _X(""));
	}

	Win32::CInputBox i(_X("Edit Comment"), _X("Enter new comment:"), sTemp);
	sValue = i.GetValue(m_hWnd);

	if(sValue != NULL)
	{
		xconvert(sConvert, sValue, 256);
		m_pCtx->m_Comments.InsertTag(m_nSelected, sConvert);
		Redraw();
	}
}

void CDisAsm::FindCallers()
{
	int i;
	uint32 nVal;

	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}

	printf("Searching callers...\r\n");

	for(i = 0; i < CPS2VM::RAMSIZE / 4; i++)
	{
		nVal = ((uint32*)CPS2VM::m_pRAM)[i];
		if(((nVal & 0xFC000000) == 0x0C000000) || ((nVal & 0xFC000000) == 0x08000000))
		{
			nVal &= 0x3FFFFFF;
			nVal *= 4;
			if(nVal == m_nSelected)
			{
				printf("JAL: 0x%0.8X\r\n", i * 4);
			}
		}
	}

	printf("Done.\r\n");
}

unsigned int CDisAsm::GetFontHeight()
{
	HDC hDC;
	HFONT nFont;
	SIZE s;

	hDC = GetDC(m_hWnd);
	nFont = GetFont();
	SelectObject(hDC, nFont);

	GetTextExtentPoint32(hDC, _X("0"), 1, &s);

	DeleteObject(nFont);

	return s.cy;
}

unsigned int CDisAsm::GetLineCount()
{
	unsigned int nFontCY, nLines;
	RECT rwin;

	GetClientRect(&rwin);

	nFontCY = GetFontHeight();

	nLines = (rwin.bottom - (YMARGIN * 2)) / (nFontCY + YSPACE);
	nLines++;

	return nLines;
}

bool CDisAsm::IsAddressVisible(uint32 nAddress)
{
	uint32 nTop, nBottom;

	nTop = m_nAddress;
	nBottom = nTop + ((GetLineCount() - 1) * 4);

	if(nAddress < nTop) return false;
	if(nAddress > nBottom) return false;

	return true;
}

void CDisAsm::HistoryReset()
{
	m_nHistoryPosition	= -1;
	m_nHistorySize		= 0;
	memset(m_nHistory, 0, sizeof(uint32) * HISTORY_STACK_MAX);
}

void CDisAsm::HistorySave(uint32 nAddress)
{
	if(m_nHistorySize == HISTORY_STACK_MAX)
	{
		memmove(m_nHistory + 1, m_nHistory, HISTORY_STACK_MAX - 1);
		m_nHistorySize--;
	}

	m_nHistory[m_nHistorySize] = nAddress;
	m_nHistoryPosition = m_nHistorySize;
	m_nHistorySize++;
}

void CDisAsm::HistoryGoBack()
{
	uint32 nAddress;

	if(m_nHistoryPosition == -1) return;

	nAddress = HistoryGetPrevious();
	m_nHistory[m_nHistoryPosition] = m_nAddress;
	m_nAddress = nAddress;

	m_nHistoryPosition--;
	Redraw();
}

void CDisAsm::HistoryGoForward()
{
	uint32 nAddress;

	if(m_nHistoryPosition == m_nHistorySize) return;

	nAddress = HistoryGetNext();
	m_nHistoryPosition++;
	m_nHistory[m_nHistoryPosition] = m_nAddress;
	m_nAddress = nAddress;

	Redraw();
}

uint32 CDisAsm::HistoryGetPrevious()
{
	return m_nHistory[m_nHistoryPosition];
}

uint32 CDisAsm::HistoryGetNext()
{
	if(m_nHistoryPosition == m_nHistorySize) return 0;
	return m_nHistory[m_nHistoryPosition + 1];
}

bool CDisAsm::HistoryHasPrevious()
{
	return (m_nHistorySize != 0) && (m_nHistoryPosition != -1);
}

bool CDisAsm::HistoryHasNext()
{
	return (m_nHistorySize != 0) && (m_nHistoryPosition != (m_nHistorySize - 1));
}

void CDisAsm::UpdateMouseSelection(unsigned int nX, unsigned int nY)
{
	uint32 nNew;
	if(nX <= 18) return;
	nNew = nY / (GetFontHeight() + YSPACE);
	nNew = (m_nAddress + (nNew * 4));
	if(nNew == m_nSelected) return;
	m_nSelected = nNew;
	Redraw();
}

uint32 CDisAsm::GetAddressAtPosition(unsigned int nX, unsigned int nY)
{
	uint32 nAddress;
	nAddress = nY / (GetFontHeight() + YSPACE);
	nAddress = (m_nAddress + (nAddress * 4));
	return nAddress;
}

uint32 CDisAsm::GetInstruction(uint32 nAddress)
{
	//Address translation perhaps?
	return m_pCtx->m_pMemoryMap->GetWord(nAddress);
}

void CDisAsm::ToggleBreakpoint(uint32 nAddress)
{
	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}
	m_pCtx->ToggleBreakpoint(nAddress);
	Redraw();
}

void CDisAsm::UpdatePosition(int nDelta)
{
	m_nAddress += nDelta;
	Redraw();
}

long CDisAsm::OnSetFocus()
{
	if(m_nFocus) return TRUE;
	m_nFocus = true;
	Redraw();
	return TRUE;
}

long CDisAsm::OnKillFocus()
{
	m_nFocus = false;
	Redraw();
	return TRUE;
}

long CDisAsm::OnLeftButtonDown(int nX, int nY)
{
	UpdateMouseSelection(nX, nY);
	SetFocus();
	return FALSE;
}

long CDisAsm::OnRightButtonUp(int nX, int nY)
{
	POINT pt;
	HMENU hMenu;
	unsigned int nPosition;
	xchar sTemp[256];
	uint32 nAddress;
	uint32 nOpcode;

	pt.x = nX;
	pt.y = nY;
	ClientToScreen(m_hWnd, &pt);

	nPosition = 0;

	hMenu = CreatePopupMenu();
	InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_GOTOPC,			_X("Goto PC"));
	InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_GOTOADDRESS,	_X("Goto Address..."));
	InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_EDITCOMMENT,	_X("Edit Comment..."));
	InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_FINDCALLERS,	_X("Find Callers"));

	if(m_nSelected != MIPS_INVALID_PC)
	{
		nOpcode = GetInstruction(m_nSelected);
		if(m_pCtx->m_pArch->IsInstructionBranch(m_pCtx, m_nSelected, nOpcode))
		{
			nAddress = m_pCtx->m_pArch->GetInstructionEffectiveAddress(m_pCtx, m_nSelected, nOpcode);
			xsnprintf(sTemp, countof(sTemp), _X("Go to 0x%0.8X"), nAddress);
			InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_GOTOEA, sTemp);
		}
	}

	if(HistoryHasPrevious())
	{
		xsnprintf(sTemp, countof(sTemp), _X("Go back (0x%0.8X)"), HistoryGetPrevious());
		InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_GOTOPREV, sTemp);
	}

	if(HistoryHasNext())
	{
		xsnprintf(sTemp, countof(sTemp), _X("Go forward (0x%0.8X)"), HistoryGetNext());
		InsertMenu(hMenu, nPosition++, MF_BYPOSITION, ID_DISASM_GOTONEXT, sTemp);
	}

	TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, m_hWnd, NULL); 

	return FALSE;
}

long CDisAsm::OnMouseMove(WPARAM nButton, int nX, int nY)
{
	if(!(nButton & MK_LBUTTON)) return TRUE;
	UpdateMouseSelection(nX, nY);
	return FALSE;
}

long CDisAsm::OnSize(unsigned int nType, unsigned int nX, unsigned int nY)
{
	CCustomDrawn::OnSize(nType, nX, nY);
	return TRUE;
}

long CDisAsm::OnMouseWheel(short nZ)
{
	if(nZ < 0)
	{
		UpdatePosition(12);
	}
	else
	{
		UpdatePosition(-12);
	}
	return FALSE;
}

long CDisAsm::OnVScroll(unsigned int nType, unsigned int nTrackPos)
{
	switch(nType)
	{
	case SB_LINEDOWN:
		UpdatePosition(4);
		break;
	case SB_LINEUP:
		UpdatePosition(-4);
		break;
	case SB_PAGEDOWN:
		UpdatePosition(40);
		break;
	case SB_PAGEUP:
		UpdatePosition(-40);
		break;
	}
	return FALSE;
}

long CDisAsm::OnKeyDown(unsigned int nKey)
{
	switch(nKey)
	{
	case VK_F9:
		ToggleBreakpoint(m_nSelected);
		break;
	case VK_DOWN:
		m_nSelected += 4;
		if(!IsAddressVisible(m_nSelected))
		{
			UpdatePosition(4);
		}
		else
		{
			Redraw();
		}
		break;
	case VK_UP:
		m_nSelected -= 4;
		if(!IsAddressVisible(m_nSelected))
		{
			UpdatePosition(-4);
		}
		else
		{
			Redraw();
		}
		break;
	}
	return TRUE;
}

long CDisAsm::OnLeftButtonDblClk(int nX, int nY)
{
	if(nX > 18)
	{

	}
	else
	{
		ToggleBreakpoint(GetAddressAtPosition(nX, nY));
	}
	return FALSE;
}

long CDisAsm::OnCommand(unsigned short nID, unsigned short nMsg, HWND hFrom)
{
	switch(nID)
	{
	case ID_DISASM_GOTOPC:
		GotoPC();
		return FALSE;
		break;
	case ID_DISASM_GOTOADDRESS:
		GotoAddress();
		return FALSE;
		break;
	case ID_DISASM_GOTOEA:
		GotoEA();
		return FALSE;
		break;
	case ID_DISASM_EDITCOMMENT:
		EditComment();
		return FALSE;
		break;
	case ID_DISASM_FINDCALLERS:
		FindCallers();
		return FALSE;
		break;
	case ID_DISASM_GOTOPREV:
		HistoryGoBack();
		break;
	case ID_DISASM_GOTONEXT:
		HistoryGoForward();
		break;
	}
	return TRUE;
}

void CDisAsm::Paint(HDC hDC)
{
	HDC hMem;
	RECT rmarg, rwin, rsel;
	HPEN nPen, nLtGrayPen;
	HFONT nFont;
	SIZE s;
	uint32 nData, nAddress, nEffAddr;
	xchar sTemp[256];
	char sDisAsm[256];
	const char* sTag;
	int nLines, i;
	unsigned int nY;
	bool nCommentDrawn;
	MIPSSUBROUTINE* pSub;

	GetClientRect(&rwin);

	nFont = GetFont();

	BitBlt(hDC, 0, 0, rwin.right, rwin.bottom, NULL, 0, 0, WHITENESS);
	SelectObject(hDC, nFont);

	GetTextExtentPoint32(hDC, _X("0"), 1, &s);

	nLines = (rwin.bottom - (YMARGIN * 2)) / (s.cy + YSPACE);
	nLines++;

	SetRect(&rmarg, 0, 0, 17, rwin.bottom);
	FillRect(hDC, &rmarg, (HBRUSH)COLOR_WINDOW);

	nLtGrayPen = CreatePen(PS_SOLID, 2, RGB(0x40, 0x40, 0x40));

	nPen = CreatePen(PS_SOLID, 0, RGB(0x80, 0x80, 0x80));
	SelectObject(hDC, nPen);
	MoveToEx(hDC, 17, 0, NULL);
	LineTo(hDC, 17, rwin.bottom);
	DeleteObject(nPen);

	SetBkMode(hDC, TRANSPARENT);

	nY = YMARGIN;

	for(i = 0; i < nLines; i++)
	{
		nAddress = m_nAddress + (i * 4);
		
		//Not thread safe...?
		if(m_pCtx->m_Breakpoint.DoesKeyExist(nAddress))
		{
			SetTextColor(hDC, RGB(0x00, 0x00, 0x00));

			hMem = CreateCompatibleDC(hDC);
			SelectObject(hMem, m_nBPointMask);
			BitBlt(hDC, 1, nY + 1, 15, 15, hMem, 0, 0, SRCAND);
			DeleteDC(hMem);

			hMem = CreateCompatibleDC(hDC);
			SelectObject(hMem, m_nBPoint);
			BitBlt(hDC, 1, nY + 1, 15, 15, hMem, 0, 0, SRCPAINT);
			DeleteDC(hMem);
		}

		if(CPS2VM::m_nStatus != PS2VM_STATUS_RUNNING)
		{
			if(nAddress == m_pCtx->m_State.nPC)
			{
				SetTextColor(hDC, RGB(0x00, 0x00, 0x00));

				hMem = CreateCompatibleDC(hDC);
				SelectObject(hMem, m_nArrowMask);
				BitBlt(hDC, 3, nY + 2, 13, 13, hMem, 0, 0, SRCAND);
				DeleteDC(hMem);

				hMem = CreateCompatibleDC(hDC);
				SelectObject(hMem, m_nArrow);
				BitBlt(hDC, 3, nY + 2, 13, 13, hMem, 0, 0, SRCPAINT);
				DeleteDC(hMem);
			}
		}

		if(nAddress == m_nSelected)
		{
			SetRect(&rsel, 18, nY, rwin.right, nY + s.cy + YSPACE);
			if(m_nFocus)
			{
				FillRect(hDC, &rsel, (HBRUSH)GetStockObject(BLACK_BRUSH));
			}
			else
			{
				FillRect(hDC, &rsel, (HBRUSH)GetStockObject(GRAY_BRUSH));
			}
			SetTextColor(hDC, RGB(0xFF, 0xFF, 0xFF));
		}
		else
		{
			SetTextColor(hDC, RGB(0x00, 0x00, 0x00));
		}

		xsnprintf(sTemp, countof(sTemp), _X("%0.8X"), nAddress);
		TextOut(hDC, 20, nY, sTemp, (int)xstrlen(sTemp));
		
		pSub = m_pCtx->m_pAnalysis->FindSubroutine(nAddress);
		if(pSub != NULL)
		{
			SelectObject(hDC, nLtGrayPen);
			if(nAddress == pSub->nStart)
			{
				MoveToEx(hDC, 90, nY + s.cy + YSPACE, NULL);
				LineTo(hDC, 90, nY + ((s.cy + YSPACE) / 2) - 1);
				LineTo(hDC, 95, nY + ((s.cy + YSPACE) / 2));
			}
			else if(nAddress == pSub->nEnd)
			{
				MoveToEx(hDC, 90, nY, NULL);
				LineTo(hDC, 90, nY + ((s.cy + YSPACE) / 2));
				LineTo(hDC, 95, nY + ((s.cy + YSPACE) / 2));
			}
			else
			{
				MoveToEx(hDC, 90, nY, NULL);
				LineTo(hDC, 90, nY + s.cy + YSPACE);
			}
		}

		nData = GetInstruction(nAddress);
		xsnprintf(sTemp, countof(sTemp), _X("%0.8X"), nData);
		TextOut(hDC, 100, nY, sTemp, (int)xstrlen(sTemp));
		
		m_pCtx->m_pArch->GetInstructionMnemonic(m_pCtx, nAddress, nData, sDisAsm, 256);
		xconvert(sTemp, sDisAsm, 256);
		TextOut(hDC, 200, nY, sTemp, (int)xstrlen(sTemp));

		m_pCtx->m_pArch->GetInstructionOperands(m_pCtx, nAddress, nData, sDisAsm, 256);
		xconvert(sTemp, sDisAsm, 256);
		TextOut(hDC, 300, nY, sTemp, (int)xstrlen(sTemp));

		nCommentDrawn = false;

		sTag = m_pCtx->m_Functions.Find(nAddress);
		if(sTag != NULL)
		{
			SetTextColor(hDC, RGB(0x00, 0x00, 0x80));
			strcpy(sDisAsm, "@");
			strcat(sDisAsm, sTag);
			xconvert(sTemp, sDisAsm, 256);
			TextOut(hDC, 450, nY, sTemp, (int)xstrlen(sTemp));
			nCommentDrawn = true;
		}

		if(!nCommentDrawn)
		{
			if(m_pCtx->m_pArch->IsInstructionBranch(m_pCtx, nAddress, nData))
			{
				nEffAddr = m_pCtx->m_pArch->GetInstructionEffectiveAddress(m_pCtx, nAddress, nData);
				sTag = m_pCtx->m_Functions.Find(nEffAddr);
				if(sTag != NULL)
				{
					SetTextColor(hDC, RGB(0x00, 0x00, 0x80));
					strcpy(sDisAsm, "-> ");
					strcat(sDisAsm, sTag);
					xconvert(sTemp, sDisAsm, 256);
					TextOut(hDC, 450, nY, sTemp, (int)xstrlen(sTemp));
					nCommentDrawn = true;
				}
			}
		}

		sTag = m_pCtx->m_Comments.Find(nAddress);
		if(sTag != NULL && !nCommentDrawn)
		{
			SetTextColor(hDC, RGB(0x00, 0x80, 0x00));
			strcpy(sDisAsm, ";");
			strcat(sDisAsm, sTag);
			xconvert(sTemp, sDisAsm, 256);
			TextOut(hDC, 450, nY, sTemp, (int)xstrlen(sTemp));
		}

		nY += s.cy + YSPACE;
	}

	DeleteObject(nLtGrayPen);
	DeleteObject(nFont);
}
