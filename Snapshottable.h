#pragma once

#include "Serialization.h"

class Snapshottable
{
public:
	virtual void State(Serialization::BaseFunctor& functor) = 0;
};