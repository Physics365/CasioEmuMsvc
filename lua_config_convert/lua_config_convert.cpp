// lua_config_convert.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <iomanip>
#include "a.h"
#include "../CasioEmuMsvc/ModelInfo.h" // 隔壁老王.png
lua_State* ls;
lua_Integer lsr;

#undef main
using casioemu::ModelInfo;
using casioemu::ColourInfo;
using casioemu::SpriteInfo;
using Button = casioemu::ButtonInfo;

int main(int argc, char* argv[]) {
	// 检查是否提供了命令行参数
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <model.lua path> <config.bin path>" << std::endl;
		return 1;
	}
	ModelInfo mi{};
	ls = luaL_newstate();
	luaL_openlibs(ls);

	lua_newuserdata(ls, 0);
	lua_newtable(ls);
	lua_newtable(ls);
	lsr = LUA_REFNIL;
	lua_pushcfunction(ls, [](lua_State* lua_state) {
		switch (lua_gettop(lua_state))
		{
		case 1:
			// emu:model() returns the model table
			lua_geti(lua_state, LUA_REGISTRYINDEX, lsr);
			return 1;

		case 2:
			// emu:model(t) sets the model table
			if (lsr != LUA_REFNIL)
				PANIC("emu.model invoked twice\n");
			lsr = luaL_ref(lua_state, LUA_REGISTRYINDEX);
			return 0;

		default:
			PANIC("Invalid number of arguments (%d)\n", lua_gettop(lua_state));
		}
		});
	lua_setfield(ls, -2, "model");

	lua_setfield(ls, -2, "__index");
	lua_pushcfunction(ls, [](lua_State*) {
		return 0;
		});
	lua_setfield(ls, -2, "__newindex");
	lua_setmetatable(ls, -2);
	lua_setglobal(ls, "emu");

	if (luaL_loadfile(ls, argv[1]) != LUA_OK)
		PANIC("LoadModelDefition failed: %s\n", lua_tostring(ls, -1));

	if (lua_pcall(ls, 0, 0, 0) != LUA_OK)
		PANIC("LoadModelDefition failed: %s\n", lua_tostring(ls, -1));

	if (lsr == LUA_REFNIL)
		PANIC("LoadModelDefition failed: model failed to call emu.model\n");

	lua_geti(ls, LUA_REGISTRYINDEX, lsr);
	lua_pushnil(ls);
	while (lua_next(ls, -2)) {
		std::string key = lua_tostring(ls, -2);
		std::cout << key << ": ";
		if (lua_type(ls, -1) == LUA_TSTRING) {
			auto value = lua_tostring(ls, -1);
			if (key == "model_name") {
				mi.model_name = value;
			}
			else if (key == "interface_image_path") {
				mi.interface_path = value;
			}
			else if (key == "rom_path") {
				mi.rom_path = value;
			}
			else if (key == "flash_path") {
				mi.flash_path = value;
			}
			std::cout << value << "\n";
		}
		else if (lua_type(ls, -1) == LUA_TNUMBER) {
			auto value = lua_tointeger(ls, -1);
			if (key == "hardware_id") {
				mi.hardware_id = static_cast<unsigned char>(value);
			}
			else if (key == "real_hardware") {
				mi.real_hardware = static_cast<unsigned char>(!!value);
			}
			else if (key == "csr_mask") {
				mi.csr_mask = static_cast<unsigned short>(value);
			}
			else if (key == "pd_value") {
				mi.pd_value = static_cast<unsigned char>(value);
			}
			std::cout << std::oct << value << "\n";
		}
		else if (lua_type(ls, -1) == LUA_TTABLE) {
			if (key == "button_map") {
				std::cout << " ↓" << "\n";
				lua_pushnil(ls);
				while (lua_next(ls, -2)) {
					for (int ix = 0; ix != 6; ++ix)
						if (lua_geti(ls, -1 - ix, ix + 1) == LUA_TNIL)
							PANIC("key '%s'[%i] is not present\n", key.c_str(), ix + 1);

					Button btn;
					btn.rect.x = static_cast<int>(lua_tointeger(ls, -6));
					btn.rect.y = static_cast<int>(lua_tointeger(ls, -5));
					btn.rect.w = static_cast<int>(lua_tointeger(ls, -4));
					btn.rect.h = static_cast<int>(lua_tointeger(ls, -3));
					btn.kiko = static_cast<int>(lua_tointeger(ls, -2));
					btn.keyname = lua_tostring(ls, -1);
					mi.buttons.push_back(btn);
					std::cout << " {" << btn.rect.x
						<< ", " << btn.rect.y
						<< ", " << btn.rect.w
						<< ", " << btn.rect.h
						<< ", " << btn.kiko
						<< ", \"" << btn.keyname << "\""
						<< "}\n";
					lua_pop(ls, 7);
				}
				//lua_pop(ls, 1);
			}
			else {
				auto len = lua_rawlen(ls, -1);
				if (len == 3) {
					for (int ix = 0; ix != 3; ++ix)
						if (lua_geti(ls, -1 - ix, ix + 1) != LUA_TNUMBER)
							PANIC("key '%s'[%i] is not a number\n", key.c_str(), ix + 1);

					ColourInfo colour_info;
					colour_info.r = static_cast<int>(lua_tointeger(ls, -3));
					colour_info.g = static_cast<int>(lua_tointeger(ls, -2));
					colour_info.b = static_cast<int>(lua_tointeger(ls, -1));
					if (key == "ink_colour") {
						mi.ink_color = colour_info;
					}
					lua_pop(ls, 3);
					std::cout << "#" << std::hex
						<< std::setw(2) << std::setfill('0') << colour_info.r
						<< std::setw(2) << std::setfill('0') << colour_info.g
						<< std::setw(2) << std::setfill('0') << colour_info.b
						<< "\n";
				}
				else if (len == 6) {
					for (int ix = 0; ix != 6; ++ix)
						if (lua_geti(ls, -1 - ix, ix + 1) != LUA_TNUMBER)
							PANIC("key '%s'[%i] is not a number\n", key.c_str(), ix + 1);

					SpriteInfo sprite_info;
					sprite_info.src.x = static_cast<int>(lua_tointeger(ls, -6));
					sprite_info.src.y = static_cast<int>(lua_tointeger(ls, -5));
					sprite_info.src.w = static_cast<int>(lua_tointeger(ls, -4));
					sprite_info.src.h = static_cast<int>(lua_tointeger(ls, -3));
					sprite_info.dest.x = static_cast<int>(lua_tointeger(ls, -2));
					sprite_info.dest.y = static_cast<int>(lua_tointeger(ls, -1));
					sprite_info.dest.w = sprite_info.src.w;
					sprite_info.dest.h = sprite_info.src.h;

					mi.sprites[key] = sprite_info;

					lua_pop(ls, 6);
					std::cout << " {" << sprite_info.src.x
						<< ", " << sprite_info.src.y
						<< ", " << sprite_info.src.w
						<< ", " << sprite_info.src.h
						<< ", " << sprite_info.dest.x
						<< ", " << sprite_info.dest.y
						<< ", " << sprite_info.dest.w
						<< ", " << sprite_info.dest.h
						<< "}\n";
				}
				else {
					std::cout << " shiranai" << "\n";
				}
			}
		}
		else {
			std::cout << " shiranai" << "\n";
		}
		lua_pop(ls, 1);
	}
	lua_pop(ls, 1);
	std::ofstream ofs(argv[2], std::ios::binary | std::ios::out);
	mi.Write(ofs);
	ofs.close();
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
