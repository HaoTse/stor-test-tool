
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
}

BEGIN_MESSAGE_MAP(CStorTestToolDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
//	ON_CBN_SELCHANGE(IDC_Device, &CStorTestToolDlg::OnCbnSelchangeDevice)
	ON_CBN_DROPDOWN(IDC_Device, &CStorTestToolDlg::OnCbnDropdownDevice)
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
