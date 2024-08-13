#include "Screen.hpp"
#include "Chipset/Chipset.hpp"
#include "Chipset/MMU.hpp"
#include "Chipset/MMURegion.hpp"
#include "Emulator.hpp"
#include "Gui/HwController.h"
#include "Logger.hpp"
#include "ModelInfo.h"
#include "Models.h"
#include "Ui.hpp"
#include <algorithm> // for std::generate
#include <array>
#include <cstdlib> // for std::rand
#include <ctime>   // for std::time
#include <vector>

constexpr uint8_t reverse_bits(uint8_t n) {
	uint8_t reversed = 0;
	for (int i = 0; i < 8; ++i) {
		reversed |= ((n >> i) & 1) << (7 - i);
	}
	return reversed;
}

// constexpr 生成查找表
constexpr std::array<uint8_t, 256> generate_lookup_table() {
	std::array<uint8_t, 256> table = {};
	for (int i = 0; i < 256; ++i) {
		table[i] = reverse_bits(static_cast<uint8_t>(i));
	}
	return table;
}

// 定义查找表
constexpr auto bit_lookup_table = generate_lookup_table();

void fillRandomData(unsigned char* buf, size_t size) {
	std::srand(static_cast<unsigned int>(SDL_GetPerformanceCounter())); // 使用当前时间作为随机种子
	std::generate(buf, buf + size, []() {
		return static_cast<unsigned char>(std::rand() % 256); // 生成0到255之间的随机数
	});
}

namespace casioemu {
	struct SpriteBitmap {
		const char* name;
		uint8_t mask, offset;
	};
	inline void update_screen_scan_alpha(float* screen_scan_alpha, float t, float screen_refresh_rate) {
		// auto t_mod = fmodf(t,screen_refresh_rate);
		if (screen_refresh_rate < screen_flashing_threshold) { // thd
			for (size_t i = 0; i < 64; i++) {
				screen_scan_alpha[i] = 1;
			}
			return;
		}
		float position = fmodf(t * pow(screen_refresh_rate, -0.6) * 5, 64);
		for (size_t i = 0; i < 64; i++) {
			screen_scan_alpha[(i + int(floor(position))) % 64] = screen_flashing_brightness_coeff - i / 64. * screen_flashing_brightness_coeff;
		}
	}
	template <HardwareId hardware_id>
	class Screen : public Peripheral {
		static int const N_ROW,
			ROW_SIZE,
			OFFSET,
			ROW_SIZE_DISP,
			SPR_MAX;

		MMURegion region_buffer{}, region_buffer1{}, region_contrast{}, region_brightness{}, region_contrast2{}, region_mode{}, region_range{}, region_select{}, region_offset{}, region_refresh_rate{};
		uint8_t *screen_buffer{}, *screen_buffer1{}, screen_contrast{}, screen_brightness{}, screen_contrast2{}, screen_mode{}, screen_range{}, screen_select{}, screen_offset{}, screen_refresh_rate{};

		MMURegion region_power{}, region_contrast2_en{};
		uint8_t screen_power{}, screen_contrast2_en{};

		MMURegion region_unk1{}, region_unk2{};

		uint8_t unk_f034{};

		MMURegion ti_port7_data{}, ti_port5_data{};
		int ti_contrast{}, ti_port_status{};
		bool ti_enabled = 0;

		float screen_scan_alpha[64]{};
		float position = 0;
		SDL_Renderer* renderer{};
		SDL_Texture* interface_texture{};
		float screen_ink_alpha[66 * 192]{};
		static const SpriteBitmap sprite_bitmap[];
		std::vector<SpriteInfo> sprite_info;
		ColourInfo ink_colour{};

		bool inited = 0;
		bool enabled_2 = 0;

	public:
		Screen(Emulator& emu)
			: Peripheral(emu) {
			std::thread thd([&]() {
				while (1) {
					tick();
				}
			});
			thd.detach();
		}
		~Screen() {
			if (screen_buffer)
				delete[] screen_buffer;
			if (screen_buffer1)
				delete[] screen_buffer1;
		}
		void Initialise() override;
		void Uninitialise() override;
		void Frame() override;
		void Reset() override;

