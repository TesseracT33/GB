#pragma once

#include <wx/msgdlg.h>

#include <fstream>
#include <string>

#include "Types.h"

namespace Util
{
	inline bool CheckBit(u8  byte, u8 pos) { return byte & (1u << pos); }
	inline bool CheckBit(u16 word, u8 pos) { return word & (1u << pos); }
	inline u8   SetBit(u8  byte, u8 pos) { return byte | (1u << pos); }
	inline u16  SetBit(u16 word, u8 pos) { return word | (1u << pos); }
	inline u8   ClearBit(u8  byte, u8 pos) { return byte & ~(1u << pos); }
	inline u16  ClearBit(u16 word, u8 pos) { return word & ~(1u << pos); }
	inline u8   ToggleBit(u8  byte, u8 pos) { return byte ^ ~(1u << pos); }
	inline u16  ToggleBit(u16 word, u8 pos) { return word ^ ~(1u << pos); }

	inline bool CheckBit(u8* byte, u8 pos) { return *byte & (1u << pos); }
	inline bool CheckBit(u16* word, u8 pos) { return *word & (1u << pos); }
	inline void SetBit(u8* byte, u8 pos) { *byte |= (1u << pos); }
	inline void SetBit(u16* word, u8 pos) { *word |= (1u << pos); }
	inline void ClearBit(u8* byte, u8 pos) { *byte &= ~(1u << pos); }
	inline void ClearBit(u16* word, u8 pos) { *word &= ~(1u << pos); }
	inline void ToggleBit(u8* byte, u8 pos) { *byte ^= ~(1u << pos); }
	inline void ToggleBit(u16* word, u8 pos) { *word ^= ~(1u << pos); }

	const size_t max_str_length_for_serialization = 4096;

	inline bool SerializeSTDString(const std::string& str, std::ofstream& ofs)
	{
		const char* c_str = str.c_str();
		size_t str_len = std::strlen(c_str);
		if (str_len > max_str_length_for_serialization)
		{
			wxMessageBox(wxString::Format("Error: cannot serialize a string longer than %i characters.", max_str_length_for_serialization));
			return false;
		}
		ofs.write((char*)&str_len, sizeof(str_len));
		ofs.write(c_str, str_len * sizeof(char));
		return true;
	}

	inline void DeserializeSTDString(std::string& str, std::ifstream& ifs)
	{
		size_t str_len = 0;
		ifs.read((char*)&str_len, sizeof(str_len));
		char c_str[max_str_length_for_serialization]{};
		ifs.read(c_str, str_len * sizeof(char));
		str = std::string(c_str);
	}
}