#include "ui.hpp"
#include "../Chipset/Chipset.hpp"
#include "../Chipset/MMU.hpp"
#include "CallAnalysis.h"
#include "CasioData.h"
#include "CodeViewer.hpp"
#include "Editors.h"
#include "HwController.h"
#include "Injector.hpp"
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

static SDL_Window* window = 0;
SDL_Renderer* renderer = 0;

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
    ImGui::SetNextWindowBgAlpha(0.0f); // 设置透明背景
    ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    ImGui::SetWindowPos(ImVec2(0, 0)); // 设置窗口位置，这决定了按钮的位置
    static UIWindow* current_filter = 0;
    if (ImGui::BeginCombo("##cb", current_filter ? current_filter->name : 0)) {
        for (int n = 0; n < windows.size(); n++) {
            bool is_selected = (current_filter == windows[n]); // You can store your selection however you want, outside or inside your objects
            if (ImGui::Selectable(windows[n]->name, is_selected))
                current_filter = windows[n];
            if (is_selected)
                ImGui::SetItemDefaultFocus(); // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
if(current_filter != 0)
    current_filter->open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Close all")) {
        for(auto& win:windows){
            win->open = false;
        }
    }
    ImGui::End();
	ImGui::Render();
	SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
}
int test_gui(bool* guiCreated,SDL_Window* wd,SDL_Renderer* rd) {
	window = wd;
	renderer = rd;
	if (renderer == nullptr) {
		SDL_Log("Error creating SDL_Renderer!");
		return 0;
	}
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 3;
    ImFontConfig config{};
   // config.OversampleH = 4;
   // config.OversampleV = 4;
   // config.PixelSnapH = true;
    config.MergeMode = true;
    io.Fonts->AddFontDefault();
#if LANGUAGE == 2
    if (std::filesystem::exists("/system/fonts/NotoSansCJK-Regular.ttc"))
        io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", 18, &config, GetKanji());
    else {
        printf("[Ui][Warn] No chinese font available!\n");
    }
#else
#endif
    //  config.GlyphOffset = ImVec2(0,1.5);
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
style.ScrollbarSize = 40.f;
style.ScrollbarRounding = 4.0f;
    style.ItemInnerSpacing = {10.0f,10.0f};
    style.ItemSpacing = {20.0f,20.0f};
    style.GrabMinSize = 40.f;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);
	if (guiCreated)
		*guiCreated = true;
	while (!m_emu || !me_mmu)
		std::this_thread::sleep_for(std::chrono::microseconds(1));
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

    for(auto item: windows){
        item->open = false;
    }


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
