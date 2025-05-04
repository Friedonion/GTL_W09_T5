#pragma once

#include <string>
#include "CString.h"
#include "ContainerAllocator.h"
#include "Core/HAL/PlatformType.h"

/*
# TCHAR가 ANSICHAR인 경우
1. const ANSICHAR* 로 FString 생성
2. std::string에서 FString 생성

# TCHAR가 WIDECHAR인 경우
1. const ANSICHAR* 로 FString 생성
1. const WIDECHAR* 로 FString 생성
2. std::wstring에서 FString 생성
3. std::string에서 FString 생성
*/

/** Determines case sensitivity options for string comparisons. */
namespace ESearchCase
{
enum Type : uint8
{
    /** Case sensitive. Upper/lower casing must match for strings to be considered equal. */
    CaseSensitive,

    /** Ignore case. Upper/lower casing does not matter when making a comparison. */
    IgnoreCase,
};
}

/** Determines search direction for string operations. */
namespace ESearchDir
{
enum Type : uint8
{
    /** Search from the start, moving forward through the string. */
    FromStart,

    /** Search from the end, moving backward through the string. */
    FromEnd,
};
}

class FString
{
public:
    using ElementType = TCHAR;

private:
    using BaseStringType = std::basic_string<
        ElementType,
        std::char_traits<ElementType>,
        FDefaultAllocator<ElementType>
    >;

    BaseStringType PrivateString;

	friend struct std::hash<FString>;
    friend ElementType* GetData(FString&);
    friend const ElementType* GetData(const FString&);

public:
    BaseStringType& GetContainerPrivate() { return PrivateString; }
    const BaseStringType& GetContainerPrivate() const { return PrivateString; }
    explicit operator std::string() const { return std::string(PrivateString); }

    FString() = default;
    ~FString() = default;

    FString(const FString&) = default;
    FString& operator=(const FString&) = default;
    FString(FString&&) = default;
    FString& operator=(FString&&) = default;

    FString(BaseStringType InString) : PrivateString(std::move(InString)) {}

#if USE_WIDECHAR
private:
    static std::wstring ConvertToWideChar(const ANSICHAR* NarrowStr);

public:
    FString(const std::wstring& InString) : PrivateString(InString) {}
    FString(const std::string& InString) : PrivateString(ConvertToWideChar(InString.c_str())) {}
    FString(const WIDECHAR* InString) : PrivateString(InString) {}
    FString(const ANSICHAR* InString) : PrivateString(ConvertToWideChar(InString)) {}
#else
public:
    FString(const std::string& InString) : PrivateString(InString) {}
    FString(const ANSICHAR* InString) : PrivateString(InString) {}
    
    explicit FString(const std::wstring& InString) : FString(InString.c_str()) {}
    explicit FString(const WIDECHAR* InString)
    {
        if (!InString) // Null 체크
        {
            PrivateString = "";
            return;
        }

        // Wide 문자열을 UTF-8 기반의 narrow 문자열로 변환
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, InString, -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded <= 0) // 변환 실패 또는 빈 문자열
        {
            PrivateString = "";
            return;
        }

        // sizeNeeded는 널 종료 문자를 포함한 길이입니다.
        std::string narrowStr(sizeNeeded - 1, 0); // 널 문자 제외한 크기로 할당
        WideCharToMultiByte(CP_UTF8, 0, InString, -1, &narrowStr[0], sizeNeeded, nullptr, nullptr);

        PrivateString = narrowStr; // 변환된 문자열로 내부 데이터 초기화
    }
#endif

#if USE_WIDECHAR
	FORCEINLINE std::string ToAnsiString() const
	{
		// Wide 문자열을 UTF-8 기반의 narrow 문자열로 변환
		if (PrivateString.empty())
		{
			return std::string();
		}
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, PrivateString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (sizeNeeded <= 0)
		{
			return std::string();
		}
		std::string result(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, PrivateString.c_str(), -1, &result[0], sizeNeeded, nullptr, nullptr);
		return result;
	}
#else
	FORCEINLINE std::wstring ToWideString() const
	{
#if USE_WIDECHAR
		return PrivateString;
#else
        // Narrow 문자열을 UTF-8로 가정하고 wide 문자열로 변환
        if (PrivateString.empty())
        {
            return std::wstring();
        }
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, PrivateString.c_str(), -1, nullptr, 0);
        if (sizeNeeded <= 0)
        {
            return std::wstring();
        }
        // sizeNeeded에는 널 문자를 포함한 길이가 들어 있음
        std::wstring wstr(sizeNeeded - 1, 0); // 널 문자를 제외한 크기로 초기화
        MultiByteToWideChar(CP_UTF8, 0, PrivateString.c_str(), -1, wstr.data(), sizeNeeded);
        return wstr;
#endif
	}
