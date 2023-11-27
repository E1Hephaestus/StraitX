#ifndef STRAITX_STRING_VIEW_HPP
#define STRAITX_STRING_VIEW_HPP

#include <string>
#include "core/types.hpp"
#include "core/unicode.hpp"
#include "core/span.hpp"
#include "core/move.hpp"
#include "core/printer.hpp"

class StringView {
protected:
	const char *m_String = nullptr;
	size_t m_CodeunitsCount = 0;
public:
	constexpr StringView() = default;
	
	StringView(const std::string &string):
		StringView(string.data(), string.size())
	{}

	constexpr StringView(const char *string, size_t codeunits_count):
		m_String(string),
		m_CodeunitsCount(codeunits_count)
	{}

	constexpr StringView(const char *string):
		m_String(string),
		m_CodeunitsCount(StaticCodeunitsCount(string))
	{}

	constexpr StringView(StringView &&other)noexcept{
		*this = Move(other);
	}

	constexpr StringView(const StringView &) = default;

	constexpr StringView& operator=(StringView &&other)noexcept{
		m_String = other.m_String;
		m_CodeunitsCount = other.m_CodeunitsCount;
		other.m_String = nullptr;
		other.m_CodeunitsCount = 0;
		return *this;
	}

	constexpr StringView &operator=(const StringView &) = default;

	constexpr StringView& operator=(const char* string){
		m_String = string;
		m_CodeunitsCount = StaticCodeunitsCount(string);
		return *this;
	}

	constexpr const char* Data()const{
		return m_String;
	}

	constexpr size_t Size()const{
		return CodeunitsCount();
	}

	constexpr size_t CodeunitsCount()const{
		return m_CodeunitsCount;
	}

	size_t CodepointsCount()const{
		size_t counter = 0;
		for(u32 ch: Unicode())
			counter++;
		return counter;
	}

	
    UnicodeString Unicode()const {
        return UnicodeString(Data(), Size());
    }

	char operator[](size_t size)const {
		return Data()[size];
	}

	constexpr operator ConstSpan<char>()const{
		return {Data(), Size()};
	}

	operator UnicodeString()const {
		return Unicode();
	}

	const char *begin()const{
		return Data();
	}

	const char *end()const{
		return Data() + Size();
	}

	static constexpr size_t StaticCodeunitsCount(const char* string) {
		if(!string)return 0;

		size_t counter = 0;
		while (*string++) 
			counter++;
		return counter;
	}
};

template<>
struct Printer<StringView> {
	static void Print(const StringView& value, StringWriter &writer) {
		writer.Write(value.Data(), value.CodeunitsCount());
	}
};

namespace std {
	template<typename T>
	struct hash;

	template<>
	struct hash<StringView>{
		size_t operator()(StringView view)const {
			unsigned int hash = 5381;

			for (size_t i = 0; i < view.Size(); ++i) {
				hash = ((hash << 5) + hash) + static_cast<unsigned char>(view.Data()[i]);
			}

			return hash;
		}
	};
}

#endif//STRAITX_STRING_VIEW_HPP