		void tick() {
			float ratio = 0;
			if constexpr (hardware_id == HW_ES_PLUS)
				ratio = 1 - 1e-4;
			else
				ratio = 1 - 5e-4;

			if constexpr (hardware_id == HW_TI) {
				ratio = 1 - 1e-4;
				if (!ti_enabled) {
					for (size_t i = 0; i < 65 * 192; i++) {
						screen_ink_alpha[i] *= ratio;
					}
					return;
				}
				if (!n_ram_buffer || !emulator.chipset.ti_screen_buf || !emulator.chipset.ti_status_buf)
					return;
				float ink_alpha_on = (ti_contrast - 100) * 20.0;
				float ink_alpha_off = std::clamp(ink_alpha_on * 0.1, 0.0, 255.0);
				ink_alpha_on = std::clamp(ink_alpha_on, 0.0f, 255.0f);
				auto screen_buffer = (uint8_t*)n_ram_buffer - casioemu::GetRamBaseAddr(hardware_id) + emulator.chipset.ti_screen_buf;
				for (int ix = 0; ix < 192; ++ix) {
					for (int iy = 0; iy < 64; ++iy) {
						uint32_t i = (ix << 6) | iy;
						int bIndx = (i >> 3);
						int subIndx = (i & 7);
						int mask = (1 << subIndx);
						bool on = (screen_buffer[bIndx] & mask) != 0;
						auto& data = screen_ink_alpha[(iy * 192 + 192) + ix];
						data = data * ratio + (on ? ink_alpha_on : ink_alpha_off) * (1 - ratio);
					}
				}
				screen_buffer = (uint8_t*)n_ram_buffer - casioemu::GetRamBaseAddr(hardware_id) + emulator.chipset.ti_status_buf;
				int x = 0;
				for (int ix = 1; ix != SPR_MAX; ++ix) {
					auto off = sprite_bitmap[ix].offset;
					auto& data = screen_ink_alpha[x];
					data = data * ratio + ((screen_buffer[off] & sprite_bitmap[ix].mask) ? ink_alpha_on : ink_alpha_off) * (1 - ratio);
					x++;
				}

				return;
			}

			if (screen_refresh_rate < screen_flashing_threshold && !enable_screen_fading)
				0;
			else {
				update_screen_scan_alpha(screen_scan_alpha, SDL_GetTicks64(), screen_refresh_rate);
			}
			if (screen_refresh_rate < 6) {
				screen_refresh_rate = 6;
			}
			auto sb = screen_brightness;
			if (sb < 3) {
				sb = 3;
			}
			auto contrast = (int)screen_contrast - 11;
			if (screen_contrast2_en) {
				contrast += screen_contrast2 * 0.5;
			}
			if (contrast < 0) {
				contrast = 0;
			}
			auto coeff = 16;
			auto off = 0;
			if constexpr (hardware_id != HW_CLASSWIZ_II) {
				coeff = 28;
				off = -240;
			}
			int ink_alpha_on = off + contrast * coeff - sb * 8;
			int ink_alpha_off = off + 20 + (contrast) * (coeff - 11) - sb * 13;
			if (ink_alpha_on < 0)
				ink_alpha_on = 0;
			if (ink_alpha_off < 0)
				ink_alpha_off = 0;
			bool enable_status, enable_dotmatrix, clear_dots;

			bool mode_6 = false;

			auto screen_buffer = this->screen_buffer;
			uint8_t* screen_buffer1;
			size_t row_size = ROW_SIZE;
			if constexpr (hardware_id == HW_CLASSWIZ_II) {
				screen_buffer1 = this->screen_buffer1;
			}
			if (screen_buffer_select != 0) {
				screen_buffer = (uint8_t*)n_ram_buffer - casioemu::GetRamBaseAddr(hardware_id) + casioemu::GetScreenBufferOffset(emulator.hardware_id, screen_buffer_select);
				if (hardware_id == HW_CLASSWIZ_II) {
					screen_buffer1 = screen_buffer + 0x600;
				}
				row_size = ROW_SIZE_DISP;
			}

			if (!enabled_2)
				goto clean_scr;

			switch (screen_mode & 7) {
			case 4: // 100
				enable_dotmatrix = true;
				clear_dots = true;
				enable_status = false;
				break;

			case 5: // 101
				enable_dotmatrix = true;
				clear_dots = false;
				enable_status = true;
				break;

			case 6: // 110
				enable_dotmatrix = true;
				clear_dots = true;
				enable_status = true;
				mode_6 = true;
				break;

			default:
				goto clean_scr;
			}
			if (screen_range & 0b100000)
				goto clean_scr;
			{
				bool flip_screen_h = screen_mode & 0b1000;
				bool flip_screen_v = !(screen_mode & 0b10000);
				if constexpr (hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) {
				}
				else {
					flip_screen_v = flip_screen_v = 0;
				}
				int rng1 = (4 - (screen_range & 0x3));
				ink_alpha_off *= (4 / rng1);
				ink_alpha_on *= (4 / rng1);
				int rng = rng1 * 8;

				if (enable_status) {
					int ink_alpha = ink_alpha_off;
					if constexpr (hardware_id == HW_CLASSWIZ_II) {
						int x = 0;
						for (int ix = 1; ix != SPR_MAX; ++ix) {
							ink_alpha = ink_alpha_off;
							auto off = (sprite_bitmap[ix].offset + screen_offset * row_size) % ((N_ROW + 1) * row_size);
							if (screen_buffer[off] & sprite_bitmap[ix].mask)
								ink_alpha += (ink_alpha_on - ink_alpha_off) * 0.2;
							if (screen_buffer1[off] & sprite_bitmap[ix].mask)
								ink_alpha += (ink_alpha_on - ink_alpha_off) * 0.8;
							if (screen_refresh_rate >= screen_flashing_threshold)
								ink_alpha *= screen_scan_alpha[0];
							screen_ink_alpha[x] = screen_ink_alpha[x] * ratio + ink_alpha * (1 - ratio);
							x++;
						}
					}
					else {
						int x = 0;
						for (int ix = 1; ix != SPR_MAX; ++ix) {
							auto off = (sprite_bitmap[ix].offset + screen_offset * row_size) % ((N_ROW + 1) * row_size);
							if (screen_buffer[off] & sprite_bitmap[ix].mask)
								ink_alpha = ink_alpha_on;
							else
								ink_alpha = ink_alpha_off;
							if (screen_refresh_rate >= screen_flashing_threshold)
								ink_alpha *= screen_scan_alpha[0];
							screen_ink_alpha[x] = screen_ink_alpha[x] * ratio + ink_alpha * (1 - ratio);
							x++;
						}
					}
				}
				else {
					if constexpr (hardware_id == HW_CLASSWIZ_II) {
						for (size_t i = 0; i < 192; i++) {
							screen_ink_alpha[i] *= ratio;
						}
					}
					else {
						for (size_t i = 0; i < 192; i++) {
							screen_ink_alpha[i] *= ratio;
						}
					}
				}

				if (enable_dotmatrix) {
					static constexpr auto SPR_PIXEL = 0;
					SDL_Rect dest = Screen<hardware_id>::sprite_info[SPR_PIXEL].dest;
					int ink_alpha = ink_alpha_off;
					if (mode_6) {
						ink_alpha_on = ink_alpha_off /= 2.55;
					}
					if constexpr (hardware_id == HW_CLASSWIZ_II) {
						for (int iy2 = 1; iy2 != (N_ROW + 1); ++iy2) {
							int iy = (iy2 + screen_offset) % (N_ROW + 1);
							bool clear = 0;
							if (iy2 >= rng && iy2 < 32)
								clear = 1;
							if (iy2 >= 32)
								if (iy2 <= 32 + rng) {
									iy = (iy2 - 32 + rng + screen_offset) % (N_ROW + 1);
								}
								else
									clear = 1;
							dest.x = sprite_info[SPR_PIXEL].dest.x;
							dest.y = sprite_info[SPR_PIXEL].dest.y + (iy2 - 1) * sprite_info[SPR_PIXEL].src.h;
							int x = 0;
							for (int ix = 0; ix != ROW_SIZE_DISP; ++ix) {
								auto index = (flip_screen_v ? N_ROW - iy : iy) * row_size + ix;
								for (uint8_t mask = 0x80; mask; mask >>= 1, dest.x += sprite_info[SPR_PIXEL].src.w) {
									ink_alpha = ink_alpha_off;
									if (!clear_dots && screen_buffer[index] & mask)
										ink_alpha += (ink_alpha_on - ink_alpha_off) * 0.2;
									if (!clear_dots && screen_buffer1[index] & mask)
										ink_alpha += (ink_alpha_on - ink_alpha_off) * 0.8;
									if (screen_refresh_rate >= screen_flashing_threshold)
										ink_alpha *= screen_scan_alpha[iy];
									if (clear)
										ink_alpha = 0;
									float& dat = screen_ink_alpha[(flip_screen_h ? (191 - x) : x) + iy2 * 192];
									dat = dat * ratio + ink_alpha * (1 - ratio);
									x++;
								}
							}
						}
					}
					else {
						for (int iy2 = 1; iy2 != (N_ROW + 1); ++iy2) {
							int iy = (iy2 + screen_offset) % (N_ROW + 1);
							bool clear = 0;
							if (iy2 >= rng && iy2 < 32)
								clear = 1;
							if (iy2 >= 32)
								if (iy2 <= 32 + rng) {
									iy = (iy2 - 32 + rng + screen_offset) % (N_ROW + 1);
								}
								else
									clear = 1;
							dest.x = sprite_info[SPR_PIXEL].dest.x;
							dest.y = sprite_info[SPR_PIXEL].dest.y + (iy2 - 1) * sprite_info[SPR_PIXEL].src.h;
							int x = 0;
							for (int ix = 0; ix != ROW_SIZE_DISP; ++ix) {
								auto index = (flip_screen_v ? N_ROW + 1 - iy : iy) * row_size + ix;
								for (uint8_t mask = 0x80; mask; mask >>= 1, dest.x += sprite_info[SPR_PIXEL].src.w) {
									if (screen_buffer[index] & mask)
										ink_alpha = ink_alpha_on;
									else
										ink_alpha = ink_alpha_off;
									if (screen_refresh_rate >= screen_flashing_threshold)
										ink_alpha *= screen_scan_alpha[iy];
									if (clear)
										ink_alpha = 0;
									float& dat = screen_ink_alpha[(flip_screen_h ? (191 - x) : x) + iy2 * 192];
									dat = dat * ratio + ink_alpha * (1 - ratio);
									x++;
								}
							}
						}
					}
				}
				else {
					if constexpr (hardware_id == HW_CLASSWIZ_II) {
						for (size_t i = 192; i < 64 * 192; i++) {
							screen_ink_alpha[i] *= ratio;
						}
					}
					else {
						for (size_t i = 192; i < 64 * 192; i++) {
							screen_ink_alpha[i] *= ratio;
						}
					}
				}
			}
			return;
		clean_scr:
			if constexpr (hardware_id == HW_CLASSWIZ_II) {
				for (size_t i = 0; i < 64 * 192; i++) {
					screen_ink_alpha[i] *= ratio;
				}
			}
			else {
				for (size_t i = 0; i < 64 * 192; i++) {
					screen_ink_alpha[i] *= ratio;
				}
			}
			return;
		}
	};

