#include "core_string.h"
#include <charconv>
#include <vector>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>

// ICU-based helpers: convert between std::wstring (platform wchar_t) and UTF-8
static std::string WideToUTF8(std::wstring_view input)
{
	if (input.empty())
		return {};

	UErrorCode status = U_ZERO_ERROR;

	if (sizeof(wchar_t) == 2)
	{
		const UChar* u16 = reinterpret_cast<const UChar*>(input.data());
		int32_t required = 0;
		u_strToUTF8(nullptr, 0, &required, u16, static_cast<int32_t>(input.size()), &status);
		if (!(status == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(status)))
			return {};
		status = U_ZERO_ERROR;
		std::string out;
		out.resize(required);
		u_strToUTF8(out.data(), required, nullptr, u16, static_cast<int32_t>(input.size()), &status);
		if (U_FAILURE(status))
			return {};
		return out;
	}
	else
	{
		// wchar_t is 4 bytes (UTF-32)
		const UChar32* u32 = reinterpret_cast<const UChar32*>(input.data());
		int32_t u16Len = 0;
		u_strFromUTF32(nullptr, 0, &u16Len, u32, static_cast<int32_t>(input.size()), &status);
		if (!(status == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(status)))
			return {};
		status = U_ZERO_ERROR;
		std::vector<UChar> u16buf(u16Len);
		u_strFromUTF32(u16buf.data(), u16Len, nullptr, u32, static_cast<int32_t>(input.size()), &status);
		if (U_FAILURE(status))
			return {};

		int32_t utf8Len = 0;
		u_strToUTF8(nullptr, 0, &utf8Len, u16buf.data(), u16Len, &status);
		if (!(status == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(status)))
			return {};
		status = U_ZERO_ERROR;
		std::string out;
		out.resize(utf8Len);
		u_strToUTF8(out.data(), utf8Len, nullptr, u16buf.data(), u16Len, &status);
		if (U_FAILURE(status))
			return {};
		return out;
	}
}

static std::wstring UTF8ToWide(std::string_view utf8)
{
	if (utf8.empty())
		return {};

	UErrorCode status = U_ZERO_ERROR;
	int32_t u16Len = 0;
	u_strFromUTF8(nullptr, 0, &u16Len, utf8.data(), static_cast<int32_t>(utf8.size()), &status);
	if (!(status == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(status)))
		return {};
	status = U_ZERO_ERROR;
	std::vector<UChar> u16buf(u16Len);
	u_strFromUTF8(u16buf.data(), u16Len, nullptr, utf8.data(), static_cast<int32_t>(utf8.size()), &status);
	if (U_FAILURE(status))
		return {};

	if (sizeof(wchar_t) == 2)
	{
		std::wstring out;
		out.resize(u16Len);
		for (int32_t i = 0; i < u16Len; ++i)
			out[i] = static_cast<wchar_t>(u16buf[i]);
		return out;
	}
	else
	{
		// convert UTF-16 buffer to UTF-32
		int32_t u32Len = 0;
		u_strToUTF32(nullptr, 0, &u32Len, u16buf.data(), u16Len, &status);
		if (!(status == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(status)))
			return {};
		status = U_ZERO_ERROR;
		std::vector<UChar32> u32buf(u32Len);
		u_strToUTF32(u32buf.data(), u32Len, nullptr, u16buf.data(), u16Len, &status);
		if (U_FAILURE(status))
			return {};

		std::wstring out;
		out.resize(u32Len);
		for (int32_t i = 0; i < u32Len; ++i)
			out[i] = static_cast<wchar_t>(u32buf[i]);
		return out;
	}
}

namespace UTF8
{
	std::string Narrow(std::wstring_view utf16Input)
	{
		return WideToUTF8(utf16Input);
	}

	std::wstring Widen(std::string_view utf8Input)
	{
		return UTF8ToWide(utf8Input);
	}

	std::string FromShiftJIS(std::string_view shiftJISInput)
	{
		// Convert Shift_JIS bytes -> wide -> UTF-8
		return UTF8::Narrow(ShiftJIS::Widen(shiftJISInput));
	}

	WideArg::WideArg(std::string_view utf8Input)
	{
		// Convert UTF-8 to platform wstring then copy into stack/heap buffer
		std::wstring converted = UTF8::Widen(utf8Input);
		convertedLength = static_cast<int>(converted.size());
		if (convertedLength <= 0)
		{
			stackBuffer[0] = L'\0';
			return;
		}

		if (convertedLength < static_cast<int>(std::size(stackBuffer)))
		{
			::memcpy(stackBuffer, converted.data(), sizeof(wchar_t) * convertedLength);
			stackBuffer[convertedLength] = L'\0';
		}
		else
		{
			heapBuffer = std::unique_ptr<wchar_t[]>(new wchar_t[convertedLength + 1]);
			::memcpy(heapBuffer.get(), converted.data(), sizeof(wchar_t) * convertedLength);
			heapBuffer[convertedLength] = L'\0';
		}
	}

