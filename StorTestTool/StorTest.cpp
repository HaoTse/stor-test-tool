#include "pch.h"
#include "StorTest.h"

#include <string>
#include <sstream>
#include <iostream>
#include <random>

#include "SCSI_IO.h"
#include "utils.h"

StorTest::StorTest(Device dev, DWORD fun_idx, DWORD start, DWORD end, DWORD smin, DWORD smax, WORD loopn) :
	selected_device(dev), function_idx(fun_idx), LBA_start(start), LBA_end(end),
	wr_sector_min(smin), wr_sector_max(smax), loop_num(loopn)
{
	QueryPerformanceFrequency(&nFreq);
}

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
	CString title, diff_table, tmp, msg = _T("");
	CString if_error_str;
	CString expect_str, read_str;
	DWORD error_LBA_cnt = 0, error_bit_cnt = 0, error_byte_cnt = 0;

	BYTE expect_data[PHYSICAL_SECTOR_SIZE];
	for (DWORD cur_LBA = 0; cur_LBA < cmd_length; cur_LBA++) {
		title.Format(_T("LBA %u"), start_LBA + cur_LBA);
		diff_table = CString(_T("\tLBA Pattern")) + CString(_T("\t\t\t\t\t\t\t\t\t\t\tRead Data\n"));
		get_LBA_pattern(expect_data, start_LBA + cur_LBA, loop);

		if_error_str = _T("\n");
		for (DWORD i = 0; i < (PHYSICAL_SECTOR_SIZE >> 4); i++) {
			expect_str = CString(_T("\t"));
			read_str = CString(_T(" | "));

			for (DWORD j = 0; j < 16; j++) {
				DWORD cur_idx = (i << 4) + j;
				BYTE tmp_expect = *(expect_data + cur_idx);
				BYTE tmp_read = *(read_buf + cur_LBA * PHYSICAL_SECTOR_SIZE + cur_idx);
				BYTE diff_byte = tmp_expect ^ tmp_read;
				if (diff_byte != 0) {
					tmp.Format(_T(">%02X"), tmp_expect);
					expect_str += tmp;
					tmp.Format(_T(">%02X"), tmp_read);
					read_str += tmp;
					if_error_str = _T(" (Error)\n");

					// count error byte and error bit
					error_byte_cnt++;
					error_bit_cnt += countBits(diff_byte);
				}
				else {
					tmp.Format(_T(" %02X"), tmp_expect);
					expect_str += tmp;
					tmp.Format(_T(" %02X"), tmp_read);
					read_str += tmp;
				}
			}

			diff_table += expect_str + read_str + CString(_T("\n"));
		}
		if (if_error_str != _T("\n")) error_LBA_cnt++;

		msg += title + if_error_str + diff_table;
	}
	tmp.Format(_T("Error LBA count: %u, Error Byte count: %u, Error bits count: %u\n"), error_LBA_cnt, error_byte_cnt, error_bit_cnt);
	msg = tmp + msg;

	set_error_msg(msg);
}

