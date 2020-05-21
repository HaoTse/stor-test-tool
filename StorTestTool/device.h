#pragma once

// The physical sector size in bytes
#define PHYSICAL_SECTOR_SIZE 512
#define PHYSICAL_SECTOR_SIZE_POW2 9

class Device
{
private:
	char ident;
	DWORD capacity_sec, byte_per_sec, max_transf_len;

	void initCapacity();
	void initMaxTransfLen();
public:
	Device();
	Device(char ident);
	HANDLE openDevice();
	char getIdent();
	DWORD getCapacitySec();
	DWORD getMaxTransfLen();
	DWORD getMaxTransfSec();
	BOOL isValid();
	CString showText();
};