	template <>
	const int Screen<HW_TI>::N_ROW = 64;
	template <>
	const int Screen<HW_TI>::ROW_SIZE = 32;
	template <>
	const int Screen<HW_TI>::OFFSET = 32;
	template <>
	const int Screen<HW_TI>::ROW_SIZE_DISP = 24;
	template <>
	const int Screen<HW_TI>::SPR_MAX = 2;
	template <>
	const SpriteBitmap Screen<HW_TI>::sprite_bitmap[] = {
		{"rsd_pixel", 0, 0},
		{"rsd_2nd", 0x02, 0x00},
		//{"rsd_fix", 0x04, 0x00},
		//{"rsd_hbo", 0x08, 0x00},
		//{"rsd_sci", 0x02, 0x01},
		//{"rsd_eng", 0x01, 0x01},
		//{"rsd_deg", 0x04, 0x01},
		//{"rsd_rad", 0x08, 0x01},
		//{"rsd_bat", 0x02, 0x02},
		//{"rsd_wait", 0x04, 0x02},
		//{"rsd_left", 0x08, 0x02},
		//{"rsd_up", 0x10, 0x02},
		//{"rsd_down", 0x20, 0x02},
		//{"rsd_right", 0x40, 0x02},
	};

	template <>
	const int Screen<HW_CLASSWIZ_II>::N_ROW = 63;
	template <>
	const int Screen<HW_CLASSWIZ_II>::ROW_SIZE = 32;
	template <>
	const int Screen<HW_CLASSWIZ_II>::OFFSET = 32;
	template <>
	const int Screen<HW_CLASSWIZ_II>::ROW_SIZE_DISP = 24;
	template <>
	const int Screen<HW_CLASSWIZ_II>::SPR_MAX = 19;

