#include "5800Flash.h"
#include "../Chipset/MMU.hpp"
#include "../Chipset/Chipset.hpp"
#include "../Emulator.hpp"
namespace casioemu {
	class Flash2 : public casioemu::Peripheral {
	public:
		MMURegion flash;
		int flash_mode = 0;
		Flash2(Emulator& emu) : casioemu::Peripheral(emu) {
		}
		void Initialise() override {
			flash.Setup(
				0x80000, 0x80000, "Flash/Fx5800PFlash", this,
				[](MMURegion* region, size_t offset) -> uint8_t {
					auto flash = ((Flash2*)region->userdata);
					auto fo = offset & 0x7ffff;
					if (flash->flash_mode == 6) {
						flash->flash_mode = 0;
						return 0x80;
					}
					return flash->emulator.chipset.flash_data[fo];
				},
				[](MMURegion* region, size_t offset, uint8_t data) {
					auto flash = ((Flash2*)region->userdata);
					auto fo = offset & 0x7ffff;
					switch (flash->flash_mode) {
					case 0:
						if (fo == 0xaaa && data == 0xaa) {
							flash->flash_mode = 1;
							return;
						}
						break;
					case 1:
						if (fo == 0x555 && data == 0x55) {
							flash->flash_mode = 2;
							return;
						}
						break;
					case 2:
						if (fo == 0xAAA && data == 0xA0) {
							flash->flash_mode = 3;
							return;
						}
						if (fo == 0xaaa && data == 0x80) {
							flash->flash_mode = 4;
							return;
						}
						break;
					case 3:
						printf("Program %x to %x\n", (int)fo, data);
						flash->emulator.chipset.flash_data[fo] = data;
						flash->flash_mode = 0;
						return;
					case 4:
						if (fo == 0xAAA && data == 0xaa) {
							flash->flash_mode = 5;
							return;
						}
						break;
					case 5:
						if (fo == 0x555 && data == 0x55) {
							flash->flash_mode = 6;
							return;
						}
						break;
					case 6: // we dont know sector's mapping(?)
						if (fo == 0)
							memset(&flash->emulator.chipset.flash_data[fo], 0xff, 0x7fff);
						if (fo == 0x20000 || fo == 0x30000)
							memset(&flash->emulator.chipset.flash_data[fo], 0xff, 0xffff);
						printf("Erase %x (%x)\n", (int)fo, data);
						return;
					case 7:
						if (fo == 0xaaa && data == 0xaa) {
							flash->flash_mode = 1;
							return;
						}
						break;
					}
					if (data == 0xf0) {
						printf("Reset mode.\n");
						flash->flash_mode = 0;
						return;
					}
					//if (data == 0xb0) {
					//	printf("Erase Suspend.\n");
					//	return;
					//}
					//if (data == 0x30) {
					//	printf("Erase Suspend.\n");
					//	return;
					//}
					printf("Unknown jedec %05x = %02x\n", (int)fo, data);
				},
				emulator);
		}
	};
	Peripheral* CreateFx5800Flash(Emulator& emu) {
		return new Flash2(emu);
	}
} // namespace casioemu