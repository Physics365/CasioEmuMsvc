#pragma once
#include "Ui.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
typedef struct {
	uint32_t offset;
	char srcbuf[80];
} CodeElem;

enum EmuDebugFlag {
	DEBUG_BREAKPOINT = 1,
	DEBUG_STEP = 2,
	DEBUG_RET_TRACE = 4
};
class CodeViewer : public UIWindow {
private:
	std::map<int, uint8_t> break_points;
	std::vector<CodeElem> codes;
	size_t rows;
	std::string src_path;
	char adrbuf[9]{0};
	int max_row = 0;
	int max_col = 0;
	int cur_col = 0;

	bool is_loaded = false;
	bool edit_active = false;
	bool need_roll = false;
	uint32_t selected_addr = -1;

public:
	uint8_t debug_flags = DEBUG_BREAKPOINT;
	CodeViewer() : UIWindow("Code") {
		PrepareDisasm();
		SetupHooks();
	}
	void SetupHooks();
	void PrepareDisasm();
	bool TryTrigBP(uint8_t seg, uint16_t offset, bool bp_mode = true);
	CodeElem LookUp(uint32_t offset, int* idx = 0);
	void RenderCore() override;
	void DrawContent();
	void DrawMonitor();
	void JumpTo(uint32_t offset);
};