	template <>
	const int Screen<HW_CLASSWIZ>::N_ROW = 63;
	template <>
	const int Screen<HW_CLASSWIZ>::ROW_SIZE = 32;
	template <>
	const int Screen<HW_CLASSWIZ>::OFFSET = 32;
	template <>
	const int Screen<HW_CLASSWIZ>::ROW_SIZE_DISP = 24;
	template <>
	const int Screen<HW_CLASSWIZ>::SPR_MAX = 21;

	template <>
	const int Screen<HW_ES_PLUS>::N_ROW = 31;
	template <>
	const int Screen<HW_ES_PLUS>::ROW_SIZE = 16;
	template <>
	const int Screen<HW_ES_PLUS>::OFFSET = 16;
	template <>
	const int Screen<HW_ES_PLUS>::ROW_SIZE_DISP = 12;
	template <>
	const int Screen<HW_ES_PLUS>::SPR_MAX = 19;

	template <>
	const SpriteBitmap Screen<HW_CLASSWIZ_II>::sprite_bitmap[] = {
		{"rsd_pixel", 0, 0},
		{"rsd_s", 0x01, 0x01},
		{"rsd_math", 0x01, 0x03},
		{"rsd_d", 0x01, 0x04},
		{"rsd_r", 0x01, 0x05},
		{"rsd_g", 0x01, 0x06},
		{"rsd_fix", 0x01, 0x07},
		{"rsd_sci", 0x01, 0x08},
		{"rsd_e", 0x01, 0x0A},
		{"rsd_cmplx", 0x01, 0x0B},
		{"rsd_angle", 0x01, 0x0C},
		{"rsd_wdown", 0x01, 0x0D},
		{"rsd_verify", 0x01, 0x0E},
		{"rsd_left", 0x01, 0x10},
		{"rsd_down", 0x01, 0x11},
		{"rsd_up", 0x01, 0x12},
		{"rsd_right", 0x01, 0x13},
		{"rsd_pause", 0x01, 0x15},
		{"rsd_sun", 0x01, 0x16}};

