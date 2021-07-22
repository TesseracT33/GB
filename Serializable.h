#pragma once
#include <fstream>

class Serializable
{
public:
	virtual void Serialize(std::ofstream& ofs) = 0;
	virtual void Deserialize(std::ifstream& ifs) = 0;
};