HANDLE StorTest::get_file_handle(CString file_path)
{
	wchar_t* path_tmp = cstr2strW(file_path);
	HANDLE file_handle = CreateFile(
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
	WriteFile(file_handle, Header, 2, &bytesWritten, NULL);

	return file_handle;
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

	CString msg;
	DWORD cur_LBA;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// initial random
		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<int> distribution(wr_sector_min, wr_sector_max);

		// W/R
		msg.Format(_T("\tStart loop %5u write/read\n"), cur_loop);
		set_log_msg(msg);
		cur_LBA = LBA_start;
		cur_LBA_cnt = 0;
		while (cur_LBA < LBA_end)
		{
			if (if_pause) continue;
			if (if_terminate) break;

			DWORD wr_sec_num = distribution(generator);
			// check remain LBA
			wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA)) ? wr_sec_num : (LBA_end - cur_LBA);
			DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* wr_data = new BYTE[wr_sec_len];

			// get LBA pattern
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, cur_loop);
			}

			// wrtie LBA
			QueryPerformanceCounter(&nBeginTime); // timer begin
			ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, TRUE)) {
				TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				CloseHandle(hDevice);
				throw std::runtime_error("Write LBA failed.");
			}
			QueryPerformanceCounter(&nEndTime); // timer end
			cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
			msg.Format(_T("Loop %5u Write LBA: %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
						cur_loop, cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num, cmd_time);
			set_cmd_msg(msg);

			// read LBA
			QueryPerformanceCounter(&nBeginTime); // timer begin
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
				TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				CloseHandle(hDevice);
				throw std::runtime_error("Read LBA failed.");
			}
			QueryPerformanceCounter(&nEndTime); // timer end
			cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
			msg.Format(_T("Loop %5u Read LBA:  %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
						cur_loop, cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num, cmd_time);
			set_cmd_msg(msg);

			// compare pattern
			BYTE expect_data[PHYSICAL_SECTOR_SIZE];
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(expect_data, cur_LBA + i, cur_loop);

				if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
					msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
					set_log_msg(msg);
					msg.Format(_T("Sequential W/R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, cur_LBA, wr_sec_num);
					set_error_msg(msg);
					diff_cmd(cur_loop, cur_LBA, wr_sec_num, wr_data);

					// abort stortest
					set_terminate();
					delete[] wr_data;
					write_log_file(); // write cmd and error log, ehance the msg buffer is empty
					CloseHandle(cmd_file_hand);
					CloseHandle(hDevice);
					throw std::runtime_error("Find an error pattern. Show in error log.");
				}
			}

			cur_LBA += wr_sec_num;
			cur_LBA_cnt += wr_sec_num;
			
			delete[] wr_data;
		}
		if (if_terminate) break;

		// R
		msg.Format(_T("\tStart loop %5u read\n"), cur_loop);
		set_log_msg(msg);
		cur_LBA = LBA_start;
		cur_LBA_cnt = 0;
		while (cur_LBA < LBA_end)
		{
			if (if_pause) continue;
			if (if_terminate) break;

			DWORD wr_sec_num = selected_device.getMaxTransfSec();
			// check remain LBA
			wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA)) ? wr_sec_num : (LBA_end - cur_LBA);
			DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* wr_data = new BYTE[wr_sec_len];

			// get LBA pattern
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, cur_loop);
			}

			// read LBA
			QueryPerformanceCounter(&nBeginTime); // timer begin
			ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
				TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				CloseHandle(hDevice);
				throw std::runtime_error("Read LBA failed.");
			}
			QueryPerformanceCounter(&nEndTime); // timer end
			cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
			msg.Format(_T("Loop %5u Read LBA:  %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
						cur_loop, cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num, cmd_time);
			set_cmd_msg(msg);

			// compare pattern
			BYTE expect_data[PHYSICAL_SECTOR_SIZE];
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(expect_data, cur_LBA + i, cur_loop);

				// Make an error pattern
				//if (cur_LBA + i == 23 && cur_loop == 3) {
				//	*(wr_data + i * PHYSICAL_SECTOR_SIZE + 23) = 0xFF;
				//	*(wr_data + i * PHYSICAL_SECTOR_SIZE + 0) = 0x01;
				//}

				if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
					msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
					set_log_msg(msg);
					msg.Format(_T("Sequential R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, cur_LBA, wr_sec_num);
					set_error_msg(msg);
					diff_cmd(cur_loop, cur_LBA, wr_sec_num, wr_data);

					// abort stortest
					set_terminate();
					delete[] wr_data;
					write_log_file(); // write cmd and error log, ehance the msg buffer is empty
					CloseHandle(cmd_file_hand);
					CloseHandle(hDevice);
					throw std::runtime_error("Find an error pattern. Show in error log.");
				}
			}

			cur_LBA += wr_sec_num;
			cur_LBA_cnt += wr_sec_num;

			delete[] wr_data;
		}
		if (if_terminate) break;

		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
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

	// initial random
	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<int> distribution(wr_sector_min, wr_sector_max);

	CString msg;
	DWORD cur_LBA = LBA_start;
	// open command log file
	cmd_file_hand = get_file_handle(dir_path + CString(_T("\\command.txt")));
	while (cur_LBA < LBA_end)
	{
		if (if_pause) {
			continue;
		}
		if (if_terminate) {
			break;
		}

		DWORD wr_sec_num = distribution(generator);;
		// check remain LBA
		wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA)) ? wr_sec_num : (LBA_end - cur_LBA);
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// get LBA pattern
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, cur_LBA + i, 0);
		}

		// wrtie LBA
		QueryPerformanceCounter(&nBeginTime); // timer begin
		ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, TRUE)) {
			TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			write_log_file(); // write cmd and error log, ehance the msg buffer is empty
			CloseHandle(cmd_file_hand);
			CloseHandle(hDevice);
			throw std::runtime_error("Write LBA failed.");
		}
		QueryPerformanceCounter(&nEndTime); // timer end
		cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
		msg.Format(_T("Write LBA: %10u~%10u (size: %4u) Elapsed %8.3f ms\n"), cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num, cmd_time);
		set_cmd_msg(msg);

		cur_LBA += wr_sec_num;
		cur_LBA_cnt += wr_sec_num;
		
		delete[] wr_data;
	}
	cur_loop_cnt += 1;

	write_log_file(); // write cmd and error log, ehance the msg buffer is empty
	CloseHandle(cmd_file_hand);

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
	CString function_folder_map[8] = { _T("Seq(wr+r)"), _T("Seq(w+r)"),
								_T("Rev(wr+r)"), _T("Rev(w+r)"),
								_T("Testmode"), _T("Onewrite"), _T("Verify"), _T("Varyzone")};
	
	dir_path.Format(_T("D:\\stortest_log\\%s_LBA_%010u_%010u_wr_%04u_%04u_loop_%05u"),
					function_folder_map[function_idx], LBA_start, LBA_end, wr_sector_min, wr_sector_max, loop_num);

	if (dirExists(dir_path)) {
		TRACE(_T("\n[Error] Log directory exists.\n"));
		return FALSE;
	}
	CreateDirectory(dir_path, NULL);
	set_log_msg(CString(_T("Log directory: ")) + dir_path + CString(_T("\n")));

	// open error log file
	error_file_hand = get_file_handle(dir_path + CString(_T("\\error.txt")));
	
	return TRUE;
}

void StorTest::close_error_log_file()
{
	CloseHandle(error_file_hand);
}

void StorTest::write_log_file()
{
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
}

CString StorTest::get_log_msg()
{
	CString rtn;
	log_msg_mutex.lock();
	rtn = log_msg;
	log_msg.Empty();
	log_msg_mutex.unlock();

	// write cmd and error log
	write_log_file();

	return rtn;
}

UINT StorTest::get_cur_LBA_cnt()
{
	return cur_LBA_cnt;
}

UINT StorTest::get_cur_loop()
{
	return cur_loop_cnt;
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
