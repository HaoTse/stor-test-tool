#pragma once

#include <atomic>
#include <mutex>

#include "device.h"

class StorTest
{
private:
	Device selected_device;
	DWORD function_idx{ 0 }, LBA_start{ 0 }, LBA_end{ 0 }, wr_sector_min{ 0 }, wr_sector_max{ 0 };
	WORD loop_num{ 0 };
	std::atomic_uint cur_LBA_cnt{ 0 }, cur_loop_cnt{ 0 };
	std::mutex log_msg_mutex, cmd_msg_mutex, error_msg_mutex;
	CString log_msg{ CString(_T("")) }, cmd_msg{ CString(_T("")) }, error_msg{ CString(_T("")) };
	std::atomic_bool if_terminate{ FALSE }, if_pause{ FALSE };
	HANDLE cmd_file_hand{ NULL }, error_file_hand{ NULL };

	// timer
	LARGE_INTEGER nFreq;
	LARGE_INTEGER nBeginTime;
	LARGE_INTEGER nEndTime;
	double cmd_time;
	
	void dec_in_hex(BYTE* hex_byte, DWORD num);
	void get_LBA_pattern(BYTE* LBA_pattern, DWORD LBA, WORD loop);
	void set_log_msg(CString msg);
	void set_cmd_msg(CString msg);
	void set_error_msg(CString msg);
	BOOL compare_sector(BYTE* expect_buf, BYTE* read_buf);
	void diff_cmd(WORD loop, DWORD start_LBA, DWORD cmd_length, BYTE* read_buf);

	BOOL fun_sequential_ac();
	BOOL fun_sequential_bc();
	BOOL fun_reverse_ac();
	BOOL fun_reverse_bc();
	BOOL fun_testmode();
	BOOL fun_onewrite();
	BOOL fun_verify();
	BOOL fun_varyzone();

public:
	StorTest(Device dev, DWORD fun_idx, DWORD start, DWORD end, DWORD smin, DWORD smax, WORD loopn);
	BOOL run();
	BOOL open_log_dir();
	void close_log_files();

	UINT get_cur_LBA_cnt();
	UINT get_cur_loop();
	CString get_log_msg();
	void set_terminate();
	BOOL get_terminate();
	void set_pause(bool setup);
	BOOL get_pause();
};