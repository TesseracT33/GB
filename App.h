#pragma once

#include "wx/wx.h"

#include "GUI.h"

class App : public wxApp
{
public:
	App();
	~App();

	bool OnInit() override;

private:
	GUI* gui = nullptr;
};