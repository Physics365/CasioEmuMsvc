#include "CodeViewer.hpp"
#include "Chipset/CPU.hpp"
#include "Chipset/Chipset.hpp"
#include "Config.hpp"
#include "Emulator.hpp"
#include "Logger.hpp"
#include "U8Disas.h"
#include "Hooks.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
casioemu::Emulator* m_emu = nullptr;

uint32_t pc_cache = 0;

bool elem_cmp(const CodeElem& a, const CodeElem& b) {
	return a.offset < b.offset;
}

CodeElem CodeViewer::LookUp(uint32_t offset, int* idx) {
	CodeElem target;
	target.offset = offset;
	auto it = std::lower_bound(codes.begin(), codes.end(), target, elem_cmp);
	if (it == codes.end()) {
		it = codes.begin();
	}
	if (idx)
		*idx = it - codes.begin();
	return {.offset = it->offset};
}

void CodeViewer::SetupHooks() {
	SetupHook(on_instruction,
		[&](casioemu::CPU& cup, InstructionEventArgs& iea) {
			pc_cache = (cup.reg_csr << 16) | cup.reg_pc;
			if (debug_flags & DEBUG_STEP) {
				iea.should_break = true;
				int idx = 0;
				LookUp(pc_cache, &idx);
				break_points[idx] = 2;
				cur_col = idx;
				need_roll = true;
			}
			if (TryTrigBP(cup.reg_csr, cup.reg_pc)) {
				iea.should_break = true;
			}
			else if (TryTrigBP(cup.reg_csr, cup.reg_pc, false)) {
				iea.should_break = true;
			}
		});
}

void CodeViewer::PrepareDisasm() {
	std::thread t1([this]() {
#ifndef _DEBUG
		printf("[UI][Info] Start to disasm ...\n");
		uint8_t* beg = (uint8_t*)m_emu->chipset.rom_data.data();
		auto rom = beg;
		auto end = rom + m_emu->chipset.rom_data.size();
		std::stringstream ss{};
		while (rom < end) {
			auto pc = rom - beg;
			decode(ss, rom, rom - beg);
			CodeElem ce{};
			ce.offset = pc;
			auto s = ss.str();
			strcpy_s(ce.srcbuf, s.c_str());
			codes.push_back(ce);
			ss.str("");
		}
		printf("[UI][Info] Finished!\n");
		max_row = codes.size();
#endif
		is_loaded = true;
	});
	t1.join();
}

bool CodeViewer::TryTrigBP(uint8_t seg, uint16_t offset, bool bp_mode) {
	for (auto it = break_points.begin(); it != break_points.end(); it++) {
		if (it->second == 1) {
			// TODO: We ignore a second trigger
			CodeElem e = codes[it->first];
			if (e.offset == ((seg << 16) | offset)) {
				break_points[it->first] = 2;
				cur_col = it->first;
				need_roll = true;
				return true;
			}
		}
	}
	if (!bp_mode && (debug_flags & DEBUG_STEP || debug_flags & DEBUG_RET_TRACE)) {
		int idx = 0;
		LookUp((seg << 16) | offset, &idx);
		break_points[idx] = 2;
		cur_col = idx;
		need_roll = true;
		return true;
	}
	return false;
}

void CodeViewer::DrawContent() {
	ImGuiListClipper c;
	c.Begin(max_row, ImGui::GetTextLineHeight());
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	while (c.Step()) {
		for (int line_i = c.DisplayStart; line_i < c.DisplayEnd; line_i++) {
			CodeElem e = codes[line_i];
			auto it = break_points.find(line_i);
			auto bb = it == break_points.end();
			if (e.offset == pc_cache) {
				ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), "[ > ]");
			}
			else {
				if (bb) {
					ImGui::Text("[ o ]");
					if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
						break_points[line_i] = 1;
					}
				}
				else {
					if (it->second == 1) {
						ImGui::TextColored(ImVec4(1.0, 0.0, 0.0, 1.0), "[ x ]");
						if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
							break_points.erase(line_i);
						}
					}
					else {
						break_points.erase(line_i);
						ImGui::Text("[ o ]");
					}
				}
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "%05x", e.offset);
			ImGui::SameLine();
			ImGui::Text("%s", e.srcbuf);
		}
	}
	if (need_roll) {
		float v = (float)cur_col / max_row * ImGui::GetScrollMaxY();
		ImGui::SetScrollY(v);
		need_roll = false;
		selected_addr = codes[cur_col].offset;
	}
}

void CodeViewer::DrawMonitor() {
	if (m_emu != nullptr) {
		casioemu::Chipset& chipset = m_emu->chipset;
		std::string s = chipset.cpu.GetBacktrace();
		ImGui::InputTextMultiline("##as", (char*)s.c_str(), s.size(), ImVec2(ImGui::GetWindowWidth(), -1), ImGuiInputTextFlags_ReadOnly);
	}
}

static bool step_debug = false, trace_debug = false;

void CodeViewer::JumpTo(uint32_t offset) {
	int idx = 0;
	// printf("jumpto:seg%d\n",seg);
	LookUp(offset, &idx);
	cur_col = idx;
	need_roll = true;
}

void CodeViewer::RenderCore() {

	int h = ImGui::GetTextLineHeight() + 4;
	int w = ImGui::CalcTextSize("F").x;
	if (!is_loaded) {
		ImGui::SetCursorPos(ImVec2(w * 2, h * 5));
		ImGui::Text(
#if LANGUAGE == 2
			"请稍等，正在加载..."
#else
			"Waiting for emulator..."
#endif
		);
		return;
	}
	ImVec2 sz;
	h *= 10;
	w *= max_col;
	sz.x = w;
	sz.y = h;
	ImGui::BeginChild("##scrolling", ImVec2(0, -30));
	DrawContent();
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::Text(
#if LANGUAGE == 2
		"转到地址:"
#else
		"Goto:"
#endif
	);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::CalcTextSize("000000").x);
	ImGui::InputText("##input", adrbuf, 8);
	if (adrbuf[0] != '\0' && ImGui::IsItemFocused()) {
		uint32_t addr = std::stoi(adrbuf, 0, 16);
		JumpTo(addr);
	}
	ImGui::SameLine();
	ImGui::Checkbox(
#if LANGUAGE == 2
		"逐过程"
#else
		"Step Over"
#endif
		,
		&step_debug);
	ImGui::SameLine();
	ImGui::Checkbox(
#if LANGUAGE == 2
		"逐语句"
#else
		"Step Through"
#endif
		,
		&trace_debug);
	ImGui::SameLine();
	if (ImGui::Button("Continue")) {
		m_emu->SetPaused(false);
	}
	debug_flags = DEBUG_BREAKPOINT | (step_debug ? DEBUG_STEP : 0) | (trace_debug ? DEBUG_RET_TRACE : 0);
}