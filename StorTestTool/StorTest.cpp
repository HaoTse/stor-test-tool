#include "pch.h"
#include "StorTest.h"

#include <string>
#include <sstream>
#include <iostream>

#include "SCSI_IO.h"
#include "utils.h"


StorTest::StorTest(Device dev, DWORD fun_idx, DWORD start, DWORD end, DWORD smin, DWORD smax, WORD loopn,
	DWORD varyzone_tlen, DWORD varyzone_vall) :
	selected_device(dev), function_idx(fun_idx), LBA_start(start), LBA_end(end),
	wr_sector_min(smin), wr_sector_max(smax), loop_num(loopn),
	test_len_pro_loop{ varyzone_tlen }, test_loop_per_verify_all{ varyzone_vall }
{
	QueryPerformanceFrequency(&nFreq);
}

void StorTest::close_hDevice()
{
	DWORD bytes_returned;
	CString msg;
	// unlock volume
	if (!DeviceIoControl(hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
		TRACE(_T("\n[Error] FSCTL_UNLOCK_VOLUME failed. Error code = %u.\n"), GetLastError());
		msg.Format(_T("FSCTL_UNLOCK_VOLUME failed. Error code = %u."), GetLastError());
		throw msg;
	}

	CloseHandle(hDevice);
}

void StorTest::dec_in_hex(BYTE* hex_byte, DWORD num)
{
	ULONGLONG num_hex = 0, hex_cnt = 1;
	DWORD tmp_num = num;

	while (tmp_num > 0) {
		num_hex += (ULONGLONG)(tmp_num % 10) * hex_cnt;
		tmp_num /= 10;
		hex_cnt = hex_cnt << 4;
	}

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

void StorTest::diff_cmd(WORD* loop_map, DWORD start_LBA, DWORD cmd_length, BYTE* read_buf)
{
	CString title, diff_table, tmp, msg = _T("");
	CString if_error_str;
	CString expect_str, read_str;
	DWORD error_LBA_cnt = 0, error_bit_cnt = 0, error_byte_cnt = 0;

	BYTE expect_data[PHYSICAL_SECTOR_SIZE];
	for (DWORD cur_LBA = 0; cur_LBA < cmd_length; cur_LBA++) {
		title.Format(_T("LBA %u"), start_LBA + cur_LBA);
		diff_table = CString(_T("\tLBA Pattern")) + CString(_T("\t\t\t\t\t\t\t\t\t\t\tRead Data\n"));
		get_LBA_pattern(expect_data, start_LBA + cur_LBA, loop_map[start_LBA + cur_LBA - LBA_start]);

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

BOOL StorTest::sfun_sequential_a(HANDLE hDevice, WORD cur_loop, STL_RNG stl_rng)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen();

	msg.Format(_T("\tStart loop %5u sequential write/read\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_start;
	cur_LBA_cnt = 0;
	while (cur_LBA < LBA_end)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		// check remain LBA
		DWORD wr_sec_num = stl_rng();
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
			set_terminate();
			close_hDevice();
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
			set_terminate();
			close_hDevice();
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
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Find an error pattern. Show in error log.");
			}
		}

		cur_LBA += wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::sfun_sequential_b(HANDLE hDevice, WORD cur_loop, STL_RNG stl_rng, BOOL use_max)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen();

	msg.Format(_T("\tStart loop %5u sequential write\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_start;
	cur_LBA_cnt = 0;
	while (cur_LBA < LBA_end)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		DWORD wr_sec_num;
		if (use_max) {
			wr_sec_num = selected_device.getMaxTransfSec();
		}
		else {
			wr_sec_num = stl_rng();
		}
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
			set_terminate();
			close_hDevice();
			throw std::runtime_error("Write LBA failed.");
		}
		QueryPerformanceCounter(&nEndTime); // timer end
		cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
		msg.Format(_T("Loop %5u Write LBA: %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
			cur_loop, cur_LBA, cur_LBA + wr_sec_num - 1, wr_sec_num, cmd_time);
		set_cmd_msg(msg);

		cur_LBA += wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::sfun_sequential_c(HANDLE hDevice, WORD cur_loop)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen(), max_transfer_sec = selected_device.getMaxTransfSec();

	msg.Format(_T("\tStart loop %5u sequential read\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_start;
	cur_LBA_cnt = 0;
	while (cur_LBA < LBA_end)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		DWORD wr_sec_num = max_transfer_sec;
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
			set_terminate();
			close_hDevice();
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
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Find an error pattern. Show in error log.");
			}
		}

		cur_LBA += wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::sfun_reverse_a(HANDLE hDevice, WORD cur_loop, STL_RNG stl_rng)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen();

	msg.Format(_T("\tStart loop %5u reverse write/read\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_end;
	cur_LBA_cnt = 0;
	while (cur_LBA > LBA_start)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		// check remain LBA
		DWORD wr_sec_num = stl_rng();
		wr_sec_num = (wr_sec_num < (cur_LBA - LBA_start)) ? wr_sec_num : (cur_LBA - LBA_start);
		DWORD begin_LBA = cur_LBA - wr_sec_num;
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// get LBA pattern
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, begin_LBA + i, cur_loop);
		}

		// wrtie LBA
		QueryPerformanceCounter(&nBeginTime); // timer begin
		ULONGLONG begin_LBA_offset = (ULONGLONG)begin_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, begin_LBA_offset, wr_data, wr_sec_len, TRUE)) {
			TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			write_log_file(); // write cmd and error log, ehance the msg buffer is empty
			CloseHandle(cmd_file_hand);
			set_terminate();
			close_hDevice();
			throw std::runtime_error("Write LBA failed.");
		}
		QueryPerformanceCounter(&nEndTime); // timer end
		cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
		msg.Format(_T("Loop %5u Write LBA: %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
			cur_loop, begin_LBA, cur_LBA - 1, wr_sec_num, cmd_time);
		set_cmd_msg(msg);

		// read LBA
		QueryPerformanceCounter(&nBeginTime); // timer begin
		if (!SCSISectorIO(hDevice, max_transf_len, begin_LBA_offset, wr_data, wr_sec_len, FALSE)) {
			TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			write_log_file(); // write cmd and error log, ehance the msg buffer is empty
			CloseHandle(cmd_file_hand);
			set_terminate();
			close_hDevice();
			throw std::runtime_error("Read LBA failed.");
		}
		QueryPerformanceCounter(&nEndTime); // timer end
		cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
		msg.Format(_T("Loop %5u Read LBA:  %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
			cur_loop, begin_LBA, cur_LBA - 1, wr_sec_num, cmd_time);
		set_cmd_msg(msg);

		// compare pattern
		BYTE expect_data[PHYSICAL_SECTOR_SIZE];
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(expect_data, begin_LBA + i, cur_loop);

			if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
				msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, begin_LBA + i);
				set_log_msg(msg);
				msg.Format(_T("Reverse W/R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, begin_LBA, wr_sec_num);
				set_error_msg(msg);
				diff_cmd(cur_loop, begin_LBA, wr_sec_num, wr_data);

				// abort stortest
				set_terminate();
				delete[] wr_data;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Find an error pattern. Show in error log.");
			}
		}

		cur_LBA -= wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::sfun_reverse_b(HANDLE hDevice, WORD cur_loop, STL_RNG stl_rng)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen();

	msg.Format(_T("\tStart loop %5u reverse write\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_end;
	cur_LBA_cnt = 0;
	while (cur_LBA > LBA_start)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		DWORD wr_sec_num = stl_rng();
		// check remain LBA
		wr_sec_num = (wr_sec_num < (cur_LBA - LBA_start)) ? wr_sec_num : (cur_LBA - LBA_start);
		DWORD begin_LBA = cur_LBA - wr_sec_num;
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// get LBA pattern
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, begin_LBA + i, cur_loop);
		}

		// wrtie LBA
		QueryPerformanceCounter(&nBeginTime); // timer begin
		ULONGLONG begin_LBA_offset = (ULONGLONG)begin_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, begin_LBA_offset, wr_data, wr_sec_len, TRUE)) {
			TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			write_log_file(); // write cmd and error log, ehance the msg buffer is empty
			CloseHandle(cmd_file_hand);
			set_terminate();
			close_hDevice();
			throw std::runtime_error("Write LBA failed.");
		}
		QueryPerformanceCounter(&nEndTime); // timer end
		cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
		msg.Format(_T("Loop %5u Write LBA: %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
			cur_loop, begin_LBA, cur_LBA - 1, wr_sec_num, cmd_time);
		set_cmd_msg(msg);

		cur_LBA -= wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::sfun_reverse_c(HANDLE hDevice, WORD cur_loop)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen(), max_transfer_sec = selected_device.getMaxTransfSec();

	msg.Format(_T("\tStart loop %5u reverse read\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_end;
	cur_LBA_cnt = 0;
	while (cur_LBA > LBA_start)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		DWORD wr_sec_num = max_transfer_sec;
		// check remain LBA
		wr_sec_num = (wr_sec_num < (cur_LBA - LBA_start)) ? wr_sec_num : (cur_LBA - LBA_start);
		DWORD begin_LBA = cur_LBA - wr_sec_num;
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// get LBA pattern
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(wr_data + i * PHYSICAL_SECTOR_SIZE, begin_LBA + i, cur_loop);
		}

		// read LBA
		QueryPerformanceCounter(&nBeginTime); // timer begin
		ULONGLONG begin_LBA_offset = (ULONGLONG)begin_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, begin_LBA_offset, wr_data, wr_sec_len, FALSE)) {
			TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			write_log_file(); // write cmd and error log, ehance the msg buffer is empty
			CloseHandle(cmd_file_hand);
			set_terminate();
			close_hDevice();
			throw std::runtime_error("Read LBA failed.");
		}
		QueryPerformanceCounter(&nEndTime); // timer end
		cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
		msg.Format(_T("Loop %5u Read LBA:  %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
			cur_loop, begin_LBA, cur_LBA - 1, wr_sec_num, cmd_time);
		set_cmd_msg(msg);

		// compare pattern
		BYTE expect_data[PHYSICAL_SECTOR_SIZE];
		for (DWORD i = 0; i < wr_sec_num; i++) {
			get_LBA_pattern(expect_data, begin_LBA + i, cur_loop);

			if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
				msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, begin_LBA + i);
				set_log_msg(msg);
				msg.Format(_T("Reverse R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, begin_LBA, wr_sec_num);
				set_error_msg(msg);
				diff_cmd(cur_loop, begin_LBA, wr_sec_num, wr_data);

				// abort stortest
				set_terminate();
				delete[] wr_data;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Find an error pattern. Show in error log.");
			}
		}

		cur_LBA -= wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::sfun_verify_c(HANDLE hDevice, WORD cur_loop)
{
	CString msg;
	DWORD cur_LBA;
	DWORD max_transf_len = selected_device.getMaxTransfLen(), max_transfer_sec = selected_device.getMaxTransfSec();

	msg.Format(_T("\tStart loop %5u sequential read\n"), cur_loop);
	set_log_msg(msg);

	cur_LBA = LBA_start;
	cur_LBA_cnt = 0;
	while (cur_LBA < LBA_end)
	{
		if (if_pause) continue;
		if (if_terminate) return FALSE;

		WORD pattern_loop;
		DWORD wr_sec_num = max_transfer_sec;
		// check remain LBA
		wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA)) ? wr_sec_num : (LBA_end - cur_LBA);
		DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
		BYTE* wr_data = new BYTE[wr_sec_len];

		// read LBA
		QueryPerformanceCounter(&nBeginTime); // timer begin
		ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
		if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
			TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
			delete[] wr_data;
			write_log_file(); // write cmd and error log, ehance the msg buffer is empty
			CloseHandle(cmd_file_hand);
			set_terminate();
			close_hDevice();
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
			// get pattern loop num
			DWORD loop_offset = i * PHYSICAL_SECTOR_SIZE + 16;
			pattern_loop = ((WORD)(*(wr_data + loop_offset)) >> 8) + *(wr_data + loop_offset + 1);

			get_LBA_pattern(expect_data, cur_LBA + i, pattern_loop);

			if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
				msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
				set_log_msg(msg);
				msg.Format(_T("Verify R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, cur_LBA, wr_sec_num);
				set_error_msg(msg);
				diff_cmd(pattern_loop, cur_LBA, wr_sec_num, wr_data);

				// abort stortest
				set_terminate();
				delete[] wr_data;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Find an error pattern. Show in error log.");
			}
		}

		cur_LBA += wr_sec_num;
		cur_LBA_cnt += wr_sec_num;

		delete[] wr_data;
	}

	return TRUE;
}

BOOL StorTest::fun_sequential_ac()
{
	// initial random
	std::random_device rd;
	RNGInt generator(rd());
	STL_RNG stl_rng(generator, wr_sector_min, wr_sector_max);

	CString msg;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W/R
		if (!sfun_sequential_a(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_sequential_c(hDevice, cur_loop)) break;

		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
	}

	return TRUE;
}

BOOL StorTest::fun_sequential_bc()
{
	// initial random
	std::random_device rd;
	RNGInt generator(rd());
	STL_RNG stl_rng(generator, wr_sector_min, wr_sector_max);

	CString msg;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W
		if (!sfun_sequential_b(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_sequential_c(hDevice, cur_loop)) break;

		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
	}

	return TRUE;
}

BOOL StorTest::fun_reverse_ac()
{
	// initial random
	std::random_device rd;
	RNGInt generator(rd());
	STL_RNG stl_rng(generator, wr_sector_min, wr_sector_max);

	CString msg;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W/R
		if (!sfun_reverse_a(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_reverse_c(hDevice, cur_loop)) break;

		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
	}

	return TRUE;
}

BOOL StorTest::fun_reverse_bc()
{
	// initial random
	std::random_device rd;
	RNGInt generator(rd());
	STL_RNG stl_rng(generator, wr_sector_min, wr_sector_max);

	CString msg;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W
		if (!sfun_reverse_b(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_reverse_c(hDevice, cur_loop)) break;

		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
	}

	return TRUE;
}

BOOL StorTest::fun_testmode()
{
	// initial random
	std::random_device rd;
	RNGInt generator(rd());
	STL_RNG stl_rng(generator, wr_sector_min, wr_sector_max);

	CString msg;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num;) {
		// sequential a+c
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W/R
		if (!sfun_sequential_a(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_sequential_c(hDevice, cur_loop)) break;

		cur_loop++;
		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);

		// reverse a+c
		// open command log file
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W/R
		if (!sfun_reverse_a(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_reverse_c(hDevice, cur_loop)) break;

		cur_loop++;
		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);

		// sequential b+c
		// open command log file
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// W/R
		if (!sfun_sequential_b(hDevice, cur_loop, stl_rng)) break;
		// R
		if (!sfun_sequential_c(hDevice, cur_loop)) break;

		cur_loop++;
		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
	}

	return TRUE;
}

BOOL StorTest::fun_onewrite()
{
	// initial random
	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<int> distribution(wr_sector_min, wr_sector_max);

	CString msg;
	DWORD cur_LBA = LBA_start;
	DWORD max_transf_len = selected_device.getMaxTransfLen();
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
			set_terminate();
			close_hDevice();
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

	return TRUE;
}

BOOL StorTest::fun_verify()
{
	CString msg;
	for (WORD cur_loop = 0; loop_num == 0 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		// R
		if (!sfun_verify_c(hDevice, cur_loop)) break;

		cur_loop_cnt++;

		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);
	}

	return TRUE;
}

