
// StorTestToolDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "StorTestTool.h"
#include "StorTestToolDlg.h"
#include "afxdialogex.h"

#include <future>
#include <thread>

#include "utils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CStorTestToolDlg dialog



CStorTestToolDlg::CStorTestToolDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_STORTESTTOOL_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	// initial stortest
	stortest = NULL;
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
	DDX_Control(pDX, IDC_Log_edit, Log_edit_ctrl);
	DDX_Control(pDX, IDC_PROGRESS_cur_loop, cur_loop_ctrl);
	DDX_Control(pDX, IDC_PROGRESS_tot_loop, tot_loop_ctrl);
	DDX_Control(pDX, IDC_cur_loop_edit_1, cur_loop_edit1_ctrl);
	DDX_Control(pDX, IDC_cur_loop_edit_2, cur_loop_edit2_ctrl);
	DDX_Control(pDX, IDC_tot_loop_edit_1, tot_loop_edit1_ctrl);
	DDX_Control(pDX, IDC_tot_loop_edit_2, tot_loop_edit2_ctrl);
	DDX_Control(pDX, ID_RUN, run_btn_ctrl);
	DDX_Control(pDX, IDC_pause_btn, pause_btn_ctrl);
	DDX_Control(pDX, IDC_stop_btn, stop_btn_ctrl);
	DDX_Control(pDX, IDCANCEL, cancel_btn_ctrl);
}

