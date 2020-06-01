
#include "pch.h"
#include "utils.h"

char* cstr2str(CString cstr) {
	const size_t newsizew = (cstr.GetLength() + 1) * 2;
	char* nstringw = new char[newsizew];
	size_t convertedCharsw = 0;
	wcstombs_s(&convertedCharsw, nstringw, newsizew, cstr, _TRUNCATE);

	return nstringw;
}

wchar_t* cstr2strW(CString cstr) {
	const size_t newsizew = (cstr.GetLength() + 1) * 2;
	wchar_t* n2stringw = new wchar_t[newsizew];
	wcscpy_s(n2stringw, newsizew, cstr);

	return n2stringw;
}

void SetDropDownHeight(CComboBox* pMyComboBox, int itemsToShow) {
	// Get rectangles    
	CRect rctComboBox, rctDropDown;
	pMyComboBox->GetClientRect(&rctComboBox); // Combo rect    
	pMyComboBox->GetDroppedControlRect(&rctDropDown); // DropDownList rect   
	int itemHeight = pMyComboBox->GetItemHeight(-1); // Get Item height   
	pMyComboBox->GetParent()->ScreenToClient(&rctDropDown); // Converts coordinates    
	rctDropDown.bottom = rctDropDown.top + rctComboBox.Height() + itemHeight * itemsToShow; // Set height   
	pMyComboBox->MoveWindow(&rctDropDown); // enable changes  
}

int enumUsbDisk(vector<Device> &device_list, int cnt)
{
	int usb_disk_cnt = 0;

	char disk_path[5] = { 0 };
	DWORD all_disk = GetLogicalDrives();

	int i = 0;
	DWORD bytes_returned = 0;
	while (all_disk && usb_disk_cnt < cnt)
	{
		if ((all_disk & 0x1) == 1)
		{
			sprintf_s(disk_path, "%c:", 'A' + i);


			if (GetDriveTypeA(disk_path) == DRIVE_REMOVABLE)
			{
				// get device capacity
				Device tmp_device('A' + i);
				HANDLE hDevice = tmp_device.openDevice();
				if (hDevice == INVALID_HANDLE_VALUE) {
					TRACE("\n[Warn] Open %s failed.\n", disk_path);
				}
				else {
					// skip invalid device (include card reader)
					if (tmp_device.isValid()) {
						device_list.push_back(tmp_device);
						usb_disk_cnt++;
					}

					CloseHandle(hDevice);
				}
			}
		}
		all_disk = all_disk >> 1;
		i++;
	}

	return usb_disk_cnt;
}

BOOL dirExists(CString path) {
	DWORD dwAttrib = GetFileAttributes(path);

	return dwAttrib != INVALID_FILE_ATTRIBUTES;
}

DWORD countBits(DWORD n)
{
	if (n == 0)
		return 0;
	else
		return (n & 1) + countBits(n >> 1);
}