BOOL StorTest::fun_varyzone()
{
	// initial random
	std::random_device rd;
	RNGInt generator(rd());
	STL_RNG wr_sec_rng(generator, wr_sector_min, wr_sector_max);
	STL_RNG begin_LBA_rng(generator, LBA_start, LBA_end - 1);

	CString msg;
	// record LBA and loop
	WORD* LBA_loop_map;
	LBA_loop_map = new WORD[LBA_end - LBA_start];

	// sequential b+c
	// open command log file
	CString cmd_file_name;
	cmd_file_name.Format(_T("\\loop%05u_command.txt"), 0);
	cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

	// W
	if (!sfun_sequential_b(hDevice, 0, wr_sec_rng, TRUE)) {
		delete[] LBA_loop_map;
		close_hDevice();
		return TRUE;
	}

	// update loop map
	memset(LBA_loop_map, 0, (LBA_end - LBA_start) * 2);

	// R
	if (!sfun_sequential_c(hDevice, 0)) {
		delete[] LBA_loop_map;
		close_hDevice();
		return TRUE;
	}

	cur_loop_cnt++;
	write_log_file(); // write cmd and error log, ehance the msg buffer is empty
	CloseHandle(cmd_file_hand);

	// varyzone
	DWORD max_transf_len = selected_device.getMaxTransfLen();
	DWORD write_LBA;
	for (WORD cur_loop = 1; loop_num == 1 || cur_loop < loop_num; cur_loop++) {
		// open command log file
		CString cmd_file_name;
		cmd_file_name.Format(_T("\\loop%05u_command.txt"), cur_loop);
		cmd_file_hand = get_file_handle(dir_path + cmd_file_name);

		msg.Format(_T("\tStart loop %5u varyzone write/read\n"), cur_loop);
		set_log_msg(msg);

		vector<DWORD> write_LBA_record, write_len_record;
		write_LBA = 0;
		cur_LBA_cnt = 0;
		while (write_LBA < test_len_pro_loop)
		{
			if (if_pause) continue;
			if (if_terminate) break;

			DWORD begin_LBA = begin_LBA_rng(), wr_sec_num = wr_sec_rng();
			// check remain LBA
			wr_sec_num = (wr_sec_num < (test_len_pro_loop - write_LBA)) ? wr_sec_num : (test_len_pro_loop - write_LBA);
			wr_sec_num = (wr_sec_num < (LBA_end - begin_LBA)) ? wr_sec_num : (LBA_end - begin_LBA);
			DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* w_data = new BYTE[wr_sec_len];

			// get LBA pattern
			for (DWORD i = 0; i < wr_sec_num; i++) {
				get_LBA_pattern(w_data + i * PHYSICAL_SECTOR_SIZE, begin_LBA + i, cur_loop);
			}

			write_LBA_record.push_back(begin_LBA);
			write_len_record.push_back(wr_sec_num);
			// wrtie LBA
			QueryPerformanceCounter(&nBeginTime); // timer begin
			ULONGLONG begin_LBA_offset = (ULONGLONG)begin_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, begin_LBA_offset, w_data, wr_sec_len, TRUE)) {
				TRACE(_T("\n[Error] Write LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] w_data;
				delete[] LBA_loop_map;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Write LBA failed.");
			}
			QueryPerformanceCounter(&nEndTime); // timer end
			cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
			msg.Format(_T("Loop %5u Write LBA: %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
				cur_loop, begin_LBA, begin_LBA + wr_sec_num - 1, wr_sec_num, cmd_time);
			set_cmd_msg(msg);

			// update loop map
			for (WORD i = 0; i < wr_sec_num; i++) {
				LBA_loop_map[begin_LBA - LBA_start + i] = cur_loop;
			}

			delete[] w_data;

			// get varyzone read range
			DWORD read_begin_LBA = begin_LBA;
			DWORD r_sec_num = wr_sec_num;
			if (begin_LBA != 0) {
				read_begin_LBA--;
				r_sec_num++;
			}
			if (read_begin_LBA + r_sec_num < LBA_end - 1) {
				r_sec_num++;
			}
			DWORD r_sec_len = r_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* r_data = new BYTE[r_sec_len];
			// read LBA
			QueryPerformanceCounter(&nBeginTime); // timer begin
			ULONGLONG read_begin_LBA_offset = (ULONGLONG)read_begin_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, read_begin_LBA_offset, r_data, r_sec_len, FALSE)) {
				TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] r_data;
				delete[] LBA_loop_map;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				set_terminate();
				close_hDevice();
				throw std::runtime_error("Read LBA failed.");
			}
			QueryPerformanceCounter(&nEndTime); // timer end
			cmd_time = ((double)(nEndTime.QuadPart - nBeginTime.QuadPart) * 1000) / (double)nFreq.QuadPart;
			msg.Format(_T("Loop %5u Read LBA:  %10u~%10u (size: %4u) Elapsed %8.3f ms\n"),
				cur_loop, read_begin_LBA, read_begin_LBA + r_sec_num - 1, r_sec_num, cmd_time);
			set_cmd_msg(msg);

			// compare pattern
			BYTE expect_data[PHYSICAL_SECTOR_SIZE];
			for (DWORD i = 0; i < r_sec_num; i++) {
				get_LBA_pattern(expect_data, read_begin_LBA + i, LBA_loop_map[read_begin_LBA + i - LBA_start]);

				if (!compare_sector(expect_data, r_data + i * PHYSICAL_SECTOR_SIZE)) {
					msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, read_begin_LBA + i);
					set_log_msg(msg);
					msg.Format(_T("Varyzone W/R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, read_begin_LBA, r_sec_num);
					set_error_msg(msg);
					diff_cmd(LBA_loop_map, read_begin_LBA, r_sec_num, r_data);

					// abort stortest
					set_terminate();
					delete[] r_data;
					delete[] LBA_loop_map;
					write_log_file(); // write cmd and error log, ehance the msg buffer is empty
					CloseHandle(cmd_file_hand);
					set_terminate();
					close_hDevice();
					throw std::runtime_error("Find an error pattern. Show in error log.");
				}
			}

			delete[] r_data;

			write_LBA += wr_sec_num;
			cur_LBA_cnt += wr_sec_num;

		}

		msg.Format(_T("\tStart loop %5u varyzone read\n"), cur_loop);
		set_log_msg(msg);

		// Read again
		cur_LBA_cnt = 0;
		for (DWORD i = 0; i < write_LBA_record.size(); i++) {
			DWORD cur_LBA = write_LBA_record.at(i), wr_sec_num = write_len_record.at(i);
			DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
			BYTE* wr_data = new BYTE[wr_sec_len];

			// read LBA
			QueryPerformanceCounter(&nBeginTime); // timer begin
			ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
			if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
				TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
				delete[] wr_data;
				delete[] LBA_loop_map;
				write_log_file(); // write cmd and error log, ehance the msg buffer is empty
				CloseHandle(cmd_file_hand);
				set_terminate();
				close_hDevice();
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
				get_LBA_pattern(expect_data, cur_LBA + i, LBA_loop_map[cur_LBA + i - LBA_start]);

				if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
					msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
					set_log_msg(msg);
					msg.Format(_T("Varyzone R Loop: %u, LBA: %u, Size: %u\n"), cur_loop, cur_LBA, wr_sec_num);
					set_error_msg(msg);
					diff_cmd(LBA_loop_map, cur_loop, wr_sec_num, wr_data);

					// abort stortest
					set_terminate();
					delete[] wr_data;
					delete[] LBA_loop_map;
					write_log_file(); // write cmd and error log, ehance the msg buffer is empty
					CloseHandle(cmd_file_hand);
					set_terminate();
					close_hDevice();
					throw std::runtime_error("Find an error pattern. Show in error log.");
				}
			}

			delete[] wr_data;

			cur_LBA_cnt += wr_sec_num;
		}

		// verify all
		if (cur_loop % test_loop_per_verify_all == 0) {
			msg.Format(_T("\tStart loop %5u varyzone verifyall\n"), cur_loop);
			set_log_msg(msg);

			DWORD cur_LBA;
			DWORD max_transf_len = selected_device.getMaxTransfLen(), max_transfer_sec = selected_device.getMaxTransfSec();

			cur_LBA = LBA_start;
			while (cur_LBA < LBA_end)
			{
				if (if_pause) continue;
				if (if_terminate) break;

				DWORD wr_sec_num = max_transfer_sec;
				// check remain LBA
				wr_sec_num = (wr_sec_num < (LBA_end - cur_LBA)) ? wr_sec_num : (LBA_end - cur_LBA);
				DWORD wr_sec_len = wr_sec_num * PHYSICAL_SECTOR_SIZE;
				BYTE* wr_data = new BYTE[wr_sec_len];

				// read LBA
				QueryPerformanceCounter(&nBeginTime); // timer begin
				ULONGLONG cur_LBA_offset = (ULONGLONG)cur_LBA * PHYSICAL_SECTOR_SIZE;
				if (!SCSISectorIO(hDevice, max_transf_len, cur_LBA_offset, wr_data, wr_sec_len, FALSE)) {
					TRACE(_T("\n[Error] Read LBA failed. Error Code = %u.\n"), GetLastError());
					delete[] LBA_loop_map;
					delete[] wr_data;
					write_log_file(); // write cmd and error log, ehance the msg buffer is empty
					CloseHandle(cmd_file_hand);
					set_terminate();
					close_hDevice();
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
					get_LBA_pattern(expect_data, cur_LBA + i, LBA_loop_map[cur_LBA + i - LBA_start]);

					if (!compare_sector(expect_data, wr_data + i * PHYSICAL_SECTOR_SIZE)) {
						msg.Format(_T("\tFound expect in loop %u and LBA %u\n"), cur_loop, cur_LBA + i);
						set_log_msg(msg);
						msg.Format(_T("Varyzone verifyall Loop: %u, LBA: %u, Size: %u\n"), cur_loop, cur_LBA, wr_sec_num);
						set_error_msg(msg);
						diff_cmd(LBA_loop_map, cur_LBA, wr_sec_num, wr_data);

						// abort stortest
						set_terminate();
						delete[] LBA_loop_map;
						delete[] wr_data;
						write_log_file(); // write cmd and error log, ehance the msg buffer is empty
						CloseHandle(cmd_file_hand);
						set_terminate();
						close_hDevice();
						throw std::runtime_error("Find an error pattern. Show in error log.");
					}
				}

				cur_LBA += wr_sec_num;

				delete[] wr_data;
			}
		}

		cur_loop_cnt++;
		write_log_file(); // write cmd and error log, ehance the msg buffer is empty
		CloseHandle(cmd_file_hand);

		if (if_terminate) break;
	}

	delete[] LBA_loop_map;
	return TRUE;
}