BEGIN_MESSAGE_MAP(CStorTestToolDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_CBN_DROPDOWN(IDC_Device, &CStorTestToolDlg::OnCbnDropdownDevice)
	ON_BN_CLICKED(ID_RUN, &CStorTestToolDlg::OnBnClickedRun)
	ON_CBN_SELCHANGE(IDC_Function, &CStorTestToolDlg::OnCbnSelchangeFunction)
	ON_BN_CLICKED(IDC_pause_btn, &CStorTestToolDlg::OnBnClickedpausebtn)
	ON_BN_CLICKED(IDC_stop_btn, &CStorTestToolDlg::OnBnClickedstopbtn)
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


void CStorTestToolDlg::OnBnClickedpausebtn()
{
	if (stortest->get_pause()) {
		stortest->set_pause(FALSE);
		pause_btn_ctrl.SetWindowText(_T("Pause"));
	}
	else {
		stortest->set_pause(TRUE);
		pause_btn_ctrl.SetWindowText(_T("Resume"));
	}
}


void CStorTestToolDlg::OnBnClickedstopbtn()
{
	stop_btn_ctrl.EnableWindow(FALSE);
	pause_btn_ctrl.EnableWindow(FALSE);
	stortest->set_terminate();
}


void CStorTestToolDlg::OnBnClickedRun()
{
	DWORD LBA_start, LBA_end, wr_sector_min, wr_sector_max;
	DWORD device_idx, function_idx;
	WORD loop_num;
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
	if (_ttoi(tmp) >= (1 << 16)) {
		MessageBox(_T("The size of loop number is only 2Bytes."), _T("Error"), MB_ICONERROR);
		return;
	}
	loop_num = _ttoi(tmp);

	TRACE(_T("\n[Msg] Device selected: %c:\n"
				"[Msg] Function selected: %s\n"
				"[Msg] Setup: {LBA range: %u~%u; write/read sector range: %u~%u, loop number: %u}\n"),
				selected_device.getIdent(), 
				function_map[function_idx], 
				LBA_start, LBA_end, wr_sector_min, wr_sector_max, loop_num);

	// check value
	if (LBA_start >= LBA_end) {
		MessageBox(_T("The start LBA must smaller than the end LBA."), _T("Error"), MB_ICONERROR);
		return;
	}
	if (wr_sector_min == 0 || wr_sector_max == 0) {
		MessageBox(_T("Write/Read sector number must be positive."), _T("Error"), MB_ICONERROR);
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
	if (wr_sector_min > (LBA_end - LBA_start)) {
		MessageBox(_T("The minimum of Write/Read sector is larger than the LBA range."), _T("Error"), MB_ICONERROR);
		return;
	}

	tot_LBA_num = LBA_end - LBA_start;
	tot_loop_num = (function_idx == 5) ? 1 : loop_num;
	stortest = new StorTest(selected_device, function_idx, LBA_start, LBA_end, wr_sector_min, wr_sector_max, loop_num);

	// check log directory
	if (!stortest->open_log_dir()) {
		MessageBox(_T("Log directory exists. Please delete it."), _T("Error"), MB_ICONERROR);
		delete stortest;
		return;
	}

	// progress updating thread
	CWinThread* progress_update_thread = AfxBeginThread(
		CStorTestToolDlg::update_progress_thread,
		(LPVOID) this,
		THREAD_PRIORITY_NORMAL,
		0,
		0,
		NULL);
}


UINT CStorTestToolDlg::update_progress_thread(LPVOID lpParam)
{
	CStorTestToolDlg* dlg = (CStorTestToolDlg*)lpParam;
	dlg->update_progress();
	return 0;
}


void CStorTestToolDlg::update_progress()
{
	DWORD tot_LBA_cnt, tot_loop_cnt, cur_LBA_cnt, cur_loop_cnt;
	DWORD progress_scale = 16; // use to scale the LBA cnt

	// disable dlg
	set_dlg_enable(FALSE);

	// stortest thread
	future<BOOL> stor_rtn = async(launch::async, &StorTest::run, stortest);

	// initial progress bar pos
	cur_loop_ctrl.SetPos(0);
	tot_loop_ctrl.SetPos(0);

	tot_LBA_cnt = tot_LBA_num;
	tot_loop_cnt = tot_loop_num;
	for (int i = 0; i <= 4; i++) {
		if ((tot_LBA_cnt >> (i * 4)) < (1 << 16)) {
			progress_scale = i * 4;
			break;
		}
	}
	// set progress bar static text
	CString str;
	str.Format(_T("%u"), tot_LBA_cnt);
	cur_loop_edit2_ctrl.SetWindowText(str);
	str.Format(_T("%u"), tot_loop_cnt);
	tot_loop_edit2_ctrl.SetWindowText(str);
	cur_loop_ctrl.SetRange(0, (short)(tot_LBA_cnt >> progress_scale));
	tot_loop_ctrl.SetRange(0, (short)(tot_loop_cnt));

	// set progress bar
	cur_loop_cnt = 0;
	do {
		// check stortest condition
		if (stortest->get_pause()) {
			continue;
		}
		if (stortest->get_terminate()) {
			break;
		}

		cur_LBA_cnt = stortest->get_cur_LBA_cnt();
		cur_loop_cnt = stortest->get_cur_loop();
		str.Format(_T("%u"), cur_LBA_cnt);
		cur_loop_edit1_ctrl.SetWindowText(str);
		cur_loop_ctrl.SetPos(cur_LBA_cnt >> progress_scale);
		str.Format(_T("%u"), cur_loop_cnt);
		tot_loop_edit1_ctrl.SetWindowText(str);
		tot_loop_ctrl.SetPos(cur_loop_cnt);

		// update log edit
		str = stortest->get_log_msg();
		Log_edit_ctrl.SetSel(-1, -1);
		Log_edit_ctrl.ReplaceSel(str);
		Log_edit_ctrl.PostMessage(WM_VSCROLL, SB_BOTTOM, 0); // scroll location
	} while (tot_loop_cnt == 0 || cur_loop_cnt < tot_loop_cnt);
	
	// update the remain log
	str = stortest->get_log_msg();
	Log_edit_ctrl.SetSel(-1, -1);
	Log_edit_ctrl.ReplaceSel(str);
	Log_edit_ctrl.PostMessage(WM_VSCROLL, SB_BOTTOM, 0); // scroll location

	// get stortest thread result
	try
	{
		if (stor_rtn.get()) {
			if(stortest->get_terminate())
				MessageBox(_T("Test terminated."), _T("Information"), MB_ICONINFORMATION);
			else
				MessageBox(_T("Test finished."), _T("Information"), MB_ICONINFORMATION);
		}
		else {
			MessageBox(_T("Test failed."), _T("Error"), MB_ICONERROR);
		}
	}
	catch (const std::exception& exp)
	{
		string msg = exp.what();
		MessageBox((LPCTSTR)CA2T(msg.c_str()), _T("Error"), MB_ICONERROR);
	}

	stortest->close_error_log_file();
	if(stortest)
		delete stortest;

	// enable dlg
	set_dlg_enable(TRUE);
	OnCbnSelchangeFunction(); // check if function is Onewrite
}


void CStorTestToolDlg::set_dlg_enable(bool setup)
{
	device_ctrl.EnableWindow(setup);
	LBA_start_ctrl.EnableWindow(setup);
	LBA_end_ctrl.EnableWindow(setup);
	sector_min_ctrl.EnableWindow(setup);
	sector_max_ctrl.EnableWindow(setup);
	loop_num_ctrl.EnableWindow(setup);
	function_ctrl.EnableWindow(setup);
	run_btn_ctrl.EnableWindow(setup);
	cancel_btn_ctrl.EnableWindow(setup);

	stop_btn_ctrl.EnableWindow(!setup);
	pause_btn_ctrl.EnableWindow(!setup);
}