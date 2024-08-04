#include "Editors.h"
#include "../Chipset/Chipset.hpp"
#include "../Models.h"
#include "Ui.hpp"
#include "hex.hpp"
struct HexEditor : public UIWindow, public MemoryEditor {
	void* data{};
	size_t size{};
	size_t display_base{};
	HexEditor(const char* name, void* data, size_t size, size_t base) : UIWindow(name), data(data), size(size), display_base(base) {
		flags = ImGuiWindowFlags_NoScrollbar;
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
	}
	void RenderCore() override {
		this->DrawContents(data, size, display_base, spans);
	}
};

inline auto MMU_Hex(auto he) {
	he->ReadFn = [](const ImU8* data, size_t off) -> ImU8 {
		return me_mmu->ReadData((size_t)data+off, 0);
	};
	he->WriteFn = [](ImU8* data, size_t off, ImU8 d) {
		return me_mmu->WriteData((size_t)data + off, d, 0);
	};
	return he;
}

std::vector<UIWindow*> GetEditors() {
	std::vector<UIWindow*> windows;
	windows.push_back(MMU_Hex(new SpansHexEditor{"Ram", (void*)casioemu::GetRamBaseAddr(m_emu->hardware_id), 0x10000 - casioemu::GetRamBaseAddr(m_emu->hardware_id), casioemu::GetRamBaseAddr(m_emu->hardware_id), GetCommonMemLabels(m_emu->hardware_id)}));
	windows.push_back(new HexEditor{"Rom", m_emu->chipset.rom_data.data(), m_emu->chipset.rom_data.size(), 0});
	if (m_emu->hardware_id == casioemu::HW_FX_5800P) {
		windows.push_back(MMU_Hex(new HexEditor{"PRam", (void*)0x40000, 0x8000, 0x40000}));
		windows.push_back(new HexEditor{"Flash", m_emu->chipset.flash_data.data(), m_emu->chipset.flash_data.size(), 0});
	}
	windows.push_back(MMU_Hex(new HexEditor{"All", 0, 0xfffff, 0}));
	return windows;
}