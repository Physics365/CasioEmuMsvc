#pragma once
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "imgui/imgui.h"
int test_gui(bool* guiCreated);
void gui_cleanup();
void gui_loop();
extern char* n_ram_buffer;
extern casioemu::MMU* me_mmu;
extern casioemu::Emulator* m_emu;
class UIWindow {
public:
	UIWindow(const char* name) : name(name) {}
	const char* name{};
	bool open = true;
	ImVec2 inital_size{800, 800};
	ImGuiWindowFlags flags{};
	virtual void Render() {
		if (!open)
			return;
		ImGui::SetNextWindowSize(inital_size, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(name, &open, flags)) {
			RenderCore();
		}
		ImGui::End();
	}
	virtual void RenderCore() = 0;
};
constexpr ImGuiTableFlags pretty_table = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable ;