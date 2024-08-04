#include "Timer.hpp"

#include "../Chipset/Chipset.hpp"
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "../Logger.hpp"

#include <cmath>

namespace casioemu {
	class Timer : public Peripheral {
		MMURegion region_counter, region_interval, region_F024, region_control;
		uint16_t data_counter, data_interval;
		uint8_t data_F024, data_control;

		size_t TM0INT = 4;

		uint64_t ext_to_int_counter, ext_to_int_next, ext_to_int_int_done;

		size_t TimerFreqDiv;

		unsigned int cycles_per_second;
		static const uint64_t ext_to_int_frequency = 16384;

	public:
		using Peripheral::Peripheral;

		void Initialise();
		void Reset();
		void Tick();
		void Uninitialise();
	};
	void Timer::Initialise() {
		if (enabled)
			return;

		enabled = true;

		cycles_per_second = emulator.GetCyclesPerSecond();

		TimerFreqDiv = 1;
		if (emulator.modeldef.real_hardware) {
			clock_type = CLOCK_LSCLK;
		}
		else {
			clock_type = CLOCK_EMUCLK;
		}

		block_bit = 3;

		ext_to_int_counter = 0;
		data_interval = 0;
		data_counter = 0;
		data_control = 0;
		data_F024 = 0;

		region_interval.Setup(
			0xF020, 2, "Timer/TM0D", &data_interval, MMURegion::DefaultRead<uint16_t>, [](MMURegion* region, size_t offset, uint8_t data) {
				uint16_t* value = (uint16_t*)(region->userdata);
				*value &= ~(((uint16_t)0xFF) << ((offset - region->base) * 8));
				*value |= ((uint16_t)data) << ((offset - region->base) * 8);
				// if (!*value)
				// 	*value = 1;
			},
			emulator);

		region_counter.Setup(
			0xF022, 2, "Timer/TM0C", &data_counter, MMURegion::DefaultRead<uint16_t>, [](MMURegion* region, size_t, uint8_t) {
				*((uint16_t*)region->userdata) = 0;
			},
			emulator);

		region_F024.Setup(
			0xF024, 1, "Timer/TM0CON0", this,
			[](MMURegion* region, size_t) {
				Timer* timer = (Timer*)region->userdata;
				return (uint8_t)(timer->data_F024 & 0x0F);
			},
			[](MMURegion* region, size_t, uint8_t data) {
				Timer* timer = (Timer*)region->userdata;
				timer->data_F024 = data & 0x0F;
				timer->TimerFreqDiv = std::pow(2, data & 0x07);
				if (timer->emulator.modeldef.real_hardware) {
					if (data & 0x08)
						timer->clock_type = CLOCK_HSCLK;
					else
						timer->clock_type = CLOCK_LSCLK;
				}
			},
			emulator);

		region_control.Setup(
			0xF025, 1, "Timer/TM0CON1", this, [](MMURegion* region, size_t) {
			Timer *timer = (Timer *)region->userdata;
			return (uint8_t)(timer->data_control & 0x01); }, [](MMURegion* region, size_t, uint8_t data) {
			Timer *timer = (Timer *)region->userdata;
			timer->data_control = data & 0x01; }, emulator);
	}

	void Timer::Reset() {
		if (!enabled) {
			Initialise();
			return;
		}

		ext_to_int_counter = 0;

		data_interval = 0;
		data_counter = 0;
		data_control = 0;
		data_F024 = 0;
	}

	void Timer::Tick() {
		if (clock_type == CLOCK_EMUCLK) {
			if (!data_interval) {
				ext_to_int_counter = 0;
			}
			else if (++ext_to_int_counter >= (data_interval * TimerFreqDiv) / 32678.0 / 0.025 * 2) {
				ext_to_int_counter = 0;
				emulator.chipset.MaskableInterrupts[TM0INT].TryRaise();
			}
			return;
		}
		if (data_control) {
			if (++ext_to_int_counter >= TimerFreqDiv) {
				ext_to_int_counter = 0;
				if (!data_interval) {
					data_counter = 0;
				}
				else if (++data_counter >= data_interval) {
					data_counter = 0;
					emulator.chipset.MaskableInterrupts[TM0INT].TryRaise();
				}
			}
		}
	}

	void Timer::Uninitialise() {
		if (!enabled)
			return;

		enabled = false;

		clock_type = CLOCK_STOPPED;

		region_interval.Kill();
		region_counter.Kill();
		region_F024.Kill();
		region_control.Kill();
	}
	Peripheral* CreateTimer(Emulator& emu) {
		return new Timer(emu);
	}
} // namespace casioemu
