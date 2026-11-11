#include "ECOSTL/Core/Hash/Hash.h"

#include <gtest/gtest.h>

TEST(Hash, StringIsConstexprAndMatchesBytes) {
	constexpr utl::Hash64 h = utl::Hash64FromString("abc");
	const uint8_t bytes[] = { 'a', 'b', 'c' };
	EXPECT_EQ(h, utl::Hash64FromBytes(bytes, 3));
	EXPECT_NE(h, utl::Hash64FromString("abd"));
}

TEST(Hash, EmptyIsOffset) {
	EXPECT_EQ(utl::Hash64FromString(""), utl::HASH_OFFSET);
}

TEST(Hash, TypeAndTypes) {
	const int a = 1, b = 2;
	EXPECT_EQ(utl::Hash64FromType(a), utl::Hash64FromType(1));
	EXPECT_NE(utl::Hash64FromType(a), utl::Hash64FromType(b));
	EXPECT_EQ(utl::Hash64FromTypes(a, b), utl::Hash64FromType(b, utl::Hash64FromType(a)));
}

TEST(Hash, Hash32MixMatchesTestGame) {
	// testGame の gameLib::HashU32 と同じ値になる
	static_assert(utl::Hash32Mix(0u) == 0u);
	EXPECT_EQ(utl::Hash32Mix(1u), 0x688990c0u);
	EXPECT_EQ(utl::Hash32Mix(2u), 0xd1132181u);
	EXPECT_EQ(utl::Hash32Mix(12345u), 0x912efcf7u);
	EXPECT_EQ(utl::Hash32Mix(0xffffffffu), 0x6768824au);
}