	const wchar_t* WideArg::c_str() const
	{
		return (convertedLength < static_cast<int>(std::size(stackBuffer))) ? stackBuffer : heapBuffer.get();
	}
}

namespace ShiftJIS
{
	// NOTE: According to https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
	//		 932 | shift_jis | ANSI/OEM Japanese; Japanese (Shift-JIS)
	// ICU encoding name for Shift-JIS
	static constexpr const char* ICU_SHIFT_JIS = "Shift_JIS";

	std::string Narrow(std::wstring_view utf16Input)
	{
		// Convert wide -> UTF-8, then UTF-8 -> Shift_JIS via ICU
		const std::string utf8 = UTF8::Narrow(utf16Input);
		if (utf8.empty())
			return {};

		UErrorCode status = U_ZERO_ERROR;
		int32_t destCapacity = static_cast<int32_t>(utf8.size()) * 4 + 16;
		std::string out;
		out.resize(destCapacity);
		int32_t actual = ucnv_convert(ICU_SHIFT_JIS, "UTF-8", out.data(), destCapacity, utf8.data(), static_cast<int32_t>(utf8.size()), &status);
		if (U_FAILURE(status))
			return {};
		out.resize(actual);
		return out;
	}

	std::wstring Widen(std::string_view utf8Input)
	{
		// Convert Shift_JIS -> UTF-8 via ICU, then UTF-8 -> wide
		if (utf8Input.empty())
			return {};
		UErrorCode status = U_ZERO_ERROR;
		int32_t destCapacity = static_cast<int32_t>(utf8Input.size()) * 4 + 16;
		std::string tmp;
		tmp.resize(destCapacity);
		int32_t actual = ucnv_convert("UTF-8", ICU_SHIFT_JIS, tmp.data(), destCapacity, utf8Input.data(), static_cast<int32_t>(utf8Input.size()), &status);
		if (U_FAILURE(status))
			return {};
		tmp.resize(actual);
		return UTF8::Widen(tmp);
	}

	std::string FromUTF8(std::string_view utf8Input)
	{
		// Convert UTF-8 to Shift_JIS
		if (utf8Input.empty())
			return {};
		UErrorCode status = U_ZERO_ERROR;
		int32_t destCapacity = static_cast<int32_t>(utf8Input.size()) * 4 + 16;
		std::string out;
		out.resize(destCapacity);
		int32_t actual = ucnv_convert(ICU_SHIFT_JIS, "UTF-8", out.data(), destCapacity, utf8Input.data(), static_cast<int32_t>(utf8Input.size()), &status);
		if (U_FAILURE(status))
			return {};
		out.resize(actual);
		return out;
	}
}

namespace ASCII
{
	template <typename T>
	constexpr b8 TryParsePrimitive(std::string_view string, T& out)
	{
		const std::from_chars_result result = std::from_chars(string.data(), string.data() + string.size(), out, 10);
		const b8 hasNoError = (result.ec == std::errc {});
		const b8 parsedFully = (result.ptr == string.data() + string.size());

		return hasNoError && parsedFully;
	}
	
	template <typename T>
	b8 TryParseFloatingPrimitive(std::string_view string, T& out)
	{
		// 跳过前后空白（如果不想允许空白，也可以去掉这段）
		auto first = string.data();
		auto last  = string.data() + string.size();

		// 建一个以 '\0' 结尾的临时字符串，方便用 C 接口
		std::string tmp(first, last);
		char* endPtr = nullptr;

		errno = 0;
		if constexpr (std::is_same_v<T, float>)
		{
			float value = std::strtof(tmp.c_str(), &endPtr);
			if (endPtr != tmp.c_str() + tmp.size() || errno == ERANGE)
				return false;
			out = value;
			return true;
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			double value = std::strtod(tmp.c_str(), &endPtr);
			if (endPtr != tmp.c_str() + tmp.size() || errno == ERANGE)
				return false;
			out = value;
			return true;
		}
		else
		{
			static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
						  "TryParseFloatingPrimitive only supports float/double");
			return false;
		}
	}

	b8 TryParse(std::string_view string, u32& out) { return TryParsePrimitive(string, out); }
	b8 TryParse(std::string_view string, i32& out) { return TryParsePrimitive(string, out); }
	b8 TryParse(std::string_view string, u64& out) { return TryParsePrimitive(string, out); }
	b8 TryParse(std::string_view string, i64& out) { return TryParsePrimitive(string, out); }
	b8 TryParse(std::string_view string, f32& out) { return TryParseFloatingPrimitive(string, out); }
	b8 TryParse(std::string_view string, f64& out) { return TryParseFloatingPrimitive(string, out); }
	b8 TryParse(std::string_view string, Complex& out) {
		std::istringstream in(std::string{string});
		in >> out;
		return static_cast<bool>(in); // allow eof, reject fail & bad
	}
}