	template <>
	const SpriteBitmap Screen<HW_CLASSWIZ>::sprite_bitmap[] = {
		{"rsd_pixel", 0, 0},
		{"rsd_s", 0x01, 0x00},
		{"rsd_a", 0x01, 0x01},
		{"rsd_m", 0x01, 0x02},
		{"rsd_sto", 0x01, 0x03},
		{"rsd_math", 0x01, 0x05},
		{"rsd_d", 0x01, 0x06},
		{"rsd_r", 0x01, 0x07},
		{"rsd_g", 0x01, 0x08},
		{"rsd_fix", 0x01, 0x09},
		{"rsd_sci", 0x01, 0x0A},
		{"rsd_e", 0x01, 0x0B},
		{"rsd_cmplx", 0x01, 0x0C},
		{"rsd_angle", 0x01, 0x0D},
		{"rsd_wdown", 0x01, 0x0F},
		{"rsd_left", 0x01, 0x10},
		{"rsd_down", 0x01, 0x11},
		{"rsd_up", 0x01, 0x12},
		{"rsd_right", 0x01, 0x13},
		{"rsd_pause", 0x01, 0x15},
		{"rsd_sun", 0x01, 0x16}};

	template <>
	const SpriteBitmap Screen<HW_ES_PLUS>::sprite_bitmap[] = {
		{"rsd_pixel", 0, 0},
		{"rsd_s", 0x10, 0x00},
		{"rsd_a", 0x04, 0x00},
		{"rsd_m", 0x10, 0x01},
		{"rsd_sto", 0x02, 0x01},
		{"rsd_rcl", 0x40, 0x02},
		{"rsd_stat", 0x40, 0x03},
		{"rsd_cmplx", 0x80, 0x04},
		{"rsd_mat", 0x40, 0x05},
		{"rsd_vct", 0x01, 0x05},
		{"rsd_d", 0x20, 0x07},
		{"rsd_r", 0x02, 0x07},
		{"rsd_g", 0x10, 0x08},
		{"rsd_fix", 0x01, 0x08},
		{"rsd_sci", 0x20, 0x09},
		{"rsd_math", 0x40, 0x0A},
		{"rsd_down", 0x08, 0x0A},
		{"rsd_up", 0x80, 0x0B},
		{"rsd_disp", 0x10, 0x0B}};

