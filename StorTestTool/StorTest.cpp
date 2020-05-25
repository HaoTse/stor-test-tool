#include "pch.h"
#include "StorTest.h"

#include <string>
#include <sstream>
#include <iostream>

#include "SCSI_IO.h"

StorTest::StorTest(Device dev, DWORD fun_idx, DWORD start, DWORD end, DWORD smin, DWORD smax, WORD loopn) :
	selected_device(dev), function_idx(fun_idx), LBA_start(start), LBA_end(end),
	wr_sector_min(smin), wr_sector_max(smax), loop_num(loopn) {}

void StorTest::dec_in_hex(BYTE* hex_byte, DWORD num)
{
	ULONGLONG num_hex;
	std::stringstream ss;
	std::string s = std::to_string(num);

	ss << std::hex << s;
	ss >> num_hex;

	for (int i = 0; i < 8; i++) {
		hex_byte[7 - i] = (BYTE)(num_hex >> (i * 8));
	}
}

void StorTest::get_LBA_pattern(BYTE* LBA_pattern, DWORD buf_offset, DWORD LBA, WORD loop)
{
	DWORD LCA = LBA >> 3;
	BYTE LBA_Byte[8] = { 0 }, LCA_Byte[8] = { 0 };
	dec_in_hex(LBA_Byte, LBA);
	dec_in_hex(LCA_Byte, LCA);

	memcpy(LBA_pattern + buf_offset, LBA_Byte, 8);
	memcpy(LBA_pattern + buf_offset + 504, LBA_Byte, 8);
	memcpy(LBA_pattern + buf_offset + 8, LCA_Byte, 8);
	memcpy(LBA_pattern + buf_offset + 496, LCA_Byte, 8);

	LBA_pattern[buf_offset + 16] = (BYTE)(loop >> 8);
	LBA_pattern[buf_offset + 17] = (BYTE)loop;
	LBA_pattern[buf_offset + 18] = (LBA + LCA + loop) % 256;

	for (int i = 19; i <= 495; i++) {
		LBA_pattern[buf_offset + i] = LBA_pattern[buf_offset + i - 1] + 1;
	}
}

BOOL StorTest::fun_sequential_ac()
{
	return TRUE;
}

BOOL StorTest::fun_sequential_bc()
{
	return TRUE;
}

BOOL StorTest::fun_reverse_ac()
{
	return TRUE;
}

BOOL StorTest::fun_reverse_bc()
{
	return TRUE;
}

BOOL StorTest::fun_testmode()
{
	return TRUE;
}

BOOL StorTest::fun_onewrite()
{
	// device information
	HANDLE hDevice = selected_device.openDevice();
	DWORD max_transf_len = selected_device.getMaxTransfLen();
	if (hDevice == INVALID_HANDLE_VALUE) {
		TRACE(_T("\n[Error] Open device failed. Error Code = %u.\n"), GetLastError());
		CloseHandle(hDevice);
		throw std::runtime_error("Open device failed.");
	}

	srand(time(NULL));
	DWORD cur_LBA = LBA_start;
	while (cur_LBA <= LBA_end)
	{
		DWORD wr_sec_num = rand() % (wr_sector_max - wr_sector_min + 1) + wr_sector_min;
		// check remain LBA
		wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA + 1)) ? wr_sec_num : (LBA_end - cur_LBA + 1);
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// get LBA pattern
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(wr_data, i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, 0);
		}

		// wrtie LBA
		ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, TRUE)) {
			TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			CloseHandle(hDevice);
			throw std::runtime_error("Write LBA failed.");
		}

		cur_LBA += wr_sec_num;
		cur_LBA_cnt += wr_sec_num;
		
		delete[] wr_data;
	}
	cur_loop_cnt += 1;

	CloseHandle(hDevice);

	return TRUE;
}

BOOL StorTest::fun_verify()
{
	return TRUE;
}

BOOL StorTest::fun_varyzone()
{
	return TRUE;
}

BOOL StorTest::run()
{
	switch (function_idx)
	{
	case 0:
		return fun_sequential_ac();
	case 1:
		return fun_sequential_bc();
	case 2:
		return fun_reverse_ac();
	case 3:
		return fun_reverse_bc();
	case 4:
		return fun_testmode();
	case 5:
		return fun_onewrite();
	case 6:
		return fun_verify();
	case 7:
		return fun_varyzone();
	default:
		throw std::runtime_error("Function setup error.");
	}
}

UINT StorTest::get_cur_LBA_cnt()
{
	return cur_LBA_cnt;
}

UINT StorTest::get_cur_loop()
{
	return cur_loop_cnt;
}
