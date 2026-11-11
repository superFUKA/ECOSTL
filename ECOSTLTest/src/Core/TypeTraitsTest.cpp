// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/Core/Template/TypeTraits.h"

#include <gtest/gtest.h>

#include "ECOSTL/Core/Template/TypeTraits.h"

namespace
{
	struct Base {};
	struct Derived : Base {};
	struct PrivateDerived : private Base {};

	struct FromInt
	{
		FromInt(int) {}
	};
	struct ExplicitFromInt
	{
		explicit ExplicitFromInt(int) {}
	};

	template<typename T, utl::EnableIfType<utl::IsSameV<T, int>, int> = 0>
	constexpr bool OnlyInt(T)
	{
		return true;
	}
	template<typename T, utl::EnableIfType<!utl::IsSameV<T, int>, int> = 0>
	constexpr bool OnlyInt(T)
	{
		return false;
	}
}

//----------------------------------------
// 基本
//----------------------------------------

// IntegralConstant・TrueType・FalseType は、継承ゼロの方針で無くした（判定は変数テンプレート「〜V」で行う）
static_assert(!utl::DependentFalseV<int>);
static_assert(utl::IsSameV<utl::TypeIdentityType<int>, int>);
static_assert(utl::IsSameV<utl::NullptrT, decltype(nullptr)>);

//----------------------------------------
// 判定
//----------------------------------------

static_assert(utl::IsSameV<int, int>);
static_assert(!utl::IsSameV<int, const int>);
static_assert(!utl::IsSameV<int, int&>);

static_assert(utl::IsLvalueReferenceV<int&>);
static_assert(utl::IsLvalueReferenceV<const int&>);
static_assert(!utl::IsLvalueReferenceV<int>);
static_assert(!utl::IsLvalueReferenceV<int&&>);

static_assert(utl::IsUnsignedV<unsigned int>);
static_assert(utl::IsUnsignedV<const unsigned char>);
static_assert(utl::IsUnsignedV<unsigned long long>);
static_assert(!utl::IsUnsignedV<int>);
static_assert(!utl::IsUnsignedV<signed char>);
static_assert(!utl::IsUnsignedV<float>);
// 整数でない型は false(コンパイルエラーにならない)
static_assert(!utl::IsUnsignedV<Base>);

//----------------------------------------
// 変換
//----------------------------------------

static_assert(utl::IsSameV<utl::RemoveReferenceType<int>, int>);
static_assert(utl::IsSameV<utl::RemoveReferenceType<int&>, int>);
static_assert(utl::IsSameV<utl::RemoveReferenceType<int&&>, int>);
static_assert(utl::IsSameV<utl::RemoveReferenceType<const int&>, const int>);

static_assert(utl::IsSameV<utl::RemoveConstType<const int>, int>);
static_assert(utl::IsSameV<utl::RemoveConstType<const volatile int>, volatile int>);
static_assert(utl::IsSameV<utl::RemoveVolatileType<volatile int>, int>);
static_assert(utl::IsSameV<utl::RemoveCVType<const volatile int>, int>);
static_assert(utl::IsSameV<utl::RemoveCVType<int>, int>);
// ポインタの先のconstは外さない
static_assert(utl::IsSameV<utl::RemoveCVType<const int*>, const int*>);
static_assert(utl::IsSameV<utl::RemoveCVType<int* const>, int*>);

static_assert(utl::IsSameV<utl::AddRvalueReferenceType<int>, int&&>);
// 参照の折りたたみで左辺値参照のまま
static_assert(utl::IsSameV<utl::AddRvalueReferenceType<int&>, int&>);
// voidは参照にできないのでそのまま
static_assert(utl::IsSameV<utl::AddRvalueReferenceType<void>, void>);

static_assert(utl::IsSameV<decltype(utl::DeclVal<int>()), int&&>);
static_assert(utl::IsSameV<decltype(utl::DeclVal<int&>()), int&>);

//----------------------------------------
// 条件
//----------------------------------------

static_assert(utl::IsSameV<utl::EnableIfType<true>, void>);
static_assert(utl::IsSameV<utl::EnableIfType<true, int>, int>);
static_assert(OnlyInt(1));
static_assert(!OnlyInt(1.0));

static_assert(utl::IsSameV<utl::ConditionalType<true, int, double>, int>);
static_assert(utl::IsSameV<utl::ConditionalType<false, int, double>, double>);

//----------------------------------------
// 変換できるか
//----------------------------------------

static_assert(utl::IsConvertibleV<int, double>);
static_assert(utl::IsConvertibleV<Derived*, Base*>);
static_assert(!utl::IsConvertibleV<Base*, Derived*>);
// privateな継承では変換できない
static_assert(!utl::IsConvertibleV<PrivateDerived*, Base*>);
static_assert(utl::IsConvertibleV<Derived*, const Base*>);
static_assert(!utl::IsConvertibleV<const Derived*, Base*>);
static_assert(utl::IsConvertibleV<int, FromInt>);
// explicitなコンストラクタでは暗黙に変換できない
static_assert(!utl::IsConvertibleV<int, ExplicitFromInt>);
static_assert(utl::IsConvertibleV<int&, const int&>);
// 右辺値は左辺値参照に渡せない
static_assert(!utl::IsConvertibleV<int, int&>);
static_assert(utl::IsConvertibleV<void, void>);
static_assert(!utl::IsConvertibleV<int, void>);
static_assert(!utl::IsConvertibleV<void, int>);

TEST(TypeTraits, Compiles) {
	// 中身はstatic_assertで確かめている
	SUCCEED();
}
