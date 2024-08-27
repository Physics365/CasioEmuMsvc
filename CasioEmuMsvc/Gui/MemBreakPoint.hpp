#pragma once
#include "Ui.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct Record {
	std::string stacktrace;
	uint32_t lr;
};

struct MemBPData_t {
	bool enableWrite = false;
	uint32_t addr;
	std::unordered_map<uint32_t, Record> records;
};

void SetMemBp(uint32_t addr, bool write);

class MemBreakPoint : public UIWindow {

private:
	std::vector<MemBPData_t> break_point_hash;

	int target_addr = -1;

	bool break_on_cv = false;

	void DrawFindContent();

	void DrawContent();

public:
	MemBreakPoint() : UIWindow("Memory") {
		SetupHooks();
	}

	void SetupHooks();

	void TryTrigBp(uint32_t addr_edit, bool write);

	void RenderCore() override;

	void ExternalAddBp(uint32_t addr, bool write);
};