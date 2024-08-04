#pragma once
#include "Ui.hpp"
class LabelViewer : public UIWindow {
public:
	LabelViewer() : UIWindow("Labels"){}
	void RenderCore() override;
};
