#include "Config.hpp"
#include "Ui.hpp"
#include "imgui_impl_sdl2.h"

#include "Emulator.hpp"
#include "Logger.hpp"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_video.h"
#include <SDL.h>
#include <SDL_image.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>

#if _WIN32
#include <Windows.h>
#pragma comment(lib, "winmm.lib")
#endif

#ifdef __ANDROID__
#include <unistd.h>
#endif

#include "StartupUi/StartupUi.h"

using namespace casioemu;

int main(int argc, char* argv[]) {
#ifdef _WIN32
	timeBeginPeriod(1);
	SetConsoleCP(65001); // Set to UTF8
	SetConsoleOutputCP(65001);
#endif //  _WIN32
#ifdef __ANDROID__
	chdir(SDL_AndroidGetExternalStoragePath());
#endif

	std::map<std::string, std::string> argv_map;
	for (int ix = 1; ix != argc; ++ix) {
		std::string key, value;
		char* eq_pos = strchr(argv[ix], '=');
		if (eq_pos) {
			key = std::string(argv[ix], eq_pos);
			value = eq_pos + 1;
		}
		else {
			key = "model";
			value = argv[ix];
		}

		if (argv_map.find(key) == argv_map.end())
			argv_map[key] = value;
		else
			logger::Info("[argv][Info] #%i: key '%s' already set\n", ix, key.c_str());
	}
	bool headless = argv_map.find("headless") != argv_map.end();

	int sdlFlags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
	if (SDL_Init(sdlFlags) != 0)
		PANIC("SDL_Init failed: %s\n", SDL_GetError());

	int imgFlags = IMG_INIT_PNG;
	if (IMG_Init(imgFlags) != imgFlags)
		PANIC("IMG_Init failed: %s\n", IMG_GetError());

	if (argv_map["model"].empty()) {
		auto s = sui_loop();
		argv_map["model"] = std::move(s);
		if (argv_map["model"].empty())
			return -1;
	}

	Emulator emulator(argv_map);
	m_emu = &emulator;

	// static std::atomic<bool> running(true);

	bool guiCreated = false;
	auto frame_event = SDL_RegisterEvents(1);
	bool busy = false;
	std::thread t3([&]() {
		SDL_Event se{};
		se.type = frame_event;
		se.user.windowID = SDL_GetWindowID(emulator.window);
		while (1) {
			if (!busy)
				SDL_PushEvent(&se);
			SDL_Delay(24);
		}
	});
	t3.detach();
#ifdef DBG
	test_gui(&guiCreated, emulator.window, emulator.renderer);
#endif
	SDL_Surface* background = IMG_Load("background.jpg");
	SDL_Texture* bg_txt = 0;
	if (background) {
		bg_txt = SDL_CreateTextureFromSurface(renderer, background);
	}

	SDL_ShowWindow(emulator.window);

	while (emulator.Running()) {
		SDL_Event event{};
		busy = false;
		if (!SDL_PollEvent(&event))
			continue;
		busy = true;
		if (event.type == frame_event) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			if (bg_txt) {
				int w, h;
				SDL_GetWindowSize(window, &w, &h);
				int bg_w, bg_h;
				SDL_QueryTexture(bg_txt, NULL, NULL, &bg_w, &bg_h);

				float window_aspect = (float)w / h;
				float bg_aspect = (float)bg_w / bg_h;

				SDL_Rect dst_rect;
				if (window_aspect > bg_aspect) {
					dst_rect.w = w;
					dst_rect.h = (int)(w / bg_aspect);
					dst_rect.x = 0;
					dst_rect.y = (h - dst_rect.h) / 2;
				}
				else {
					dst_rect.h = h;
					dst_rect.w = (int)(h * bg_aspect);
					dst_rect.x = (w - dst_rect.w) / 2;
					dst_rect.y = 0;
				}

				SDL_RenderCopy(renderer, bg_txt, NULL, &dst_rect);
			}
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 20);
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			SDL_RenderFillRect(renderer, 0);
#ifdef SINGLE_WINDOW
			emulator.Frame();
			gui_loop();
			SDL_RenderPresent(emulator.renderer);
#else
			gui_loop();
			emulator.Frame();
			SDL_RenderPresent(emulator.renderer);
#endif

			while (SDL_PollEvent(&event)) {
				if (event.type != frame_event)
					goto hld;
			}
			continue;
		}
	hld:
		switch (event.type) {
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_CLOSE:
				emulator.Shutdown();
				std::exit(0);
				break;
			case SDL_WINDOWEVENT_RESIZED:
				break;
			}
			break;
		case SDL_FINGERUP:
		case SDL_FINGERDOWN:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_TEXTINPUT:
		case SDL_MOUSEMOTION:
		case SDL_MOUSEWHEEL:
		default:
#ifdef SINGLE_WINDOW
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (ImGui::GetIO().WantCaptureMouse) {
				break;
			}
#else
			if ((SDL_GetKeyboardFocus() != emulator.window) && guiCreated) {
				ImGui_ImplSDL2_ProcessEvent(&event);
				break;
			}
#endif
			emulator.UIEvent(event);
			break;
		}
	}
	return 0;
};