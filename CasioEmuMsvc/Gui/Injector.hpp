#pragma once
#include "../Emulator.hpp"
#include "Ui.hpp"
class Injector : public UIWindow {
private:
	char data_buf[1024];

public:
	Injector() : UIWindow("Rop") {}
	void RenderCore() override;
};
