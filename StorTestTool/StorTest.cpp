#include "pch.h"
#include "StorTest.h"

#include <string>
#include <sstream>
#include <iostream>

#include "SCSI_IO.h"
#include "utils.h"

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

void StorTest::get_LBA_pattern(BYTE* LBA_pattern, DWORD LBA, WORD loop)
{
	DWORD LCA = LBA >> 3;
	BYTE LBA_Byte[8] = { 0 }, LCA_Byte[8] = { 0 };
	dec_in_hex(LBA_Byte, LBA);
	dec_in_hex(LCA_Byte, LCA);

	memcpy(LBA_pattern, LBA_Byte, 8);
	memcpy(LBA_pattern + 504, LBA_Byte, 8);
	memcpy(LBA_pattern + 8, LCA_Byte, 8);
	memcpy(LBA_pattern + 496, LCA_Byte, 8);

	*(LBA_pattern + 16) = (BYTE)(loop >> 8);
	*(LBA_pattern + 17) = (BYTE)loop;
	*(LBA_pattern + 18) = (LBA + LCA + loop) % 256;

	for (int i = 19; i <= 495; i++) {
		*(LBA_pattern + i) = *(LBA_pattern + i - 1) + 1;
	}
}

void StorTest::set_log_msg(CString msg)
{
	log_msg_mutex.lock();
	log_msg += msg;
	log_msg_mutex.unlock();
}

void StorTest::set_cmd_msg(CString msg)
{
	cmd_msg_mutex.lock();
	cmd_msg += msg;
	cmd_msg_mutex.unlock();
}

void StorTest::set_error_msg(CString msg)
{
	error_msg_mutex.lock();
	error_msg += msg;
	error_msg_mutex.unlock();
}

BOOL StorTest::compare_sector(BYTE* expect_buf, BYTE* read_buf)
{
	if (memcmp(expect_buf, read_buf, PHYSICAL_SECTOR_SIZE) == 0) {
		return TRUE;
	}

	return FALSE;
}

void StorTest::diff_cmd(WORD loop, DWORD start_LBA, DWORD cmd_length, BYTE* read_buf)
{
	CString msg, tmp;
	CString expect_str = _T(""), read_str = _T("");
	msg.Format(_T("Loop: %u\n"), loop);

	BYTE expect_data[PHYSICAL_SECTOR_SIZE];
	for (DWORD l = 0; l < cmd_length; l++) {
		tmp.Format(_T("LBA: %u\n"), start_LBA + l);
		msg += tmp;
		get_LBA_pattern(expect_data, start_LBA + l, loop);

		expect_str = _T("");
		read_str = _T("");
		for (DWORD i = 0; i < (PHYSICAL_SECTOR_SIZE >> 4); i++) {
			expect_str += CString(_T("\t"));
			read_str += CString(_T("\t"));

			for (DWORD j = 0; j < 16; j++) {
				DWORD cur_idx = (i << 4) + j;
				if (*(expect_data + cur_idx) != *(read_buf + l * PHYSICAL_SECTOR_SIZE + cur_idx)) {
					tmp.Format(_T("<%3u>"), *(expect_data + cur_idx));
					expect_str += tmp;
					tmp.Format(_T("<%3u>"), *(read_buf + l * PHYSICAL_SECTOR_SIZE + cur_idx));
					read_str += tmp;
				}
				else {
					tmp.Format(_T("%5u"), *(expect_data + cur_idx));
					expect_str += tmp;
					tmp.Format(_T("%5u"), *(read_buf + l * PHYSICAL_SECTOR_SIZE + cur_idx));
					read_str += tmp;
				}
			}

			expect_str += CString(_T("\n"));
			read_str += CString(_T("\n"));
		}

		msg += CString(_T("LBA Pattern:\n")) + expect_str + CString(_T("Read Data:\n")) + read_str;
	}

	set_error_msg(msg);
}

