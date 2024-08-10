#include "Peripheral.hpp"
#include <MMURegion.hpp>
namespace casioemu {
	class Uart : public Peripheral {
	public:
		MMURegion region_UA0BUF, region_UA0CON, region_UA0MOD0, region_UA0MOD1, region_UA0BRTH, region_UA0STAT;
		uint8_t uart_control{}, uart_mod0{}, uart_mod1{};
		uint16_t uart_baud{};
		uint8_t uart_buf{};
		uint8_t uart_status{};

		using Peripheral::Peripheral;
		 // TODO: ªÀŸ µœ÷
		void Initialise() {
			region_UA0BUF.Setup(0xF290, 1, "Uart0/Buffer", this, MMURegion::IgnoreRead<0>, 0, emulator);
			region_UA0CON.Setup(0xF291, 1, "Uart0/Control", &uart_control, MMURegion::DefaultRead<uint8_t, 0x1>, MMURegion::DefaultWrite<uint8_t, 0x1>, emulator);
			region_UA0MOD0.Setup(0xF292, 1, "Uart0/Mode0", &uart_mod0, MMURegion::DefaultRead<uint8_t, 0b10111>, MMURegion::DefaultWrite<uint8_t, 0b10111>, emulator);
			region_UA0MOD1.Setup(0xF293, 1, "Uart0/Mode1", &uart_mod1, MMURegion::DefaultRead<uint8_t, 0x7f>, MMURegion::DefaultWrite<uint8_t, 0x7f>, emulator);
			region_UA0BRTH.Setup(0xF294, 2, "Uart0/Baud", &uart_baud, MMURegion::DefaultRead<uint16_t, 0b111111111111>, MMURegion::DefaultWrite<uint16_t, 0b111111111111>, emulator);
			region_UA0STAT.Setup(
				0xF296, 1, "Uart0/Status", &uart_status, MMURegion::DefaultRead<uint8_t>,
				[](MMURegion* region, size_t, uint8_t data) {
					*(char*)region->userdata = 0;
				}, emulator);
		}
		void Reset() {

		}
		void Tick() {

		}
	};
	Peripheral* CreateUart(Emulator& emu) {
		return new Uart(emu);
	}
} // namespace casioemu