#include "StartupUi.h"
#include "../Binary.h"
#include "../Config.hpp"
#include "../Gui/imgui/imgui.h"
#include "../Gui/imgui/imgui_impl_sdl2.h"
#include "../Gui/imgui/imgui_impl_sdlrenderer2.h"
#include "../ModelInfo.h"
#include <SDL.h>
#include <SDL_image.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include "../Gui/Ui.hpp"
#include "Romu.h"

inline SDL_Window *window;
inline SDL_Renderer *renderer2;
inline std::vector<UIWindow *> windows2;

inline char *stristr(const char *str1, const char *str2) {
    const char *p1 = str1;
    const char *p2 = str2;
    const char *r = *p2 == 0 ? str1 : 0;

    while (*p1 != 0 && *p2 != 0) {
        if (tolower((unsigned char) *p1) == tolower((unsigned char) *p2)) {
            if (r == 0) {
                r = p1;
            }

            p2++;
        } else {
            p2 = str2;
            if (r != 0) {
                p1 = r + 1;
            }

            if (tolower((unsigned char) *p1) == tolower((unsigned char) *p2)) {
                r = p1;
                p2++;
            } else {
                r = 0;
            }
        }

        p1++;
    }

    return *p2 == 0 ? (char *) r : 0;
}

inline std::string tohex(int n, int len) {
    std::string retval = "";
    for (int x = 0; x < len; x++) {
        retval = "0123456789ABCDEF"[n & 0xF] + retval;
        n >>= 4;
    }
    return retval;
}

inline std::string tohex(unsigned long long n, int len) {
    std::string retval = "";
    for (int x = 0; x < len; x++) {
        retval = "0123456789ABCDEF"[n & 0xF] + retval;
        n >>= 4;
    }
    return retval;
}

namespace casioemu {
    class StartupUi {
    public:
        std::map<std::array<char, 8>, std::string> RomNames;
        struct Model {
        public:
            std::filesystem::path path;
            std::string name;
            std::string version;
            std::string type;
            std::string id;
            std::string checksum;
            std::string checksum2;
            std::string sum_good;
            bool realhw;
            bool show_sum = true;
        };
        std::vector<Model> models;
        std::filesystem::path selected_path{};

        StartupUi() {
            std::filesystem::create_directories("models");
            std::ifstream ifs2{"roms.db", std::ifstream::binary};
            if (ifs2)
                Binary::Read(ifs2, RomNames);
            for (auto &dir: std::filesystem::directory_iterator("models")) {
                if (dir.is_directory()) {
                    auto config = dir.path() / "config.bin";
                    std::ifstream ifs(config, std::ios::in | std::ios::binary);
                    if (!ifs)
                        continue;
                    ModelInfo mi{};
                    Binary::Read(ifs, mi);
                    ifs.close();
                    Model mod{};
                    mod.path = dir;
                    mod.name = mi.model_name;
                    mod.realhw = mi.real_hardware;
                    switch (mi.hardware_id) {
                        case HW_ES_PLUS:
                            mod.type = "ESP";
                            break;
                        case HW_CLASSWIZ:
                            mod.type = "CWX";
                            break;
                        case HW_CLASSWIZ_II:
                            mod.type = "CWII";
                            break;
                        case HW_FX_5800P:
                            mod.type = "Fx5800p";
                            break;
                        default:
                            mod.type = "Unknown";
                            break;
                    }
                    {
                        std::ifstream ifs2(dir.path() / mi.rom_path,
                                           std::ios::in | std::ios::binary);
                        if (!ifs2)
                            continue;
                        std::vector<byte> rom{std::istreambuf_iterator<char>{ifs2.rdbuf()},
                                              std::istreambuf_iterator<char>{}};
                        ifs2.close();
                        auto ri = rom_info(rom, mi.real_hardware);
                        if (ri.type != 0) {
                            switch (ri.type) {
                                case RomInfo::ES:
                                    mod.type = "ES";
                                    break;
                                case RomInfo::ESP:
                                    mod.type = "ESP";
                                    break;
                                case RomInfo::ESP2nd:
                                    mod.type = "ESP2nd";
                                    break;
                                case RomInfo::CWX:
                                    mod.type = "CWX";
                                    break;
                                case RomInfo::CWII:
                                    mod.type = "CWII";
                                    break;
                                case RomInfo::Fx5800p:
                                    mod.type = "Fx5800p";
                                    break;
                            }
                        }
                        if (ri.ok) {
                            mod.version = ri.ver;
                            std::array<char, 8> key{};
                            memcpy(key.data(), mod.version.data(), 6);
                            auto iter = RomNames.find(key);
                            if (iter != RomNames.end())
                                mod.name = iter->second;
                            mod.checksum = tohex(ri.real_sum, 4);
                            mod.checksum2 = tohex(ri.desired_sum, 4);
                            mod.sum_good = ri.real_sum == ri.desired_sum ? "OK" : "NG";
                            mod.id = tohex(*(unsigned long long *) ri.cid, 8);
                        } else {
                            mod.show_sum = false;
                        }
                    }
                    // SDL_Surface* loaded_surface = IMG_Load((dir.path() / mi.interface_path).string().c_str());
                    // mod.thumbnail_t = scale_texture_region_uniform(renderer2, loaded_surface, &mi.sprites["rsd_interface"].src, 100, 200);
                    models.push_back(mod);
                }
            }
        }

