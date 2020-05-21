
// StorTestToolDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "StorTestTool.h"
#include "StorTestToolDlg.h"
#include "afxdialogex.h"

#include "utils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CStorTestToolDlg dialog



CStorTestToolDlg::CStorTestToolDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_STORTESTTOOL_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CStorTestToolDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_Device, device_ctrl);
	DDX_Control(pDX, IDC_Function, function_ctrl);
	DDX_Control(pDX, IDC_LBA_end, LBA_end_ctrl);
	DDX_Control(pDX, IDC_LBA_start, LBA_start_ctrl);
	DDX_Control(pDX, IDC_Loop_num, loop_num_ctrl);
	DDX_Control(pDX, IDC_Sector_max, sector_max_ctrl);
	DDX_Control(pDX, IDC_Sector_min, sector_min_ctrl);
}

BEGIN_MESSAGE_MAP(CStorTestToolDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
//	ON_CBN_SELCHANGE(IDC_Device, &CStorTestToolDlg::OnCbnSelchangeDevice)
	ON_CBN_DROPDOWN(IDC_Device, &CStorTestToolDlg::OnCbnDropdownDevice)
	ON_BN_CLICKED(ID_RUN, &CStorTestToolDlg::OnBnClickedRun)
	ON_CBN_SELCHANGE(IDC_Function, &CStorTestToolDlg::OnCbnSelchangeFunction)
END_MESSAGE_MAP()


// CStorTestToolDlg message handlers

BOOL CStorTestToolDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	// initial function dropdown list
	int function_cnt = 8;
	for (int i = 0; i < function_cnt; i++) {
		function_ctrl.InsertString(i, function_map[i]);
	}
	SetDropDownHeight(&function_ctrl, function_cnt);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CStorTestToolDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CStorTestToolDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CStorTestToolDlg::OnCbnDropdownDevice()
{
	// empty options
	device_ctrl.ResetContent();

	// reset device_list
	for (vector<Device>::iterator iter = device_list.begin(); iter != device_list.end(); ) {
		iter = device_list.erase(iter);
	}
	vector<Device>().swap(device_list);

	// set device combo box
	int usb_cnt = enumUsbDisk(device_list, 8);
	if (usb_cnt == -1) {
		MessageBox(_T("Enumerate usb disk failed."), _T("Error"), MB_ICONERROR);
	}
	else {
		for (int i = 0; i < usb_cnt; i++) {
			Device cur_device = device_list.at(i);
			device_ctrl.InsertString(i, cur_device.showText());
		}
	}
	SetDropDownHeight(&device_ctrl, usb_cnt);
}


void CStorTestToolDlg::OnBnClickedRun()
{
	DWORD LBA_start, LBA_end, wr_sector_min, wr_sector_max, loop_num;
	DWORD device_idx, function_idx;
	Device selected_device;

	// get selected device
	device_idx = device_ctrl.GetCurSel();
	if (device_idx == CB_ERR) {
		MessageBox(_T("Must select a device."), _T("Error"), MB_ICONERROR);
		return;
	}
	selected_device = device_list.at(device_idx);

	// get selected function
	function_idx = function_ctrl.GetCurSel();
	if (function_idx == CB_ERR) {
		MessageBox(_T("Must select function."), _T("Error"), MB_ICONERROR);
		return;
	}

	// initial variable
	CString tmp;
	LBA_start_ctrl.GetWindowText(tmp);
	if (tmp.IsEmpty()) {
		MessageBox(_T("Must set LBA range."), _T("Error"), MB_ICONERROR);
		return;
	}
	LBA_start = _ttoi(tmp);
	LBA_end_ctrl.GetWindowText(tmp);
	if (tmp.IsEmpty()) {
		MessageBox(_T("Must set LBA range."), _T("Error"), MB_ICONERROR);
		return;
	}
	LBA_end = _ttoi(tmp);
	sector_min_ctrl.GetWindowText(tmp);
	if (tmp.IsEmpty()) {
		MessageBox(_T("Must set Write/Read sector range."), _T("Error"), MB_ICONERROR);
		return;
	}
	wr_sector_min = _ttoi(tmp);
	sector_max_ctrl.GetWindowText(tmp);
	if (tmp.IsEmpty()) {
		MessageBox(_T("Must set Write/Read sector range."), _T("Error"), MB_ICONERROR);
		return;
	}
	wr_sector_max = _ttoi(tmp);
	loop_num_ctrl.GetWindowText(tmp);
	if (function_idx != 5 && tmp.IsEmpty()) {
		MessageBox(_T("Must set loop number."), _T("Error"), MB_ICONERROR);
		return;
	}
	loop_num = _ttoi(tmp);

	TRACE(_T("\n[Msg] Device selected: %c:\n"
				"[Msg] Function selected: %s\n"
				"[Msg] Setup: {LBA range: %u ~ %u; write/read sector range: %u ~ %u, loop number: %u}\n"),
				selected_device.getIdent(), 
				function_map[function_idx], 
				LBA_start, LBA_end, wr_sector_min, wr_sector_max, loop_num);

	// check value
	if (LBA_start > LBA_end) {
		MessageBox(_T("The start LBA cannot larger than the end LBA."), _T("Error"), MB_ICONERROR);
		return;
	}
	if (wr_sector_min > wr_sector_max) {
		MessageBox(_T("The minimum of Write/Read sector cannot larger than the Maximum."), _T("Error"), MB_ICONERROR);
		return;
	}
	if (LBA_end >= selected_device.getCapacitySec()) {
		tmp.Format(_T("Total number of sectors: %u"), selected_device.getCapacitySec());
		MessageBox(tmp, _T("Error"), MB_ICONERROR);
		return;
	}
	if (wr_sector_max > selected_device.getMaxTransfSec()) {
		tmp.Format(_T("Maximum transfer number of sectors: %u"), selected_device.getMaxTransfSec());
		MessageBox(tmp, _T("Error"), MB_ICONERROR);
		return;
	}

}


void CStorTestToolDlg::OnCbnSelchangeFunction()
{
	DWORD function_idx;
	// get selected function
	function_idx = function_ctrl.GetCurSel();
	if (function_idx == 5) {
		loop_num_ctrl.SetWindowText(_T(""));
		loop_num_ctrl.EnableWindow(FALSE);
	}
	else {
		loop_num_ctrl.EnableWindow(TRUE);
	}
}
