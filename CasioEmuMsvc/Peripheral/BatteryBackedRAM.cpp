#include "BatteryBackedRAM.hpp"

#include "../Chipset/Chipset.hpp"
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "../Gui/ui.hpp"
#include "../Logger.hpp"
#include "../ModelInfo.h"
#include "../Models.h"
#include <cstring>
#include <fstream>

namespace casioemu {
	class BatteryBackedRAM : public Peripheral {
		MMURegion region, region_2, region_3, region_4, region_5;

		size_t ram_size;
		bool ram_file_requested;

	public:
		using Peripheral::Peripheral;
		uint8_t* ram_buffer;
		uint8_t* pram_buffer;
		void Initialise();
		void Uninitialise();
		void SaveRAMImage();
		void LoadRAMImage();
	};
	void BatteryBackedRAM::Initialise() {
		bool real_hardware = emulator.modeldef.real_hardware;
		ram_size = GetRamSize(emulator.hardware_id);
		if (!real_hardware)
			ram_size += 0x100;

		ram_buffer = new uint8_t[ram_size];
		memset(ram_buffer, 0, ram_size);

		ram_file_requested = false;
		if (emulator.argv_map.find("ram") != emulator.argv_map.end()) {
			ram_file_requested = true;

			if (emulator.argv_map.find("clean_ram") == emulator.argv_map.end())
				LoadRAMImage();
		}

		region.Setup(
			GetRamBaseAddr(emulator.hardware_id),
			GetRamSize(emulator.hardware_id),
			"BatteryBackedRAM", ram_buffer, [](MMURegion* region, size_t offset) { return ((uint8_t*)region->userdata)[offset - region->base]; }, [](MMURegion* region, size_t offset, uint8_t data) { ((uint8_t*)region->userdata)[offset - region->base] = data; }, emulator);
		if (emulator.hardware_id == HW_FX_5800P) {
			pram_buffer = new uint8_t[0x8000];
			region_5.Setup(
				0x40000,
				0x8000,
				"Segment4", pram_buffer, [](MMURegion* region, size_t offset) { return ((uint8_t*)region->userdata)[offset - region->base]; }, [](MMURegion* region, size_t offset, uint8_t data) { ((uint8_t*)region->userdata)[offset - region->base] = data; }, emulator);
		}
		if (!real_hardware)
			region_2.Setup(
				emulator.hardware_id == HW_ES_PLUS ? 0x9800 : emulator.hardware_id == HW_CLASSWIZ ? 0x49800
																								  : 0x89800,
				0x0100,
				"BatteryBackedRAM/2", ram_buffer + ram_size - 0x100, [](MMURegion* region, size_t offset) { return ((uint8_t*)region->userdata)[offset - region->base]; }, [](MMURegion* region, size_t offset, uint8_t data) { ((uint8_t*)region->userdata)[offset - region->base] = data; }, emulator);

		n_ram_buffer = (char*)ram_buffer;
		// logger::Info("inited hex editor!\n");
	}

	void BatteryBackedRAM::Uninitialise() {
		if (ram_file_requested && emulator.argv_map.find("preserve_ram") == emulator.argv_map.end())
			SaveRAMImage();

		delete[] ram_buffer;
	}

	void BatteryBackedRAM::SaveRAMImage() {
		std::ofstream ram_handle(emulator.argv_map["ram"], std::ofstream::binary);
		if (ram_handle.fail()) {
			logger::Info("[BatteryBackedRAM] std::ofstream failed: %s\n", std::strerror(errno));
			return;
		}
		ram_handle.write((char*)ram_buffer, ram_size);
		if (ram_handle.fail()) {
			logger::Info("[BatteryBackedRAM] std::ofstream failed: %s\n", std::strerror(errno));
			return;
		}
	}

	void BatteryBackedRAM::LoadRAMImage() {
		std::ifstream ram_handle(emulator.argv_map["ram"], std::ifstream::binary);
		if (ram_handle.fail()) {
			logger::Info("[BatteryBackedRAM] std::ifstream failed: %s\n", std::strerror(errno));
			return;
		}
		ram_handle.read((char*)ram_buffer, ram_size);
		if (ram_handle.fail()) {
			logger::Info("[BatteryBackedRAM] std::ifstream failed: %s\n", std::strerror(errno));
			return;
		}
	}
	Peripheral* CreateBatteryBackedRAM(Emulator& emu) {
		return new BatteryBackedRAM(emu);
	}
} // namespace casioemu