#endif
	template <typename Number>
		requires std::is_integral_v<Number>
    static FString FromInt(Number Num);

    static FString SanitizeFloat(float InFloat);

	static float ToFloat(const FString& InString);
    
    static int ToInt(const FString& InString);

    /**
     * 문자열 내용을 기반으로 bool 값을 반환합니다.
     */
    bool ToBool() const
    {
        // 빈 문자열은 false로 처리
        if (IsEmpty())
        {
            return false;
        }

        // 가장 일반적인 경우: "true" 또는 "1" (대소문자 무관)
        // Equals 함수가 이미 대소문자 무시 비교를 지원하므로 활용합니다.
        if (Equals(TEXT("true"), ESearchCase::IgnoreCase))
        {
            return true;
        }
        if (Equals(TEXT("1"))) // "1"은 대소문자 구분이 의미 없음
        {
            return true;
        }

        // 그 외: "false" 또는 "0" (대소문자 무관)
        // 이 경우들도 명시적으로 false를 반환하는 것이 안전합니다.
        if (Equals(TEXT("false"), ESearchCase::IgnoreCase))
        {
            return false;
        }
        if (Equals(TEXT("0"))) // "0"도 대소문자 구분이 의미 없음
        {
            return false;
        }

        // 위 조건에 해당하지 않는 모든 다른 문자열은 false로 처리합니다.
        // (예: "Yes", "No", "On", "Off" 등을 추가로 지원하고 싶다면 여기에 조건을 추가할 수 있습니다.)
        // UE_LOG(LogTemp, Warning, TEXT("FString::ToBool() : Unrecognized string '%s' treated as false."), **this); // 필요시 경고 로그
        return false;
    }

    /**
     * 이 문자열의 시작 부분에서 Count개의 문자를 제외한 나머지를 복사하여 반환합니다.
     * @param Count 제거할 앞부분 문자의 개수.
     * @return 시작 부분이 제거된 새로운 FString 객체. Count가 0보다 작거나 같으면 원본 복사본을,
     *         Count가 문자열 길이보다 크거나 같으면 빈 문자열을 반환합니다.
     */
    FString RightChop(int32 Count) const;
    FString Left(int32 Count) const;
    FString Replace(const FString& From, const FString& To) const;

public:
    FORCEINLINE int32 Len() const;
    FORCEINLINE bool IsEmpty() const;

    /** 배열의 모든 요소를 지웁니다. */
    void Empty();

    /**
     * 문자열이 서로 같은지 비교합니다.
     * @param Other 비교할 String
     * @param SearchCase 대소문자 구분
     * @return 같은지 여부
     */
    bool Equals(const FString& Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const;

    /**
     * 문자열이 겹치는지 확인합니다.
     * @param SubStr 찾을 문자열
     * @param SearchCase 대소문자 구분
     * @param SearchDir 찾을 방향
     * @return 문자열 겹침 여부
     */
    bool Contains(
        const FString& SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
        ESearchDir::Type SearchDir = ESearchDir::FromStart
    ) const;

    /**
     * 문자열을 찾아 Index를 반홥합니다.
     * @param SubStr 찾을 문자열
     * @param SearchCase 대소문자 구분
     * @param SearchDir 찾을 방향
     * @param StartPosition 시작 위치
     * @return 찾은 문자열의 Index를 반환합니다. 찾지 못하면 -1
     */
    int32 Find(
        const FString& SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
        ESearchDir::Type SearchDir = ESearchDir::FromStart, int32 StartPosition = -1
    ) const;

    void Reserve(int32 CharacterCount);
    void Resize(int32 CharacterCount);

    [[nodiscard]] FString ToUpper() const &;
    [[nodiscard]] FString ToUpper() &&;
    void ToUpperInline();

    [[nodiscard]] FString ToLower() const &;
    [[nodiscard]] FString ToLower() &&;
    void ToLowerInline();

public:
    /** ElementType* 로 반환하는 연산자 */
    FORCEINLINE const ElementType* operator*() const;

    FORCEINLINE FString& operator+=(const FString& SubStr);
    FORCEINLINE friend FString operator+(const FString& Lhs, const FString& Rhs);

    FORCEINLINE bool operator==(const FString& Rhs) const;
    FORCEINLINE bool operator==(const ElementType* Rhs) const;
    FORCEINLINE ElementType& operator[](int32 Index)
    {
        return PrivateString[Index];
    }

    FORCEINLINE const ElementType& operator[](int32 Index) const
    {
        return PrivateString[Index];
    }
public:
    // --- Printf 함수 ---
    /**
     * @brief 가변 인자를 사용하여 포맷팅된 FString을 생성합니다. printf와 유사하게 동작합니다.
     * @param Format 포맷 문자열 (TCHAR*).
     * @param ... 포맷 문자열에 대응하는 가변 인자.
     * @return 포맷팅된 새로운 FString 객체.
     */
    static FString Printf(const ElementType* Format, ...);
};

template <typename Number>
	requires std::is_integral_v<Number>
FString FString::FromInt(Number Num)
{
#if USE_WIDECHAR
    return FString{std::to_wstring(Num)};
#else
    return FString{std::to_string(Num)};
#endif
}

FORCEINLINE int32 FString::Len() const
{
    return static_cast<int32>(PrivateString.length());
}

FORCEINLINE bool FString::IsEmpty() const
{
    return PrivateString.empty();
}

FORCEINLINE const FString::ElementType* FString::operator*() const
{
    return PrivateString.c_str();
}

FString operator+(const FString& Lhs, const FString& Rhs)
{
    FString CopyLhs{Lhs};
    return CopyLhs += Rhs;
}

FORCEINLINE bool FString::operator==(const FString& Rhs) const
{
    return Equals(Rhs, ESearchCase::IgnoreCase);
}

FORCEINLINE bool FString::operator==(const ElementType* Rhs) const
{
    return Equals(Rhs);
}

FORCEINLINE FString& FString::operator+=(const FString& SubStr)
{
    this->PrivateString += SubStr.PrivateString;
    return *this;
}

template<>
struct std::hash<FString>
{
	size_t operator()(const FString& Key) const noexcept
	{
		return hash<FString::BaseStringType>()(Key.PrivateString);
	}
};


inline FString::ElementType* GetData(FString& String)
{
    return String.PrivateString.data();
}

inline const FString::ElementType* GetData(const FString& String)
{
    return String.PrivateString.data();
}
