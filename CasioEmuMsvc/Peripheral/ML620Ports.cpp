#include "Config.hpp"
#include "MMURegion.hpp"
#include "Peripheral.hpp"
#define DefSfr(x)        \
	MMURegion reg_##x{}; \
	uint8_t dat_##x{};

#define DefSfr2(x)       \
	MMURegion reg_##x{}; \
	uint16_t dat_##x{};

#define DefSfr4(x)       \
	MMURegion reg_##x{}; \
	uint32_t dat_##x{};

namespace casioemu {
	class ML620Port {
		DefSfr(data);
		DefSfr(dir);
		DefSfr2(mode0);
		DefSfr2(mode1);
		DefSfr2(con);

		DefSfr4(exicon);

		DefSfr(ie);
		DefSfr(is);
		DefSfr(ic);
		DefSfr(iu);

	public:
		ML620Port(Emulator& emulator, int i) {
			auto pbase = 0xf210 + i * 8;
			reg_data.Setup(
				pbase++, 1, "PortN/Data", this,
				[](MMURegion* reg, size_t offset) -> uint8_t {
					auto p = (ML620Port*)reg->userdata;
					return p->dat_data;
				},
				[](MMURegion* reg, size_t offset, uint8_t data) {
					auto p = (ML620Port*)reg->userdata;
					p->dat_data = data;
				},
				emulator);
			reg_dir.Setup(
				pbase++, 1, "PortN/Direction", this,
				[](MMURegion* reg, size_t offset) -> uint8_t {
					auto p = (ML620Port*)reg->userdata;
					return p->dat_dir;
				},
				[](MMURegion* reg, size_t offset, uint8_t data) {
					auto p = (ML620Port*)reg->userdata;
					p->dat_dir = data;
				},
				emulator);
			reg_con.Setup((pbase += 2) - 2, 2, "PortN/Control", &dat_con, MMURegion::DefaultRead<uint16_t>, MMURegion::DefaultWrite<uint16_t>, emulator);

			reg_mode0.Setup((pbase += 2) - 2, 2, "PortN/Mode01", &dat_mode0, MMURegion::DefaultRead<uint16_t>, MMURegion::DefaultWrite<uint16_t>, emulator);
			if (i == 3 || i == 5) {
				reg_mode1.Setup((pbase += 2) - 2, 2, "PortN/Mode23", &dat_mode1, MMURegion::DefaultRead<uint16_t>, MMURegion::DefaultWrite<uint16_t>, emulator);
			}
			if (i != 6) {
				pbase = 0xf980 + i * 8;
				if (i != 2) { // P2 only have 1
					reg_exicon.Setup((pbase += 4) - 4, 2, "PortN/ExI", &dat_exicon, MMURegion::DefaultRead<uint16_t>, MMURegion::DefaultWrite<uint16_t>, emulator);
				}
				else
					reg_exicon.Setup((pbase += 4) - 4, 4, "PortN/ExI", &dat_exicon, MMURegion::DefaultRead<uint16_t>, MMURegion::DefaultWrite<uint16_t>, emulator);
				reg_ie.Setup(pbase++, 1, "PortN/Enable", &dat_ie, MMURegion::DefaultRead<uint8_t>, MMURegion::DefaultWrite<uint8_t>, emulator);
				reg_is.Setup(pbase++, 1, "PortN/Status", &dat_is, MMURegion::DefaultRead<uint8_t>, MMURegion::DefaultWrite<uint8_t>, emulator);
				reg_ic.Setup(pbase++, 1, "PortN/Clear", &dat_ic, MMURegion::DefaultRead<uint8_t>, MMURegion::DefaultWrite<uint8_t>, emulator);
				reg_iu.Setup(pbase++, 1, "PortN/Update", &dat_iu, MMURegion::DefaultRead<uint8_t>, MMURegion::DefaultWrite<uint8_t>, emulator);
			}
		}
	};
	class Ports : public Peripheral {
	public:
		ML620Port* ports[16];
		Ports(Emulator& emu) : Peripheral(emu) {
		}
		void Initialise() override {
			auto init = {0, 2, 3, 4, 5, 6, 7, 8};
			for (auto num : init) {
				ports[num] = new ML620Port{emulator, num};
			}
		}
	};
	Peripheral* CreateML620Ports(Emulator& emu) {
		return new Ports(emu);
	}
} // namespace casioemu