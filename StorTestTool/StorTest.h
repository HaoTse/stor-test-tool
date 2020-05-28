#pragma once

#include <atomic>
#include <mutex>
#include <random>

#include "device.h"

// c++11 random generator
typedef std::mt19937 RNGInt;
typedef struct STL_RNG
{
	STL_RNG(RNGInt& rng, DWORD _min, DWORD _max) : gen(rng), wr_sec_min(_min), wr_sec_max(_max) {}
	RNGInt& gen;
	DWORD wr_sec_min, wr_sec_max;
	DWORD operator()() { return std::uniform_int_distribution<DWORD>(wr_sec_min, wr_sec_max)(gen); }
} STL_RNG;

class StorTest
{
private:
	// setup parameters
	Device selected_device;
	DWORD function_idx{ 0 }, LBA_start{ 0 }, LBA_end{ 0 }, wr_sector_min{ 0 }, wr_sector_max{ 0 };
	WORD loop_num{ 0 };

	// control progress bar
	std::atomic_uint cur_LBA_cnt{ 0 }, cur_loop_cnt{ 0 };
	std::atomic_bool if_terminate{ FALSE }, if_pause{ FALSE };

	// log msg
	std::mutex log_msg_mutex, cmd_msg_mutex, error_msg_mutex;
	CString log_msg{ CString(_T("")) }, cmd_msg{ CString(_T("")) }, error_msg{ CString(_T("")) };
	
	// log files 
	CString dir_path;
	HANDLE cmd_file_hand{ NULL }, error_file_hand{ NULL };

	// timer
	LARGE_INTEGER nFreq{ NULL };
	LARGE_INTEGER nBeginTime{ NULL };
	LARGE_INTEGER nEndTime{ NULL };
	double cmd_time{ NULL };

	void dec_in_hex(BYTE* hex_byte, DWORD num);
	void get_LBA_pattern(BYTE* LBA_pattern, DWORD LBA, WORD loop);

	void set_log_msg(CString msg);
	void set_cmd_msg(CString msg);
	void set_error_msg(CString msg);
	
	BOOL compare_sector(BYTE* expect_buf, BYTE* read_buf);
	void diff_cmd(WORD loop, DWORD start_LBA, DWORD cmd_length, BYTE* read_buf);

	HANDLE get_file_handle(CString file_path);

	BOOL sfun_sequential_a(HANDLE hDevice, WORD cur_loop, STL_RNG stl_rng);
	BOOL sfun_sequential_b(HANDLE hDevice, WORD cur_loop, STL_RNG stl_rng);
	BOOL sfun_sequential_c(HANDLE hDevice, WORD cur_loop);

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
	void close_error_log_file();
	void write_log_file();
	CString get_log_msg();

	UINT get_cur_LBA_cnt();
	UINT get_cur_loop();

	void set_terminate();
	BOOL get_terminate();
	void set_pause(bool setup);
	BOOL get_pause();
};