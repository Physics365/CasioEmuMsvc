#pragma once
#include <SDL.h>
#include <map>
#include <string>
#include <vector>

#include "Binary.h"
namespace casioemu {
	enum HardwareId {
		HW_MIN = 3,
		HW_ES_PLUS = 3,
		HW_CLASSWIZ = 4,
		HW_CLASSWIZ_II = 5,
		HW_FX_5800P = 6,
		HW_MAX = 6,
	};
	struct SpriteInfo {
		SDL_Rect src, dest;
	};
	struct ColourInfo {
		int r, g, b;
	};
	struct ButtonInfo {
		SDL_Rect rect{};
		int kiko{};
		std::string keyname;
		void Write(std::ostream& stm) const {
			Binary::Write(stm, rect);
			Binary::Write(stm, kiko);
			Binary::Write(stm, keyname);
		}
		void Read(std::istream& stm) {
			Binary::Read(stm, rect);
			Binary::Read(stm, kiko);
			Binary::Read(stm, keyname);
		}
	};
	struct ModelInfo {
		std::vector<ButtonInfo> buttons;
		std::map<std::string, SpriteInfo> sprites;
		ColourInfo ink_color{};
		std::string interface_path;
		std::string model_name;
		std::string rom_path;
		std::string flash_path;
		unsigned short csr_mask{};
		unsigned short hardware_id{};
		bool real_hardware{};
		unsigned char pd_value{};
		bool enable_new_screen{};
		bool is_sample_rom{};
		bool legacy_ko{};
		void Write(std::ostream& os) const {
			Binary::Write(os, std::string("\n\nGenshin Configuration file v50\n\n原神配置文件v50\n\n"));
			Binary::Write(os, csr_mask);
			Binary::Write(os, hardware_id);
			Binary::Write(os, real_hardware);
			Binary::Write(os, pd_value);
			Binary::Write(os, buttons);
			Binary::Write(os, sprites);
			Binary::Write(os, ink_color);
			Binary::Write(os, interface_path);
			Binary::Write(os, model_name);
			Binary::Write(os, rom_path);
			Binary::Write(os, flash_path);
			Binary::Write(os, enable_new_screen);
			Binary::Write(os, is_sample_rom);
			Binary::Write(os, legacy_ko);
		}
		void Read(std::istream& is) {
			{
				std::string unused;
				Binary::Read(is, unused);
			}
			Binary::Read(is, csr_mask);
			Binary::Read(is, hardware_id);
			Binary::Read(is, real_hardware);
			Binary::Read(is, pd_value);
			Binary::Read(is, buttons);
			Binary::Read(is, sprites);
			Binary::Read(is, ink_color);
			Binary::Read(is, interface_path);
			Binary::Read(is, model_name);
			Binary::Read(is, rom_path);
			Binary::Read(is, flash_path);
			Binary::Read(is, enable_new_screen);
			Binary::Read(is, is_sample_rom);
			Binary::Read(is, legacy_ko);
		}
	};
} // namespace casioemu