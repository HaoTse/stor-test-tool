
// StorTestToolDlg.h : header file
//

#pragma once

#include <vector>

#include "device.h"

using namespace std;

// CStorTestToolDlg dialog
class CStorTestToolDlg : public CDialogEx
{
// Construction
public:
	CStorTestToolDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_STORTESTTOOL_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
//	afx_msg void OnCbnSelchangeDevice();
	afx_msg void OnCbnDropdownDevice();
private:
	CString function_map[8] = { _T("Sequential (W/R+R)"), _T("Sequential (W+R)"),
								_T("Reverse (W/R+R)"), _T("Reverse (W+R)"),
								_T("Testmode "), _T("onewrite "), _T("Verify "), _T("Varyzone"), };
public:
	vector<Device> device_list;
	CComboBox device_ctrl;
	CComboBox function_ctrl;
};
