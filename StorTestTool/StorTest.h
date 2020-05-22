#pragma once

#include "device.h"

class StorTest
{
private:
	DWORD function_idx{ 0 }, LBA_start{ 0 }, LBA_end{ 0 }, wr_sector_min{ 0 }, wr_sector_max{ 0 };
	Device selected_device;
	WORD loop_num{ 0 };
	
	void dec_in_hex(BYTE* hex_byte, DWORD num);
	void get_LBA_pattern(BYTE* LBA_pattern, DWORD buf_offset, DWORD LBA, WORD loop);

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
};