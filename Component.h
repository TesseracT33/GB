#pragma once

#include "Configurable.h"
#include "Snapshottable.h"

class Component : public Snapshottable, public Configurable
{
public:
	virtual void State(Serialization::BaseFunctor& functor) override {};
	virtual void Configure(Serialization::BaseFunctor& functor) override {};
	virtual void SetDefaultConfig() override {};
};