
// StorTestToolDlg.h : header file
//

#pragma once

#include <vector>

#include "device.h"
#include "StorTest.h"

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
	afx_msg void OnBnClickedRun();
	afx_msg void OnCbnDropdownDevice();
	afx_msg void OnCbnSelchangeFunction();
	afx_msg void OnBnClickedpausebtn();
	afx_msg void OnBnClickedstopbtn();
private:
	StorTest* stortest;
	DWORD tot_LBA_num{ 0 }, tot_loop_num{ 0 };
	CString function_map[8] = { _T("Sequential (W/R+R)"), _T("Sequential (W+R)"),
								_T("Reverse (W/R+R)"), _T("Reverse (W+R)"),
								_T("Testmode "), _T("Onewrite "), _T("Verify "), _T("Varyzone"), };
	
	static UINT update_progress_thread(LPVOID lpParam);
	void update_progress();
	void set_dlg_enable(bool setup);
public:
	vector<Device> device_list;
	CComboBox device_ctrl;
	CComboBox function_ctrl;
	CEdit LBA_end_ctrl;
	CEdit LBA_start_ctrl;
	CEdit loop_num_ctrl;
	CEdit sector_max_ctrl;
	CEdit sector_min_ctrl;
	CRichEditCtrl Log_edit_ctrl;
	CProgressCtrl cur_loop_ctrl;
	CProgressCtrl tot_loop_ctrl;
	CStatic cur_loop_edit1_ctrl;
	CStatic cur_loop_edit2_ctrl;
	CStatic tot_loop_edit1_ctrl;
	CStatic tot_loop_edit2_ctrl;
	CButton run_btn_ctrl;
	CButton pause_btn_ctrl;
	CButton stop_btn_ctrl;
};
