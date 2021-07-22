#include "Config.h"
#include <fstream>


void Config::AddConfigurable(Configurable* configurable)
{
	assert(configurable != nullptr);
	this->configurables.push_back(configurable);
}


bool Config::ConfigFileExists()
{
	return wxFileExists(config_file_path);
}


void Config::Save()
{
	std::ofstream ofs(config_file_path.mb_str(), std::ofstream::out | std::fstream::trunc);
	if (!ofs) // if the file could not be created
	{
		wxMessageBox("Config file could not be created or saved.");
		SetDefaults(false);
		return;
	}

	for (Configurable* configurable : configurables)
		configurable->SaveConfig(ofs);

	ofs.close();
}


void Config::Load()
{
	std::ifstream ifs(config_file_path.mb_str(), std::ifstream::in);
	if (!ifs) // if the file could not be opened
	{
		wxMessageBox("Config file could not be opened. Reverting to defaults.");
		SetDefaults(false);
		return;
	}

	for (Configurable* configurable : configurables)
		configurable->LoadConfig(ifs);

	ifs.close();
}


void Config::SetDefaults(bool save)
{
	for (Configurable* configurable : configurables)
		configurable->SetDefaultConfig();

	if (save)
		Save();
}