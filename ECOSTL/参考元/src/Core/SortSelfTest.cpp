#include "Sort.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{
	struct Record
	{
		int key;
		int order;
	};

	template<typename T>
	bool IsAscending(const T* values, const uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
		{
			if (values[i - 1] > values[i])
			{
				return false;
			}
		}
		return true;
	}

	template<typename T>
	bool IsDescending(const T* values, const uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
		{
			if (values[i - 1] < values[i])
			{
				return false;
			}
		}
		return true;
	}

	bool IsAscendingByKey(const Record* values, const uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
		{
			if (values[i - 1].key > values[i].key)
			{
				return false;
			}
		}
		return true;
	}

	bool IsDescendingByKey(const Record* values, const uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
		{
			if (values[i - 1].key < values[i].key)
			{
				return false;
			}
		}
		return true;
	}

	bool HasStableOrder(const Record* values, const uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
		{
			if (values[i - 1].key == values[i].key && values[i - 1].order > values[i].order)
			{
				return false;
			}
		}
		return true;
	}

	bool HasStableAbsOrder(const int* values, const int* payload, const uint32_t count)
	{
		for (uint32_t i = 1; i < count; ++i)
		{
			const int lhsAbs = std::abs(values[i - 1]);
			const int rhsAbs = std::abs(values[i]);
			if (lhsAbs == rhsAbs && payload[i - 1] > payload[i])
			{
				return false;
			}
		}
		return true;
	}

	void Expect(bool condition, const char* name, bool& allPassed)
	{
		std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
		if (!condition)
		{
			allPassed = false;
		}
	}

	void TestPublicSort(bool& allPassed)
	{
		int values[] = { 9, 3, 7, 1, 8, 2, 6, 4, 5 };
		utl::Sort(values, 9, false);
		Expect(IsAscending(values, 9), "Sort ascending", allPassed);

		int descValues[] = { 9, 3, 7, 1, 8, 2, 6, 4, 5 };
		utl::Sort(descValues, 9, true);
		Expect(IsDescending(descValues, 9), "Sort descending", allPassed);
	}

	void TestSortArrayWithPayload(bool& allPassed)
	{
		int keys[] = { 40, 10, 30, 20, 50 };
		char payload[] = { 'd', 'a', 'c', 'b', 'e' };
		utl::SortArray(keys, 5, false, payload);
		const bool sorted = IsAscending(keys, 5);
		const bool payloadAligned = payload[0] == 'a' && payload[1] == 'b' && payload[2] == 'c' && payload[3] == 'd' && payload[4] == 'e';
		Expect(sorted && payloadAligned, "SortArray keeps payload aligned", allPassed);

		int descKeys[] = { 40, 10, 30, 20, 50 };
		char descPayload[] = { 'd', 'a', 'c', 'b', 'e' };
		utl::SortArray(descKeys, 5, true, descPayload);
		const bool descSorted = IsDescending(descKeys, 5);
		const bool descAligned = descPayload[0] == 'e' && descPayload[1] == 'd' && descPayload[2] == 'c' && descPayload[3] == 'b' && descPayload[4] == 'a';
		Expect(descSorted && descAligned, "SortArray descending keeps payload aligned", allPassed);
	}

	void TestComparatorSort(bool& allPassed)
	{
		auto absLess = [](const int a, const int b) { return std::abs(a) < std::abs(b); };
		int values[] = { -5, 2, -1, 4, -3, 1 };
		utl::Sort(values, 6, absLess, false);
		const bool ordered =
			std::abs(values[0]) <= std::abs(values[1]) &&
			std::abs(values[1]) <= std::abs(values[2]) &&
			std::abs(values[2]) <= std::abs(values[3]) &&
			std::abs(values[3]) <= std::abs(values[4]) &&
			std::abs(values[4]) <= std::abs(values[5]);
		Expect(ordered, "Sort comparator ascending", allPassed);

		int descValues[] = { -5, 2, -1, 4, -3, 1 };
		utl::Sort(descValues, 6, absLess, true);
		const bool descOrdered =
			std::abs(descValues[0]) >= std::abs(descValues[1]) &&
			std::abs(descValues[1]) >= std::abs(descValues[2]) &&
			std::abs(descValues[2]) >= std::abs(descValues[3]) &&
			std::abs(descValues[3]) >= std::abs(descValues[4]) &&
			std::abs(descValues[4]) >= std::abs(descValues[5]);
		Expect(descOrdered, "Sort comparator descending", allPassed);
	}

	void TestMemberSort(bool& allPassed)
	{
		Record values[] = { { 4, 0 }, { 1, 1 }, { 3, 2 }, { 2, 3 } };
		utl::SortMember(values, 4, &Record::key, false);
		Expect(IsAscendingByKey(values, 4), "SortMember ascending", allPassed);

		Record descValues[] = { { 4, 0 }, { 1, 1 }, { 3, 2 }, { 2, 3 } };
		utl::SortMember(descValues, 4, &Record::key, true);
		Expect(IsDescendingByKey(descValues, 4), "SortMember descending", allPassed);

		auto parityThenKey = [](const int a, const int b)
		{
			const int parityA = a % 2;
			const int parityB = b % 2;
			return parityA == parityB ? a < b : parityA < parityB;
		};
		Record cmpValues[] = { { 5, 0 }, { 2, 1 }, { 3, 2 }, { 4, 3 }, { 1, 4 } };
		utl::SortMemberCmp(cmpValues, 5, &Record::key, parityThenKey, false);
		const bool ordered = cmpValues[0].key == 2 && cmpValues[1].key == 4 && cmpValues[2].key == 1 && cmpValues[3].key == 3 && cmpValues[4].key == 5;
		Expect(ordered, "SortMember comparator", allPassed);
	}

	void TestStableSort(bool& allPassed)
	{
		Record values[] = { { 2, 0 }, { 1, 1 }, { 2, 2 }, { 1, 3 }, { 2, 4 }, { 1, 5 } };
		utl::StableSortMember(values, 6, &Record::key, false);
		Expect(IsAscendingByKey(values, 6) && HasStableOrder(values, 6), "StableSortMember preserves order", allPassed);

		Record descValues[] = { { 2, 0 }, { 1, 1 }, { 2, 2 }, { 1, 3 }, { 2, 4 }, { 1, 5 } };
		utl::StableSortMember(descValues, 6, &Record::key, true);
		const bool descOrdered =
			IsDescendingByKey(descValues, 6) &&
			descValues[0].order == 0 && descValues[1].order == 2 && descValues[2].order == 4 &&
			descValues[3].order == 1 && descValues[4].order == 3 && descValues[5].order == 5;
		Expect(descOrdered, "StableSortMember descending preserves order", allPassed);

		int keys[] = { -2, 2, -1, 1, -2, 2 };
		int payload[] = { 0, 1, 2, 3, 4, 5 };
		auto absLess = [](const int a, const int b) { return std::abs(a) < std::abs(b); };
		utl::StableSortArrayCmp(keys, 6, absLess, false, payload);
		const bool ordered =
			std::abs(keys[0]) <= std::abs(keys[1]) &&
			std::abs(keys[1]) <= std::abs(keys[2]) &&
			std::abs(keys[2]) <= std::abs(keys[3]) &&
			std::abs(keys[3]) <= std::abs(keys[4]) &&
			std::abs(keys[4]) <= std::abs(keys[5]);
		Expect(ordered && HasStableAbsOrder(keys, payload, 6), "StableSortArrayCmp preserves order", allPassed);
	}

	void TestForcedFallback(bool& allPassed)
	{
		int values[] = { 9, 3, 15, 1, 14, 7, 11, 5, 13, 2, 16, 4, 12, 6, 10, 8, 17 };
		utl::detail::IntroSortComp(values, 0, 16, 0, utl::detail::DefaultLess{}, false);
		Expect(IsAscending(values, 17), "IntroSort fallback ascending", allPassed);

		int descValues[] = { 9, 3, 15, 1, 14, 7, 11, 5, 13, 2, 16, 4, 12, 6, 10, 8, 17 };
		utl::detail::IntroSortComp(descValues, 0, 16, 0, utl::detail::DefaultLess{}, true);
		Expect(IsDescending(descValues, 17), "IntroSort fallback descending", allPassed);

		Record memberValues[] =
		{
			{ 9, 0 }, { 3, 1 }, { 15, 2 }, { 1, 3 }, { 14, 4 }, { 7, 5 }, { 11, 6 }, { 5, 7 }, { 13, 8 },
			{ 2, 9 }, { 16, 10 }, { 4, 11 }, { 12, 12 }, { 6, 13 }, { 10, 14 }, { 8, 15 }, { 17, 16 }
		};
		utl::detail::IntroSortMemberComp(memberValues, 0, 16, 0, &Record::key, utl::detail::DefaultLess{}, false);
		Expect(IsAscendingByKey(memberValues, 17), "IntroSortMember fallback ascending", allPassed);

		Record memberDescValues[] =
		{
			{ 9, 0 }, { 3, 1 }, { 15, 2 }, { 1, 3 }, { 14, 4 }, { 7, 5 }, { 11, 6 }, { 5, 7 }, { 13, 8 },
			{ 2, 9 }, { 16, 10 }, { 4, 11 }, { 12, 12 }, { 6, 13 }, { 10, 14 }, { 8, 15 }, { 17, 16 }
		};
		utl::detail::IntroSortMemberComp(memberDescValues, 0, 16, 0, &Record::key, utl::detail::DefaultLess{}, true);
		Expect(IsDescendingByKey(memberDescValues, 17), "IntroSortMember fallback descending", allPassed);
	}
}

int main()
{
	bool allPassed = true;

	TestPublicSort(allPassed);
	TestSortArrayWithPayload(allPassed);
	TestComparatorSort(allPassed);
	TestMemberSort(allPassed);
	TestStableSort(allPassed);
	TestForcedFallback(allPassed);

	return allPassed ? 0 : 1;
}