	template <HardwareId hardware_id>
	void Screen<hardware_id>::Initialise() {
		if (!inited) {
			renderer = emulator.GetRenderer();
			interface_texture = emulator.GetInterfaceTexture();
			sprite_info.resize(SPR_MAX);
			for (int ix = 0; ix != SPR_MAX; ++ix)
				sprite_info[ix] = emulator.modeldef.sprites[sprite_bitmap[ix].name];

			ink_colour = emulator.modeldef.ink_color;
			if constexpr (hardware_id != HW_TI) {
				screen_buffer = new uint8_t[(N_ROW + 1) * ROW_SIZE];
				fillRandomData(screen_buffer, (N_ROW + 1) * ROW_SIZE);
				if constexpr (hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) {
					region_power.Setup(
						0xF03D, 1, "Screen/Power", this,
						[](MMURegion* region, size_t offset) {
							return ((Screen*)region->userdata)->screen_power;
						},
						[](MMURegion* region, size_t offset, uint8_t data) {
							((Screen*)region->userdata)->screen_power = data & 0xf;
							if ((data & 1) == 0) { // 关闭屏幕
								((Screen*)region->userdata)->Uninitialise();
							}
							else {
								((Screen*)region->userdata)->Initialise();
							}
						},
						emulator);
				}
				if constexpr (hardware_id == HW_CLASSWIZ_II) {
					screen_buffer1 = new uint8_t[(N_ROW + 1) * ROW_SIZE];
					fillRandomData(screen_buffer1, (N_ROW + 1) * ROW_SIZE);
				}
			}
			inited = true;
		}
		if constexpr (hardware_id == HW_TI) {
			ti_port7_data.Setup(
				0xF248, 1, "Port7/Data", this,
				(MMURegion::ReadFunction)MMURegion::IgnoreRead<0>,
				(MMURegion::WriteFunction)[](MMURegion * region, size_t offset, uint8_t data) {
					auto this_obj = (Screen*)region->userdata;
					if (data == 0)
						return;
					// std::cout << "Screen command:" << std::hex << (int)data << "\n"<< std::oct;
					switch (this_obj->ti_port_status) {
					case 0:
						if (data == 0xa0) {
							std::cout << "Enabled screen!\n";
							this_obj->ti_enabled = 1;
						}
						if (data == 0x81) {
							this_obj->ti_port_status = 1;
						}
						if (data == 0xae) {
							std::cout << "Disabled screen!\n";
							this_obj->ti_enabled = 0;
						}
						break;
					case 1:
						std::cout << "Set contrast!\n";
						this_obj->ti_contrast = data;
						this_obj->ti_port_status = 0;
						break;
					}
				},
				emulator); // f238
			ti_port5_data.Setup(
				0xF238, 1, "Port5/Data", this,
				(MMURegion::ReadFunction)MMURegion::IgnoreRead<0>,
				(MMURegion::WriteFunction)[](MMURegion * region, size_t offset, uint8_t data) {
					auto this_obj = (Screen*)region->userdata;
					if (data == 0)
						return;
					// std::cout << "Screen command1:" << std::hex << (int)data << "\n"<< std::oct;
				},
				emulator); 
			return;
		}
		if (!(hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) || (!enabled_2 && (screen_power & 1))) {
			if constexpr (hardware_id != HW_CLASSWIZ_II) {
				region_buffer.Setup(
					0xF800, (N_ROW + 1) * ROW_SIZE, "Screen/Buffer", this, [](MMURegion* region, size_t offset) {
				offset -= region->base;
				if (offset % ROW_SIZE >= ROW_SIZE_DISP)
					return (uint8_t)0;
				return ((Screen*)region->userdata)->screen_buffer[offset]; },
					[](MMURegion* region, size_t offset, uint8_t data) {
					offset -= region->base;
					if (offset % ROW_SIZE >= ROW_SIZE_DISP)
						return;

					auto this_obj = (Screen*)region->userdata;
					this_obj->screen_buffer[offset] = data; },
					emulator);
			}
			else {
				if (!emulator.modeldef.real_hardware) {
					region_buffer.Setup(
						0xF800, (N_ROW + 1) * ROW_SIZE, "Screen/Buffer", this,
						[](MMURegion* region, size_t offset) {
							offset -= region->base;
							if (offset % ROW_SIZE >= ROW_SIZE_DISP)
								return (uint8_t)0;
							return ((Screen*)region->userdata)->screen_buffer[offset];
						},
						[](MMURegion* region, size_t offset, uint8_t data) {
							offset -= region->base;
							if (offset % ROW_SIZE >= ROW_SIZE_DISP)
								return;

							auto this_obj = (Screen*)region->userdata;
							this_obj->screen_buffer[offset] = data;
						},
						emulator);
					region_buffer1.Setup(
						0x89000, (N_ROW + 1) * ROW_SIZE, "Screen/Buffer1", this,
						[](MMURegion* region, size_t offset) {
							offset -= region->base;
							if (offset % ROW_SIZE >= ROW_SIZE_DISP)
								return (uint8_t)0;
							return ((Screen*)region->userdata)->screen_buffer1[offset];
						},
						[](MMURegion* region, size_t offset, uint8_t data) {
							offset -= region->base;
							if (offset % ROW_SIZE >= ROW_SIZE_DISP)
								return;

							auto this_obj = (Screen*)region->userdata;
							this_obj->screen_buffer1[offset] = data;
						},
						emulator);
				}
				else {
					region_buffer.Setup(
						0xF800, (N_ROW + 1) * ROW_SIZE, "Screen/Buffer", this,
						[](MMURegion* region, size_t offset) {
							offset -= region->base;
							if (offset % ROW_SIZE >= ROW_SIZE_DISP)
								return (uint8_t)0;
							if (((Screen*)region->userdata)->screen_select & 0x04) {
								return ((Screen*)region->userdata)->screen_buffer1[offset];
							}
							else {
								return ((Screen*)region->userdata)->screen_buffer[offset];
							}
						},
						[](MMURegion* region, size_t offset, uint8_t data) {
							offset -= region->base;
							if (offset % ROW_SIZE >= ROW_SIZE_DISP)
								return;

							auto this_obj = (Screen*)region->userdata;
							// * Set require_frame to true only if the value changed.
							if (((Screen*)region->userdata)->screen_select & 0x04) {
								this_obj->screen_buffer1[offset] = data;
							}
							else {
								this_obj->screen_buffer[offset] = data;
							}
						},
						emulator);
				}
			}
			if constexpr (hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) {
				region_range.Setup(0xF030, 1, "Screen/Range", &screen_range, MMURegion::DefaultRead<uint8_t, 0x2F>,
					MMURegion::DefaultWrite<uint8_t, 0x2F>, emulator);
			}
			else {
				region_range.Setup(0xF030, 1, "Screen/Range", &screen_range, MMURegion::DefaultRead<uint8_t, 0x07>,
					MMURegion::DefaultWrite<uint8_t, 0x07>, emulator);
			}

			if constexpr (hardware_id == HW_CLASSWIZ_II) {
				region_mode.Setup(
					0xF031, 1, "Screen/Mode", this,
					[](MMURegion* region, size_t offset) {
						auto screen = ((Screen*)region->userdata);
						return screen->screen_mode;
					},
					[](MMURegion* region, size_t offset, uint8_t data) {
						auto screen = ((Screen*)region->userdata);
						auto old = screen->screen_mode & 0b1000;
						auto new_ = data & 0b1000;
						if (old ^ new_) {
							// TODO: 交换缓冲区有效视窗位数据
							auto sb = screen->screen_buffer;
							for (int iy = 0; iy != (N_ROW + 1); ++iy) {
								for (int ix = 0; ix != ROW_SIZE_DISP; ++ix) {
									sb[ix + iy * ROW_SIZE] = bit_lookup_table[sb[(ix) + iy * ROW_SIZE]];
								}
							}
							for (int iy = 0; iy != (N_ROW + 1); ++iy) {
								for (int ix = 0; ix != (ROW_SIZE_DISP / 2); ++ix) {
									std::swap(sb[ix + iy * ROW_SIZE], sb[(ROW_SIZE_DISP - 1 - ix) + iy * ROW_SIZE]);
								}
							}
							if constexpr (hardware_id == HW_CLASSWIZ_II) {
								sb = screen->screen_buffer1;
								for (int iy = 0; iy != (N_ROW + 1); ++iy) {
									for (int ix = 0; ix != ROW_SIZE_DISP; ++ix) {
										sb[ix + iy * ROW_SIZE] = bit_lookup_table[sb[(ix) + iy * ROW_SIZE]];
									}
								}
								for (int iy = 0; iy != (N_ROW + 1); ++iy) {
									for (int ix = 0; ix != (ROW_SIZE_DISP / 2); ++ix) {
										std::swap(sb[ix + iy * ROW_SIZE], sb[(ROW_SIZE_DISP - 1 - ix) + iy * ROW_SIZE]);
									}
								}
							}
						}
						screen->screen_mode = data & 127;
					},
					emulator);
			}
			else if constexpr (hardware_id == HW_CLASSWIZ) {
				region_mode.Setup(0xF031, 1, "Screen/Mode", &screen_mode, MMURegion::DefaultRead<uint8_t, 63>,
					MMURegion::DefaultWrite<uint8_t, 63>, emulator);
			}
			else {
				region_mode.Setup(0xF031, 1, "Screen/Mode", &screen_mode, MMURegion::DefaultRead<uint8_t, 0x07>,
					MMURegion::DefaultWrite<uint8_t, 0x07>, emulator);
			}
			if constexpr (hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) {
				region_contrast.Setup(0xF032, 1, "Screen/Contrast", &screen_contrast, MMURegion::DefaultRead<uint8_t, 0x3F>,
					MMURegion::DefaultWrite<uint8_t, 0x3F>, emulator);
				region_unk1.Setup(
					0xF03E, 1, "Screen/Unk1", this,
					[](MMURegion* region, size_t offset) {
						return (uint8_t)0;
					},
					[](MMURegion* region, size_t offset, uint8_t data) {
						((Screen*)region->userdata)->emulator.chipset.mmu.WriteData(0xF817, data);
					},
					emulator);
				region_unk2.Setup(
					0xF03F, 1, "Screen/Unk2", this,
					[](MMURegion* region, size_t offset) {
						return (uint8_t)0;
					},
					[](MMURegion* region, size_t offset, uint8_t data) {
						((Screen*)region->userdata)->emulator.chipset.mmu.WriteData(0xF817, data);
					},
					emulator);
			}
			else {
				region_contrast.Setup(0xF032, 1, "Screen/Contrast", &screen_contrast, MMURegion::DefaultRead<uint8_t, 0x1f>,
					MMURegion::DefaultWrite<uint8_t, 0x1f>, emulator);
			}

			if constexpr (hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) {
				region_select.Setup(0xF037, 1, "Screen/Select", &screen_select, MMURegion::DefaultRead<uint8_t, 0x04 | 1>,
					MMURegion::DefaultWrite<uint8_t, 0x04 | 1>, emulator);

				region_brightness.Setup(0xF033, 1, "Screen/Brightness", &screen_brightness, MMURegion::DefaultRead<uint8_t, 0x07>,
					MMURegion::DefaultWrite<uint8_t, 0x07>, emulator);

				region_contrast2.Setup(0xF035, 1, "Screen/Contrast2", &screen_contrast2, MMURegion::DefaultRead<uint8_t, 0x1F>,
					MMURegion::DefaultWrite<uint8_t, 0x1F>, emulator);

				region_contrast2_en.Setup(0xF036, 1, "Screen/Contrast2EN", &screen_contrast2_en, MMURegion::DefaultRead<uint8_t, 0b1001>,
					MMURegion::DefaultWrite<uint8_t, 0b1001>, emulator);
			}
			else {
				screen_contrast2 = 0x17;
				screen_contrast2_en = 1;
			}

			if constexpr (hardware_id == HardwareId::HW_ES_PLUS) {
				region_refresh_rate.Setup(0xF034, 1, "Screen/Unknown_F034", &unk_f034, MMURegion::DefaultRead<uint8_t, 0b11>,
					MMURegion::DefaultWrite<uint8_t, 0b11>, emulator);
			}
			else {
				region_offset.Setup(0xF039, 1, "Screen/DSPOFST", &screen_offset, MMURegion::DefaultRead<uint8_t, 0x3F>,
					MMURegion::DefaultWrite<uint8_t, 0x3F>, emulator);

				region_refresh_rate.Setup(0xF034, 1, "Screen/RefreshRate", &screen_refresh_rate, MMURegion::DefaultRead<uint8_t, 0x7F>,
					MMURegion::DefaultWrite<uint8_t, 0x7F>, emulator);
			}
			enabled_2 = true;
		}
	}

