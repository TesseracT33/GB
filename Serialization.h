#pragma once

#include <fstream>
#include <queue>
#include <string>
#include <typeinfo>
#include <vector>

class Serialization
{
public:
	// A functor which can either do serialization via an ofstream, or deserialization via an ifstream
	struct BaseFunctor
	{
		virtual void fun(void* obj, size_t size) = 0;
	};

	struct SerializeFunctor final : public BaseFunctor
	{
		std::ofstream& ofs;

		SerializeFunctor(std::ofstream& _ofs) : ofs(_ofs) {};

		void fun(void* obj, size_t size) override { ofs.write((const char*)obj, size); };
	};

	struct DeserializeFunctor final : public BaseFunctor
	{
		std::ifstream& ifs;

		DeserializeFunctor(std::ifstream& _ifs) : ifs(_ifs) {};

		void fun(void* obj, size_t size) override { ifs.read((char*)obj, size); };
	};

	// serialize or deserialize an std::string (depending on the functor)
	static void STD_string(Serialization::BaseFunctor& functor, std::string& string)
	{
		if (typeid(functor).hash_code() == typeid(serialize_functor_inst).hash_code())
		{
			// serialization
			const char* c_str = string.c_str();
			size_t size = std::strlen(c_str);
			functor.fun(&size, sizeof(size_t));
			functor.fun((void*)c_str, size * sizeof(char));
		}
		else
		{
			// deserialization
			size_t size;
			functor.fun(&size, sizeof(size_t));
			char* c_str = new char[size];
			functor.fun(c_str, size * sizeof(char));
			string = std::string(c_str);
			delete[] c_str;
		}
	}

	// serialize or deserialize an std::vector (depending on the functor)
	template<typename T>
	static void STD_vector(Serialization::BaseFunctor& functor, std::vector<T>& vector)
	{
		if (typeid(functor).hash_code() == typeid(serialize_functor_inst).hash_code())
		{
			// serialization
			size_t size = vector.size();
			functor.fun(&size, sizeof(size_t));
			for (size_t i = 0; i < size; i++)
				functor.fun(&vector[i], sizeof(T));
		}
		else
		{
			// deserialization
			vector.clear();

			size_t size;
			functor.fun(&size, sizeof(size_t));
			vector.resize(size);
			for (size_t i = 0; i < size; i++)
			{
				T t;
				functor.fun(&t, sizeof(T));
				vector.push_back(t);
			}
		}
	}

	// serialize or deserialize an std::queue (depending on the functor)
	template<typename T>
	static void STD_queue(Serialization::BaseFunctor& functor, std::queue<T>& queue)
	{
		if (typeid(functor).hash_code() == typeid(serialize_functor_inst).hash_code())
		{
			// serialization
			size_t size = queue.size();
			functor.fun(&size, sizeof(size_t));
			while (!queue.empty())
			{
				T t = queue.front();
				queue.pop();
				functor.fun(&t, sizeof(T));
			}
		}
		else
		{
			// deserialization
			while (!queue.empty())
				queue.pop();

			size_t size;
			functor.fun(&size, sizeof(size_t));
			for (size_t i = 0; i < size; i++)
			{
				T t;
				functor.fun(&t, sizeof(T));
				queue.push(t);
			}
		}
	}

private:
	// These are only used for comparing the hashcode of an instance of a supplied 'BaseFunctor' in any of the above serialization functions,
	// to either 'SerializeFunctor' or 'DeserializeFunctor'
	static std::ofstream ofs;
	static std::ifstream ifs;
	static SerializeFunctor serialize_functor_inst;
	static DeserializeFunctor deserialize_functor_inst;
};