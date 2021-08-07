#include "Serialization.h"

std::ofstream Serialization::ofs{};
std::ifstream Serialization::ifs{};
Serialization::SerializeFunctor Serialization::serialize_functor_inst{ ofs };
Serialization::DeserializeFunctor Serialization::deserialize_functor_inst{ ifs };