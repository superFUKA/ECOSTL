#include <gtest/gtest.h>

#include "ECOSTL/src/Type/TypeID.h"

TEST(TypeID, SameTypeSameID) {
    utl::TypeIDAllocator allocator;
    EXPECT_EQ(allocator.GetID<int>(), allocator.GetID<int>());
    EXPECT_EQ(utl::GetTypeID<int>(allocator), utl::GetTypeID<int>(allocator));
    EXPECT_EQ(utl::GetTypeID<int>(), utl::GetTypeID<int>());
}

TEST(TypeID, DifferentTypeDifferentID) {
    utl::TypeIDAllocator allocator;
    const utl::TypeID intID = allocator.GetID<int>();
    const utl::TypeID intFloatID = allocator.GetID<int, float>();
    EXPECT_NE(intID, allocator.GetID<float>());
    EXPECT_NE(intID, intFloatID);
    EXPECT_NE(utl::GetTypeID<int>(), utl::GetTypeID<float>());
}

TEST(TypeID, NeverInvalid) {
    utl::TypeIDAllocator allocator;
    EXPECT_NE(allocator.GetID<int>(), utl::INVALID_TYPE_ID);
    EXPECT_NE(utl::GetTypeID<double>(), utl::INVALID_TYPE_ID);
}

TEST(TypeID, KeyIsTypeNameOnly) {
    utl::TypeIDAllocator a;
    utl::TypeIDAllocator b;
    // 呼び出し元が違っても、型の組が同じなら同じキーになる
    EXPECT_EQ(a.GetID<int>(), 1u);
    EXPECT_EQ(b.GetID<float>(), 1u);
    EXPECT_EQ(a.GetID<float>(), 2u);
}

TEST(TypeID, OrderOfTypesMatters) {
    utl::TypeIDAllocator allocator;
    const utl::TypeID ab = allocator.GetID<int, float>();
    const utl::TypeID ba = allocator.GetID<float, int>();
    EXPECT_NE(ab, ba);
}

TEST(TypeID, AllocatorsAreIndependent) {
    utl::TypeIDAllocator a;
    utl::TypeIDAllocator b;
    a.GetID<int>();
    EXPECT_EQ(b.GetID<double>(), 1u);
}
