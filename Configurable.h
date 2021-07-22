#pragma once

#include <fstream>

class Configurable
{
public:
	virtual void LoadConfig(std::ifstream& ifs) = 0;
	virtual void SaveConfig(std::ofstream& ofs) = 0;
	virtual void SetDefaultConfig() = 0;
};

