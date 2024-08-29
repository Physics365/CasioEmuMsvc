#include "Config.hpp"
#include "Ui.hpp"
#include "imgui_impl_sdl2.h"

#include <SDL.h>
#include <SDL_image.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include "Emulator.hpp"
#include "Logger.hpp"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_video.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#if _WIN32
#include <Windows.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "StartupUi/StartupUi.h"

using namespace casioemu;

int main(int argc, char* argv[]) {
#ifdef _WIN32
	timeBeginPeriod(1);
	SetConsoleCP(65001); // Set to UTF8
	SetConsoleOutputCP(65001);
#endif //  _WIN32

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

    auto s = sui_loop();
    argv_map["model"] = s;
    if (s.empty())
        return -1;

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
	test_gui(&guiCreated);
#endif
	while (emulator.Running()) {
		SDL_Event event{};
		busy = false;
		if (!SDL_PollEvent(&event))
			continue;
		busy = true;
		if (event.type == frame_event) {
#ifdef DBG
			gui_loop();
#endif
			emulator.Frame();
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
                if (!ImGui::GetIO().WantCaptureMouse)
                    emulator.UIEvent(event);
                break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_TEXTINPUT:
		case SDL_MOUSEMOTION:
		case SDL_MOUSEWHEEL:
		default:
#ifdef DBG
			if ((SDL_GetKeyboardFocus() != emulator.window) && guiCreated) {
				ImGui_ImplSDL2_ProcessEvent(&event);
				continue;
			}
#endif
			emulator.UIEvent(event);
			break;
		}
	}
	return 0;
}

