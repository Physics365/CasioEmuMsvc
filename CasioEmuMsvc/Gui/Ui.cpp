#include "ui.hpp"
#include "CallAnalysis.h"
#include "CasioData.h"
#include "Chipset/Chipset.hpp"
#include "Chipset/MMU.hpp"
#include "CodeViewer.hpp"
#include "Editors.h"
#include "HwController.h"
#include "Injector.hpp"
#include "LabelFile.h"
#include "LabelViewer.h"
#include "MemBreakpoint.hpp"
#include "VariableWindow.h"
#include "WatchWindow.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_sdlrenderer2.h"
#include <SDL.h>
#include <filesystem>

char* n_ram_buffer = 0;
casioemu::MMU* me_mmu = 0;

static SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
static SDL_Window* window = 0;
SDL_Renderer* renderer = 0;

std::vector<Label> g_labels;

CodeViewer* code_viewer = 0;
Injector* injector = 0;
MemBreakPoint* membp = 0;

std::vector<UIWindow*> windows{};

static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
const ImWchar* GetPua() {
	static const ImWchar ranges[] = {
		0xE000,
		0xE900, // PUA
		0,
	};
	return &ranges[0];
}
inline const ImWchar* GetKanji() {
	static const ImWchar ranges[] = {
		0x2000,
		0x206F, // General Punctuation
		0x3000,
		0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
		0x31F0,
		0x31FF, // Katakana Phonetic Extensions
		0xFF00,
		0xFFEF, // Half-width characters
		0xFFFD,
		0xFFFD, // Invalid
		0x4e00,
		0x9FAF, // CJK Ideograms
		0,
	};
	return &ranges[0];
}
void gui_loop() {
	if (!m_emu->Running())
		return;

	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	for (auto win : windows) {
		win->Render();
	}
	ImGui::Render();
	SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
	SDL_RenderClear(renderer);
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
	SDL_RenderPresent(renderer);
}
int test_gui(bool* guiCreated) {
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
	window = SDL_CreateWindow("CasioEmuMsvc Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	if (renderer == nullptr) {
		SDL_Log("Error creating SDL_Renderer!");
		return 0;
	}
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImFontConfig config;
	config.MergeMode = true;
	if (std::filesystem::exists("C:\\Windows\\Fonts\\CascadiaCode.ttf"))
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\CascadiaCode.ttf", 15);
	else {
		printf("[Ui][Warn] \"CascadiaCode.ttf\" not found\n");
		io.Fonts->AddFontDefault();
	}
#if LANGUAGE == 2
	if (std::filesystem::exists("NotoSansSC-Medium.otf"))
		io.Fonts->AddFontFromFileTTF("NotoSansSC-Medium.otf", 18, &config, GetKanji());
	else if (std::filesystem::exists("C:\\Windows\\Fonts\\msyh.ttc")) {
		printf("[Ui][Warn] fallback to MSYH.\n");
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18, &config, GetKanji());
	}
	else {
		printf("[Ui][Warn] No chinese font available!\n");
	}
#else
#endif
	// config.GlyphOffset = ImVec2(0,1.5);
	io.Fonts->Build();
	io.WantCaptureKeyboard = true;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Setup Platform/Renderer backends
	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	style.FrameRounding = 4.0f;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);
	if (guiCreated)
		*guiCreated = true;
	while (!me_mmu)
		std::this_thread::sleep_for(std::chrono::microseconds(1));

	g_labels = parseFile(m_emu->GetModelFilePath("labels"));

	for (auto item : std::initializer_list<UIWindow*>{
			 new VariableWindow(),
			 new HwController(),
			 new LabelViewer(),
			 new CasioData(),
			 new WatchWindow(),
			 CreateCallAnalysisWindow(),
			 code_viewer = new CodeViewer(),
			 injector = new Injector(),
			 membp = new MemBreakPoint()})
		windows.push_back(item);
	for (auto item : GetEditors())
		windows.push_back(item);
	return 0;
}

void gui_cleanup() {
	// Cleanup
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
