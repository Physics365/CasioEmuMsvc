#include "HwController.h"
#include "..\Config.hpp"
#include "Ui.hpp"
#include "imgui\imgui.h"

int screen_flashing_threshold = 20;
float screen_fading_blending_coefficient = 0;
bool enable_screen_fading = false;
float screen_flashing_brightness_coeff = 1.5f;
int screen_buffer_select = 0;
bool audio_enable = false;
void HwController::RenderCore() {
	ImGui::SliderInt(
#if LANGUAGE == 2
		"闪屏模拟阈值"
#else
		"screen_flashing_threshold"
#endif
		,
		&screen_flashing_threshold, 0, 0x3F);
	ImGui::SliderFloat(
#if LANGUAGE == 2
		"闪屏对比度补偿"
#else
		"screen_flashing_brightness_coeff"
#endif
		,
		&screen_flashing_brightness_coeff, 1, 8);
	//	ImGui::Checkbox(
	// #if LANGUAGE == 2
	//		"启用屏幕渐变"
	// #else
	//		"enable_screen_fading"
	// #endif
	//		,
	//		&enable_screen_fading);
	//	if (enable_screen_fading)
	//		ImGui::SliderFloat(
	// #if LANGUAGE == 2
	//			"屏幕渐变混合因子"
	// #else
	//			"Blending rate"
	// #endif
	//			,
	//			&screen_fading_blending_coefficient, 0, 1);
	ImGui::Separator();
	ImGui::SliderInt(
#if LANGUAGE == 2
		"屏幕缓冲区选择"
#else
		"Screen buffer select"
#endif
		,
		&screen_buffer_select, 0, 2);
	ImGui::Separator();
	static int cps = log(m_emu->cycles.cycles_per_second) / log(2);
	if (ImGui::SliderInt(
#if LANGUAGE == 2
			"每秒周期数"
#else
			"Cycles per second"
#endif
			,
			&cps, 1, 28, "2^%d CPS")) {
		m_emu->cycles.Setup((Uint64)1 << cps, m_emu->cycles.timer_interval);
	}
	ImGui::Text("%.6f MHz", (double)m_emu->cycles.cycles_per_second / 1024 / 1024);
	//	static char buf4[40];
	//	ImGui::InputText("##cps_in", buf4, 40);
	//	ImGui::SameLine();
	//	if (ImGui::Button(
	// #if LANGUAGE == 2
	//			"设置"
	// #else
	//			"Set"
	// #endif
	//			)) {
	//		m_emu->cycles.Setup((std::stoi(buf4)), m_emu->cycles.timer_interval);
	//	}

	//	ImGui::Checkbox(
	// #if LANGUAGE == 2
	//		"音响，启动！"
	// #else
	//		"Enable Audio Controller"
	// #endif
	//		, &audio_enable);
}