BOOL StorTest::run()
{
	CString msg;
	// device information
	hDevice = selected_device.openDevice();
	if (hDevice == INVALID_HANDLE_VALUE) {
		TRACE(_T("\n[Error] Open device failed. Error Code = %u.\n"), GetLastError());
		msg.Format(_T("Open device failed. Error Code = %u."), GetLastError());
		throw msg;
	}

	// lock volume
	DWORD bytes_returned = 0;
	if (!DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
		TRACE("\n[Error] FSCTL_LOCK_VOLUME failed. Error code = %u.\n", GetLastError());
		msg.Format(_T("FSCTL_LOCK_VOLUME failed. Error code = %u."), GetLastError());
		throw msg;
	}

	// dismount volume
	if (!DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL)) {
		TRACE("\n[Error] FSCTL_DISMOUNT_VOLUME failed. Error code = %u.\n", GetLastError());
		msg.Format(_T("FSCTL_DISMOUNT_VOLUME failed. Error code = %u."), GetLastError());
		throw msg;
	}

	BOOL rtn;
	switch (function_idx)
	{
	case 0:
		set_log_msg(CString(_T("Start Sequential mode a+c\n")));
		rtn = fun_sequential_ac();
		break;
	case 1:
		set_log_msg(CString(_T("Start Sequential mode b+c\n")));
		rtn = fun_sequential_bc();
		break;
	case 2:
		set_log_msg(CString(_T("Start Reverse mode a+c\n")));
		rtn = fun_reverse_ac();
		break;
	case 3:
		set_log_msg(CString(_T("Start Reverse mode b+c\n")));
		rtn = fun_reverse_bc();
		break;
	case 4:
		set_log_msg(CString(_T("Start TestMode\n")));
		rtn = fun_testmode();
		break;
	case 5:
		set_log_msg(CString(_T("Start OneWrite\n")));
		rtn = fun_onewrite();
		break;
	case 6:
		set_log_msg(CString(_T("Start Verify\n")));
		rtn = fun_verify();
		break;
	case 7:
		set_log_msg(CString(_T("Start Varyzone\n")));
		rtn = fun_varyzone();
		break;
	default:
		throw std::runtime_error("Function setup error.");
	}

	close_hDevice();
	return rtn;
}

BOOL StorTest::open_log_dir()
{
	CString function_folder_map[8] = { _T("Seq(wr+r)"), _T("Seq(w+r)"),
								_T("Rev(wr+r)"), _T("Rev(w+r)"),
								_T("Testmode"), _T("Onewrite"), _T("Verify"), _T("Varyzone") };

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
