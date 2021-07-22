#pragma once

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/string.h>
#include "wx/wx.h"

#include "Configurable.h"

class Config
{
public:
	std::vector<Configurable*> configurables;

	void AddConfigurable(Configurable* configurable);

	bool ConfigFileExists();
	void Load();
	void Save();
	void SetDefaults(bool save = true);

private:
	const wxString config_file_path = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + "config.bin";
};