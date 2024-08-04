#pragma once
#include <vector>
using word = unsigned short;
using byte = unsigned char;
struct RomInfo {
	char ver[9];
	byte cid[8];
	word desired_sum;
	word real_sum;
	enum {
		Unknown,
		ES,
		ESP,
		ESP2nd,
		CWX,
		CWII,
		Fx5800p,
	} type;
	bool ok;
};
RomInfo rom_info(std::vector<byte> rom, bool checksum = true);