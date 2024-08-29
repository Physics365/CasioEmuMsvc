#include "Config.hpp"
#include "Gui/imgui/imgui_impl_sdl2.h"
#include "Gui/ui.hpp"
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

#include "StartupUi/StartupUi.h"

using namespace casioemu;

int SDL_main(int argc, char* argv[]) {
    chdir(SDL_AndroidGetExternalStoragePath());
    std::map<std::string,std::string> argv_map;
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

	static std::atomic<bool> running(true);

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
	test_gui(&guiCreated,emulator.window,emulator.renderer);
	while (emulator.Running()) {
		SDL_Event event{};
		busy = false;
		if (!SDL_PollEvent(&event))
			continue;
		busy = true;
		if (event.type == frame_event) {
            SDL_SetRenderDrawColor(emulator.renderer, 0,0,0,255);
            SDL_RenderClear(emulator.renderer);
            emulator.Frame();
            gui_loop();
            SDL_RenderPresent(emulator.renderer);
			continue;
		}
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
            ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		}
	}
	return 0;
}

