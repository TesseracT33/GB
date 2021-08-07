#pragma once

#include <fstream>

#include "Serialization.h"

class Configurable
{
public:
	virtual void Configure(Serialization::BaseFunctor& functor) = 0;
	virtual void SetDefaultConfig() = 0;
};

