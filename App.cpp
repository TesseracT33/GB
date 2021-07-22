#include "App.h"

wxIMPLEMENT_APP(App);


App::App()
{
}


App::~App()
{
}


bool App::OnInit()
{
	gui = new GUI();
	gui->Show();
	return true;
}