BOOL StorTest::fun_sequential_ac()
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
	CString msg;
	DWORD cur_LBA;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		msg.Format(_T("Loop %u\n"), cur_loop);
		set_cmd_msg(msg);

		// W/R
		msg.Format(_T("\tStart loop %u write\n"), cur_loop);
		set_log_msg(msg);
		cur_LBA = LBA_start;
		cur_LBA_cnt = 0;
		while (cur_LBA <= LBA_end)
		{
			if (if_pause) continue;
			if (if_terminate) break;

			DWORD wr_sec_num = rand() % (wr_sector_max - wr_sector_min + 1) + wr_sector_min;
			// check remain LBA
			wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA + 1)) ? wr_sec_num : (LBA_end - cur_LBA + 1);
			DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* wr_data = new BYTE[wr_sec_len];

			// get LBA pattern
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, cur_loop);
			}

			// wrtie LBA
			ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, TRUE)) {
				TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				CloseHandle(hDevice);
				throw std::runtime_error("Write LBA failed.");
			}
			msg.Format(_T("Write LBA: %u ~ %u (size = %u)\n"), cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num);
			set_cmd_msg(msg);

			// read LBA
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
				TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				CloseHandle(hDevice);
				throw std::runtime_error("Read LBA failed.");
			}
			msg.Format(_T("Read LBA: %u ~ %u (size = %u)\n"), cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num);
			set_cmd_msg(msg);

			// compare pattern
			BYTE expect_data[PHYSICAL_SECTOR_SIZE];
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(expect_data, cur_LBA + i, cur_loop);

				if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
					msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
					set_log_msg(msg);
					msg.Format(_T("Sequential W/R LBA: %u, Size: %u\n"), cur_LBA, wr_sec_num);
					set_error_msg(msg);
					diff_cmd(cur_loop, cur_LBA, wr_sec_num, wr_data);

					// abort stortest
					set_terminate();
					delete[] wr_data;
					throw std::runtime_error("Find an error pattern. Show in error log.");
				}
			}

			cur_LBA += wr_sec_num;
			cur_LBA_cnt += wr_sec_num;
			
			delete[] wr_data;
		}
		if (if_terminate) break;

		// R
		msg.Format(_T("\tStart loop %u read\n"), cur_loop);
		set_log_msg(msg);
		cur_LBA = LBA_start;
		cur_LBA_cnt = 0;
		while (cur_LBA <= LBA_end)
		{
			if (if_pause) continue;
			if (if_terminate) break;

			DWORD wr_sec_num = selected_device.getMaxTransfSec();
			// check remain LBA
			wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA + 1)) ? wr_sec_num : (LBA_end - cur_LBA + 1);
			DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* wr_data = new BYTE[wr_sec_len];

			// get LBA pattern
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, cur_loop);
			}

			// read LBA
			ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
				TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				CloseHandle(hDevice);
				throw std::runtime_error("Read LBA failed.");
			}
			msg.Format(_T("Read LBA: %u ~ %u (size = %u)\n"), cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num);
			set_cmd_msg(msg);

			// compare pattern
			BYTE expect_data[PHYSICAL_SECTOR_SIZE];
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(expect_data, cur_LBA + i, cur_loop);

				// Make an error pattern
				//if (cur_LBA + i == 23 && cur_loop == 3)
				//	*(wr_data + i * PHYSICAL_SECTOR_SIZE + 23) = 0xFF;

				if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
					msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
					set_log_msg(msg);
					msg.Format(_T("Sequential R LBA: %u, Size: %u\n"), cur_LBA, wr_sec_num);
					set_error_msg(msg);
					diff_cmd(cur_loop, cur_LBA, wr_sec_num, wr_data);

					// abort stortest
					set_terminate();
					delete[] wr_data;
					throw std::runtime_error("Find an error pattern. Show in error log.");
				}
			}

			cur_LBA += wr_sec_num;
			cur_LBA_cnt += wr_sec_num;

			delete[] wr_data;
		}
		if (if_terminate) break;

		cur_loop_cnt++;
	}

	CloseHandle(hDevice);

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
	CString msg;
	DWORD cur_LBA = LBA_start;
	while (cur_LBA <= LBA_end)
	{
		if (if_pause) {
			continue;
		}
		if (if_terminate) {
			break;
		}

		DWORD wr_sec_num = rand() % (wr_sector_max - wr_sector_min + 1) + wr_sector_min;
		// check remain LBA
		wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA + 1)) ? wr_sec_num : (LBA_end - cur_LBA + 1);
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// get LBA pattern
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, 0);
		}

		// wrtie LBA
		ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, TRUE)) {
			TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			CloseHandle(hDevice);
			throw std::runtime_error("Write LBA failed.");
		}

		msg.Format(_T("Write LBA: %u ~ %u (size = %u)\n"), cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num);
		set_cmd_msg(msg);

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
		set_log_msg(CString(_T("Start Sequential mode a+c\n")));
		return fun_sequential_ac();
	case 1:
		set_log_msg(CString(_T("Start Sequential mode b+c\n")));
		return fun_sequential_bc();
	case 2:
		set_log_msg(CString(_T("Start Reverse mode a+c\n")));
		return fun_reverse_ac();
	case 3:
		set_log_msg(CString(_T("Start Reverse mode b+c\n")));
		return fun_reverse_bc();
	case 4:
		set_log_msg(CString(_T("Start TestMode\n")));
		return fun_testmode();
	case 5:
		set_log_msg(CString(_T("Start OneWrite\n")));
		return fun_onewrite();
	case 6:
		set_log_msg(CString(_T("Start Verify\n")));
		return fun_verify();
	case 7:
		set_log_msg(CString(_T("Start Varyzone\n")));
		return fun_varyzone();
	default:
		throw std::runtime_error("Function setup error.");
	}
}