	template <HardwareId hardware_id>
	void Screen<hardware_id>::Uninitialise() {
		fillRandomData(screen_buffer, (N_ROW + 1) * ROW_SIZE);
		if constexpr (hardware_id == HW_CLASSWIZ_II) {
			fillRandomData(screen_buffer1, (N_ROW + 1) * ROW_SIZE);
		}
		if constexpr (hardware_id != HW_CLASSWIZ_II) {
			region_buffer.Kill();
		}
		else {
			if (!emulator.modeldef.real_hardware) {
				region_buffer.Kill();
				region_buffer1.Kill();
			}
			else {
				region_buffer.Kill();
			}
		}
		screen_range = 0;
		region_range.Kill();
		screen_mode = 0;
		region_mode.Kill();
		screen_contrast = 0;
		region_contrast.Kill();
		if constexpr (hardware_id == HW_CLASSWIZ || hardware_id == HW_CLASSWIZ_II) {
			screen_select = 0;
			region_select.Kill();
			screen_contrast2 = 0;
			region_contrast2.Kill();
			screen_contrast2_en = 0;
			region_contrast2_en.Kill();
			region_unk1.Kill();
			region_unk2.Kill();
			screen_brightness = 0;
			region_brightness.Kill();
		}
		screen_refresh_rate = 0;
		region_refresh_rate.Kill();
		if constexpr (hardware_id != HardwareId::HW_ES_PLUS) {
			screen_offset = 0;
			region_offset.Kill();
		}
		enabled_2 = false;
	}
	template <HardwareId hardware_id>
	void Screen<hardware_id>::Frame() {
		int x = 0;
		if (!emulator.modeldef.enable_new_screen) {
			SDL_SetTextureColorMod(interface_texture, ink_colour.r, ink_colour.g, ink_colour.b);
		}
		for (int ix = 1; ix != SPR_MAX; ++ix) {
			SDL_SetTextureAlphaMod(interface_texture, Uint8(std::clamp((int)screen_ink_alpha[x], 0, 255)));
			x++;
			SDL_RenderCopy(renderer, interface_texture, &sprite_info[ix].src, &sprite_info[ix].dest);
		}
		static constexpr auto SPR_PIXEL = 0;
		SDL_Rect dest = Screen<hardware_id>::sprite_info[SPR_PIXEL].dest;
		for (int iy2 = 1; iy2 != (N_ROW + 1); ++iy2) {
			int x = 0;
			dest.x = sprite_info[SPR_PIXEL].dest.x;
			dest.y = sprite_info[SPR_PIXEL].dest.y + (iy2 - 1) * sprite_info[SPR_PIXEL].src.h;
			for (int ix = 0; ix != ROW_SIZE_DISP; ++ix) {
				for (uint8_t mask = 0x80; mask; mask >>= 1, dest.x += sprite_info[SPR_PIXEL].src.w) {
					if (screen_ink_alpha[x + iy2 * 192] > 255) {
						SDL_SetTextureColorMod(interface_texture,
							std::max(0, ink_colour.r - (int)(screen_ink_alpha[x + iy2 * 192] - 255)),
							std::max(0, ink_colour.g - (int)((screen_ink_alpha[x + iy2 * 192] - 255) * 0.8)),
							std::max(0, ink_colour.b - (int)((screen_ink_alpha[x + iy2 * 192] - 255) * 0.1)));
						SDL_SetTextureAlphaMod(interface_texture, 255);
					}
					else {
						SDL_SetTextureColorMod(interface_texture, ink_colour.r, ink_colour.g, ink_colour.b);
						SDL_SetTextureAlphaMod(interface_texture, Uint8(std::clamp((int)screen_ink_alpha[x + iy2 * 192], 0, 255)));
					}
					x++;
					SDL_RenderCopy(renderer, interface_texture, &sprite_info[SPR_PIXEL].src, &dest);
				}
			}
		}
	}

	template <HardwareId hardware_id>
	void Screen<hardware_id>::Reset() {
	}

	Peripheral* CreateScreen(Emulator& emulator) {
		switch (emulator.hardware_id) {
		case HW_FX_5800P:
		case HW_ES_PLUS:
			return new Screen<HW_ES_PLUS>(emulator);

		case HW_CLASSWIZ:
			return new Screen<HW_CLASSWIZ>(emulator);

		case HW_CLASSWIZ_II:
			return new Screen<HW_CLASSWIZ_II>(emulator);

		case HW_TI:
			return new Screen<HW_TI>(emulator);

		default:
			PANIC("Unknown hardware id\n");
		}
	}
} // namespace casioemu
