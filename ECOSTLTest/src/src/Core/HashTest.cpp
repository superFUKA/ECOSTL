#include "ECOSTL/src/Core/Hash.h"

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