BOOL StorTest::open_log_dir()
{
	CString dir_path;
	
	dir_path.Format(_T("D:\\stortest_log\\fun_%u_LBA_%u_%u_wr_%u_%u_loop_%u"), function_idx, LBA_start, LBA_end, wr_sector_min, wr_sector_max, loop_num);

	if (dirExists(dir_path)) {
		TRACE(_T("\n[Error] Log directory exists.\n"));
		return FALSE;
	}
	CreateDirectory(dir_path, NULL);
	set_log_msg(CString(_T("Log directory: ")) + dir_path + CString(_T("\n")));

	// open command log file and error log file
	wchar_t* path_tmp = cstr2strW(dir_path + CString(_T("\\command.txt")));
	cmd_file_hand = CreateFile(
		path_tmp,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	delete[] path_tmp;

	path_tmp = cstr2strW(dir_path + CString(_T("\\error.txt")));
	error_file_hand = CreateFile(
		path_tmp,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	delete[] path_tmp;

	unsigned char Header[2]; // unicode text file header
	DWORD bytesWritten;
	Header[0] = 0xFF;
	Header[1] = 0xFE;
	WriteFile(cmd_file_hand, Header, 2, &bytesWritten, NULL);
	WriteFile(error_file_hand, Header, 2, &bytesWritten, NULL);
	
	return TRUE;
}

void StorTest::close_log_files()
{
	CloseHandle(cmd_file_hand);
	CloseHandle(error_file_hand);
}

UINT StorTest::get_cur_LBA_cnt()
{
	return cur_LBA_cnt;
}

UINT StorTest::get_cur_loop()
{
	return cur_loop_cnt;
}

CString StorTest::get_log_msg()
{
	CString rtn;
	log_msg_mutex.lock();
	rtn = log_msg;
	log_msg.Empty();
	log_msg_mutex.unlock();

	// write log
	DWORD bytesWritten;
	if (!cmd_msg.IsEmpty()) {
		cmd_msg_mutex.lock();
		WriteFile(
			cmd_file_hand,
			cmd_msg,
			cmd_msg.GetLength() * 2,
			&bytesWritten,
			nullptr);
		cmd_msg.Empty();
		cmd_msg_mutex.unlock();
	}
	if (!error_msg.IsEmpty()) {
		error_msg_mutex.lock();
		WriteFile(
			error_file_hand,
			error_msg,
			error_msg.GetLength() * 2,
			&bytesWritten,
			nullptr);
		error_msg.Empty();
		error_msg_mutex.unlock();
	}

	return rtn;
}

void StorTest::set_terminate()
{
	if_terminate = TRUE;
	if_pause = FALSE;
}

BOOL StorTest::get_terminate()
{
	return if_terminate;
}

void StorTest::set_pause(bool setup)
{
	if_pause = setup;
}

BOOL StorTest::get_pause()
{
	return if_pause;
}