        std::vector<std::string> recently_used{};
        char search_txt[200]{};
        const char *current_filter = "##";
        bool not_show_emu = false;

        void Render() {
            auto io = ImGui::GetIO();

            ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y});
            ImGui::SetNextWindowPos({});
            ImGui::Begin(
#if LANGUAGE == 2
                    "启动"
#else
                    "Startup"
#endif
                    ,
                    0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
            ImGui::Text(
#if LANGUAGE == 2
                    "选择你的英雄:"
#else
                    "Choose a model:"
#endif
            );
            ImGui::Separator();
            ImGui::Text(
#if LANGUAGE == 2
                    "最近使用"
#else
                    "Recently used"
#endif
            );
            if (ImGui::BeginTable("Recently", 2, pretty_table)) {
                RenderHeaders();
                auto i = 114;
                for (auto &s: recently_used) {
                    auto iter = std::find_if(models.begin(), models.end(), [&](const Model &x) {
                        return x.path == s;
                    });
                    if (iter != models.end()) {
                        auto &model = *iter;
                        RenderModel(model, i);
                    }
                }
                ImGui::EndTable();
            }
            if (ImGui::CollapsingHeader(
#if LANGUAGE == 2
                    "全部"
#else
                    "All"
#endif
            )) {
                ImGui::SetNextItemWidth(200);
                ImGui::InputText(
#if LANGUAGE == 2
                        "搜索"
#else
                        "##search"
#endif
                        ,
                        search_txt, 300);
                ImGui::SameLine();
                const char *items[] = {"##", "ES", "ESP", "ESP2nd", "CWX", "CWII", "Fx5800p"};
                ImGui::SetNextItemWidth(240);
                if (ImGui::BeginCombo("##cb", current_filter)) {
                    for (int n = 0; n < IM_ARRAYSIZE(items); n++) {
                        bool is_selected = (current_filter ==
                                            items[n]); // You can store your selection however you want, outside or inside your objects
                        if (ImGui::Selectable(items[n], is_selected))
                            current_filter = items[n];
                        if (is_selected)
                            ImGui::SetItemDefaultFocus(); // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::Checkbox(
#if LANGUAGE == 2
                        "不要显示模拟器 Rom"
#else
                        "No emu roms"
#endif
                        ,
                        &not_show_emu);
                if (ImGui::BeginTable("All", 2, pretty_table)) {
                    RenderHeaders();
                    auto i = 114;
                    for (auto &model: models) {
                        bool matches_filter = (strcmp(current_filter, "##") == 0) ||
                                              (current_filter == model.type);
                        bool matches_search = (stristr(model.name.c_str(), search_txt) != nullptr ||
                                               stristr(model.version.c_str(), search_txt) !=
                                               nullptr);
                        if (matches_filter && matches_search && (not_show_emu ? model.realhw : 1)) {
                            RenderModel(model, i);
                        }
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::End();
        }

        void RenderHeaders() {
            ImGui::TableSetupColumn(
#if LANGUAGE == 2
                    "名称"
#else
                    "Name"
#endif
                    ,
                    ImGuiTableColumnFlags_WidthStretch, 100);
            ImGui::TableSetupColumn(
                    "",
                    ImGuiTableColumnFlags_WidthFixed, 240);
            ImGui::TableHeadersRow();
        }

        void RenderModel(const Model &model, int &i) {
            ImGui::TableNextRow();
            ImGui::PushID(i++);
            ImGui::TableNextColumn();
            ImGui::Text("%s", model.name.c_str());
            ImGui::TableNextColumn();
            if (ImGui::Button(
#if LANGUAGE == 2
                    "原O,启动!"
#else
                    "Launch"
#endif
            )) {
                selected_path = model.path;
                auto iter = std::find_if(recently_used.begin(), recently_used.end(),
                                         [&](auto &x) {
                                             return x == model.path.string();
                                         });
                if (iter != recently_used.end())
                    recently_used.erase(iter);
                recently_used.insert(recently_used.begin(), model.path.string());
                if (recently_used.size() > 5) {
                    recently_used.resize(5);
                }
            }
            ImGui::PopID();
        }
    };
} // namespace casioemu
inline const ImWchar *GetKanji() {
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

std::array<byte, 64> sign_dat{};

std::string sui_loop() {
    casioemu::StartupUi ui;
    {
        std::ifstream ifs1{"recent.bin", std::ifstream::binary};
        if (ifs1)
            Binary::Read(ifs1, ui.recently_used);
    }
    {
        std::ifstream ifs2{"sign.bin", std::ifstream::binary};
        if (ifs2)
            Binary::Read(ifs2, sign_dat);
    }
    window = SDL_CreateWindow(
            "CasioEmuMsvc",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            800, 800,
            SDL_WINDOW_SHOWN | (SDL_WINDOW_RESIZABLE));
    renderer2 = SDL_CreateRenderer(window, -1,
                                   SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    if (renderer2 == nullptr) {
        SDL_Log("Error creating SDL_Renderer!");
        return "";
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = 3;
    ImFontConfig config{};
    //config.OversampleH = 4;
    //config.OversampleV = 4;
    //config.PixelSnapH = true;
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

    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    style.FrameRounding = 8.0f;
    style.ScrollbarSize = 40.f;
    style.FramePadding = {8.f, 8.f};
    style.ItemInnerSpacing = {20.0f, 20.0f};
    style.ItemSpacing = {20.0f, 20.0f};

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer2);
    ImGui_ImplSDLRenderer2_Init(renderer2);
    bool v_close = true;
    while (1) {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ui.Render();
        for (auto &wind: windows2) {
            wind->Render();
        }
        ImGui::EndFrame();
        ImGui::Render();
        SDL_RenderSetScale(renderer2, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer2, 0, 0, 0, 255);
        SDL_RenderClear(renderer2);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer2);
        SDL_Event event;
        if (!SDL_PollEvent(&event))
            continue;
        switch (event.type) {

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        goto exit;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:
                ImGui_ImplSDL2_ProcessEvent(&event);
                break;
        }
        if (!ui.selected_path.empty())
            goto exit;
    }
    exit:
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    for (auto &wind: windows2) {
        delete wind;
    }
    SDL_DestroyRenderer(renderer2);
    SDL_DestroyWindow(window);
    std::ofstream ofs{"recent.bin", std::ofstream::binary};
    if (ofs)
        Binary::Write(ofs, ui.recently_used);
    else {
        std::cout << "[Warn] Cannot write to recent.bin.\n";
    }
    return ui.selected_path.string();
}