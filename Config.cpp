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
	std::ofstream ofs(config_file_path.mb_str(), std::ofstream::out | std::ofstream::binary);
	if (!ofs) // if the file could not be created
	{
		wxMessageBox("Config file could not be created or saved.");
		SetDefaults(false);
		return;
	}

	Serialization::SerializeFunctor functor{ ofs };

	for (Configurable* configurable : configurables)
		configurable->Configure(functor);

	ofs.close();
}


void Config::Load()
{
	std::ifstream ifs(config_file_path.mb_str(), std::ifstream::in | std::ofstream::binary);
	if (!ifs) // if the file could not be opened
	{
		wxMessageBox("Config file could not be opened. Reverting to defaults.");
		SetDefaults(false);
		return;
	}

	Serialization::DeserializeFunctor functor{ ifs };

	for (Configurable* configurable : configurables)
		configurable->Configure(functor);

	ifs.close();
}


void Config::SetDefaults(bool save)
{
	for (Configurable* configurable : configurables)
		configurable->SetDefaultConfig();

	if (save)
		Save();
}