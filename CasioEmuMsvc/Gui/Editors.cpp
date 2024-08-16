#include "Editors.h"
#include "CPU.hpp"
#include "Chipset/Chipset.hpp"
#include "Hooks.h"
#include "Models.h"
#include "Ui.hpp"
#include "hex.hpp"
float ram_edit_ov[0x100000]{};
struct HexEditor : public UIWindow, public MemoryEditor {
	void* data{};
	size_t size{};
	size_t display_base{};
	HexEditor(const char* name, void* data, size_t size, size_t base) : UIWindow(name), data(data), size(size), display_base(base) {
		flags = ImGuiWindowFlags_NoScrollbar;
		this->ram_edit_ov = ::ram_edit_ov;
	}
	void RenderCore() override {
		this->DrawContents(data, size, display_base);
	}
};
struct SpansHexEditor : public UIWindow, public MemoryEditor {
	void* data{};
	size_t size{};
	size_t display_base{};
	std::vector<MarkedSpan> spans{};
	SpansHexEditor(const char* name, void* data, size_t size, size_t base, std::vector<MarkedSpan> spans) : UIWindow(name), data(data), size(size), display_base(base), spans(spans) {
		flags = ImGuiWindowFlags_NoScrollbar;
		this->ram_edit_ov = ::ram_edit_ov;
	}
	void RenderCore() override {
		this->DrawContents(data, size, display_base, spans);
	}
};

inline auto MMU_Hex(auto he) {
	he->ReadFn = [](const ImU8* data, size_t off) -> ImU8 {
		return me_mmu->ReadData((size_t)data + off, 0);
	};
	he->WriteFn = [](ImU8* data, size_t off, ImU8 d) {
		return me_mmu->WriteData((size_t)data + off, d, 0);
	};
	return he;
}
inline auto Highlight_Default(auto he) {
	he->HighlightFn = [](const ImU8* data, size_t off) -> bool {
		if ((uint32_t)(data + off) == (uint32_t)m_emu->chipset.cpu.reg_sp) {
			return true;
		}
		if ((uint32_t)(data + off) == (uint32_t)casioemu::GetInputAreaOffset(m_emu->hardware_id) + *((unsigned char*)n_ram_buffer - casioemu::GetRamBaseAddr(m_emu->hardware_id) + casioemu::GetCursorOffset(m_emu->hardware_id))) {
			return true;
		}
		return false;
	};
	return he;
}

std::vector<UIWindow*> GetEditors() {
	SetupHook(on_memory_write, [](casioemu::MMU& mmu, MemoryEventArgs& mea) {
		// if (mmu.ReadData(mea.offset) != mea.value)
		if (mea.offset < 0x10000)
			ram_edit_ov[mea.offset] = 255;
	});
	std::vector<UIWindow*> windows;
	windows.push_back(
		Highlight_Default(
			MMU_Hex(
				new SpansHexEditor{
					"Ram",
					(void*)casioemu::GetRamBaseAddr(m_emu->hardware_id),
					0x10000 - casioemu::GetRamBaseAddr(m_emu->hardware_id),
					casioemu::GetRamBaseAddr(m_emu->hardware_id),
					GetCommonMemLabels(m_emu->hardware_id)})));
	windows.push_back(new HexEditor{"Rom", m_emu->chipset.rom_data.data(), m_emu->chipset.rom_data.size(), 0});
	if (m_emu->hardware_id == casioemu::HW_FX_5800P) {
		windows.push_back(MMU_Hex(new HexEditor{"PRam", (void*)0x40000, 0x8000, 0x40000}));
		windows.push_back(new HexEditor{"Flash", m_emu->chipset.flash_data.data(), m_emu->chipset.flash_data.size(), 0});
	}
	windows.push_back(MMU_Hex(new HexEditor{"All", 0, 0xfffff, 0}));
	return windows;
}