#pragma once
#include "src/Core/Core.hpp"
#include "src/Core/Template/Template.h"
#include "src/Core/Sort.h"
#include <cstdint>
#include <type_traits>
#include <cstring>

namespace utl
{
	namespace detail
	{
		template<typename T>
		struct DependentFalse;

		struct HashMapDefaultTag {};
	}

	// HashMap用のデフォルトハッシュ
	template<typename KeyType>
	struct HashMapDefaultPolicy
	{
		static constexpr Hash64 Mix64(Hash64 x)
		{
			// splitmix64 finalizer: integer/ID keyの低品質分布を均す
			x ^= (x >> 30);
			x *= 0xbf58476d1ce4e5b9ull;
			x ^= (x >> 27);
			x *= 0x94d049bb133111ebull;
			x ^= (x >> 31);
			return x;
		}

		Hash64 operator()(const KeyType& key) const
		{
			if constexpr (std::is_same_v<KeyType, Hash64>)
			{
				return Mix64(key);
			}
			else if constexpr (std::is_integral_v<KeyType> || std::is_enum_v<KeyType>)
			{
				uint64_t x = static_cast<uint64_t>(key);
				return Mix64(static_cast<Hash64>(x));
			}
			else if constexpr (std::is_trivially_copyable_v<KeyType> && std::has_unique_object_representations_v<KeyType>)
			{
				return Hash64FromType<KeyType>(key);
			}
			else
			{
				static_assert(detail::DependentFalse<KeyType>::value,
					"HashMapDefaultPolicy: provide custom hash for non-trivial/padded types.");
				return Hash64{};
			}
		}
	};

	// 内部実装
	namespace detail
	{
		template<typename T>
		struct DependentFalse
		{
			static constexpr bool value = false;
		};

		template<typename KeyType, typename ValueType>
		struct HashMapBucket
		{
			Hash64 hash;
			KeyType key;
			ValueType value;
		};

		template<typename BucketType>
		class HashMapStorage
		{
		public:
			// バケットと状態配列の管理
			static constexpr uint8_t kEmpty = 0;

			MemoryAllocatorAPIHolder alloc;
			BucketType* buckets;
			uint8_t* states;
			uint32_t size;
			uint32_t cap;

			explicit HashMapStorage(MemoryAllocatorAPI* allocator)
				: alloc(allocator)
				, buckets(nullptr)
				, states(nullptr)
				, size(0)
				, cap(0)
			{
			}

			HashMapStorage(const HashMapStorage& src) = delete;
			HashMapStorage& operator=(const HashMapStorage& src) = delete;

			HashMapStorage(HashMapStorage&& src) noexcept
				: alloc(utl::Move(src.alloc))
				, buckets(src.buckets)
				, states(src.states)
				, size(src.size)
				, cap(src.cap)
			{
				src.buckets = nullptr;
				src.states = nullptr;
				src.size = 0;
				src.cap = 0;
			}

			HashMapStorage& operator=(HashMapStorage&& src) noexcept
			{
				if (this == &src) return *this;
				Reset();
				alloc.Reset(src.alloc.Get());
				buckets = src.buckets;
				states = src.states;
				size = src.size;
				cap = src.cap;
				src.buckets = nullptr;
				src.states = nullptr;
				src.size = 0;
				src.cap = 0;
				return *this;
			}

			~HashMapStorage()
			{
				Reset();
			}

			void Allocate(uint32_t newCap)
			{
				buckets = static_cast<BucketType*>(alloc.Allocate(sizeof(BucketType) * newCap, alignof(BucketType)));
				states = static_cast<uint8_t*>(alloc.Allocate(sizeof(uint8_t) * newCap, alignof(uint8_t)));
				std::memset(states, 0, sizeof(uint8_t) * newCap);
				size = 0;
				cap = newCap;
			}

			void Reset()
			{
				if (buckets == nullptr)
				{
					return;
				}
				if constexpr (!std::is_trivially_destructible_v<BucketType>)
				{
					for (uint32_t i = 0; i < cap; ++i)
					{
						if (states[i] != kEmpty)
						{
							buckets[i].~BucketType();
						}
					}
				}
				alloc.Free(buckets);
				alloc.Free(states);
				buckets = nullptr;
				states = nullptr;
				size = 0;
				cap = 0;
			}
		};

		template<uint32_t MaxLoadNumerator = 1,
			uint32_t MaxLoadDenominator = 2,
			uint32_t MinCapacity = 8,
			uint32_t HashShift = 7,
			uint32_t FingerprintBits = 7,
			uint32_t GrowFactor = 2>
		struct HashMapConfig
		{
			// 負荷率設定
			static_assert(MaxLoadDenominator != 0, "MaxLoadDenominator must not be 0.");
			static constexpr float kMaxLoad = static_cast<float>(MaxLoadNumerator) / static_cast<float>(MaxLoadDenominator);
			static_assert(MinCapacity >= 1, "MinCapacity must be >= 1.");
			static constexpr uint32_t kMinCapacity = MinCapacity;
			static_assert(HashShift < 64, "HashShift must be < 64.");
			static constexpr uint32_t kHashShift = HashShift;
			static_assert(FingerprintBits >= 1 && FingerprintBits <= 7, "FingerprintBits must be 1..7.");
			static constexpr uint32_t kFingerprintBits = FingerprintBits;
			static constexpr uint8_t kFingerprintMask = static_cast<uint8_t>((1u << FingerprintBits) - 1u);
			static_assert(GrowFactor >= 2, "GrowFactor must be >= 2.");
			static constexpr uint32_t kGrowFactor = GrowFactor;
		};

		template<typename KeyType, typename ValueType, typename HashPolicy, typename Config>
		class HashMapCore
		{
		public:
			// ハッシュマップ中核実装
			HashMapCore(uint32_t capacity = Config::kMinCapacity, MemoryAllocatorAPI* alloc = nullptr)
				: storage_(alloc)
				, hasher_()
			{
				Reserve(capacity);
			}

			HashMapCore(const HashMapCore& src) = delete;
			HashMapCore& operator=(const HashMapCore& src) = delete;

			HashMapCore(HashMapCore&& src) noexcept = default;
			HashMapCore& operator=(HashMapCore&& src) noexcept = default;

			~HashMapCore() = default;

			void Reserve(uint32_t capacity)
			{
				uint32_t newCap = ProbeOps::NextPow2(capacity < Config::kMinCapacity ? Config::kMinCapacity : capacity);
				if (newCap <= storage_.cap) return;
				Rehash(newCap);
			}

			ValueType* Insert(const KeyType& key, const ValueType& value)
			{
				return Emplace(key, value);
			}

			ValueType* Insert(const KeyType& key, ValueType&& value)
			{
				return Emplace(key, utl::Move(value));
			}

			template<typename... ArgTypes>
			ValueType* Emplace(const KeyType& key, ArgTypes&&... args)
			{
				return EmplaceImpl(key, ForwardArg(args)...);
			}

			ValueType* Find(const KeyType& key)
			{
				if (storage_.cap == 0) return nullptr;

				Hash64 hash = hasher_(key);
				return FindByHash(key, hash);
			}

			const ValueType* Find(const KeyType& key) const
			{
				if (storage_.cap == 0) return nullptr;

				Hash64 hash = hasher_(key);
				return FindByHashConst(key, hash);
			}

			ValueType* FindByHash(const KeyType& key, Hash64 hash)
			{
				return const_cast<ValueType*>(FindByHashConst(key, hash));
			}

			const ValueType* FindByHash(const KeyType& key, Hash64 hash) const
			{
				return FindByHashConst(key, hash);
			}

			const ValueType* FindByHashConst(const KeyType& key, Hash64 hash) const
			{
				if (storage_.cap == 0) return nullptr;
				uint32_t mask = storage_.cap - 1;
				IndexType index = ProbeOps::CalcIndex(hash, mask);
				IndexType linear = index;

				const uint8_t targetState = MakeState(hash);
				for (IndexType dist = 0; dist < storage_.cap; ++dist)
				{
					uint8_t state = GetState(linear);
					if (state == kEmpty)
					{
						return nullptr;
					}
					{
						const Bucket& b = storage_.buckets[index];
						if (state == targetState && b.hash == hash && (kKeyIsHash || b.key == key))
						{
							return &storage_.buckets[index].value;
						}
						uint32_t bIdeal = ProbeOps::CalcIndex(b.hash, mask);
						uint32_t bDist = (index + storage_.cap - bIdeal) & mask;
						if (bDist < dist)
						{
							return nullptr;
						}
					}
					index = (index + 1) & mask;
					++linear;
					if (linear >= storage_.cap) linear = 0;
				}

				return nullptr;
			}

			bool Remove(const KeyType& key)
			{
				if (storage_.cap == 0) return false;

				Hash64 hash = hasher_(key);
				return RemoveByHash(key, hash);
			}

			bool RemoveByHash(const KeyType& key, Hash64 hash)
			{
				if (storage_.cap == 0) return false;
				uint32_t mask = storage_.cap - 1;
				IndexType index = ProbeOps::CalcIndex(hash, mask);
				IndexType linear = index;

				const uint8_t targetState = MakeState(hash);
				for (IndexType dist = 0; dist < storage_.cap; ++dist)
				{
					uint8_t state = GetState(linear);
					if (state == kEmpty)
					{
						return false;
					}
					{
						Bucket& b = storage_.buckets[index];
						if (state == targetState && b.hash == hash && (kKeyIsHash || b.key == key))
						{
							EraseAndShift(index);
							--storage_.size;
							return true;
						}
						uint32_t bIdeal = ProbeOps::CalcIndex(b.hash, mask);
						uint32_t bDist = (index + storage_.cap - bIdeal) & mask;
						if (bDist < dist)
						{
							return false;
						}
					}
					index = (index + 1) & mask;
					++linear;
					if (linear >= storage_.cap) linear = 0;
				}

				return false;
			}

			uint32_t Size() const { return storage_.size; }
			uint32_t Capacity() const { return storage_.cap; }

		private:
			using IndexType = uint32_t;
			using Bucket = HashMapBucket<KeyType, ValueType>;
			using Storage = HashMapStorage<Bucket>;
			static constexpr bool kKeyIsHash = std::is_same_v<KeyType, Hash64>;

			struct ProbeOps
			{
				static IndexType CalcIndex(Hash64 hash, IndexType mask)
				{
					return static_cast<IndexType>(hash >> Config::kHashShift) & mask;
				}

				static IndexType ProbeDist(IndexType index, IndexType ideal, IndexType mask)
				{
					return (index + (mask + 1) - ideal) & mask;
				}

				static IndexType NextPow2(IndexType v)
				{
					if (v == 0) return 1;
					v--;
					v |= v >> 1;
					v |= v >> 2;
					v |= v >> 4;
					v |= v >> 8;
					v |= v >> 16;
					v++;
					return v;
				}
			};

			Storage storage_;
			HashPolicy hasher_;

			static constexpr float kMaxLoad = Config::kMaxLoad;
			static constexpr uint32_t kMinCapacity = Config::kMinCapacity;
			static constexpr uint32_t kHashShift = Config::kHashShift;
			static constexpr uint8_t kFingerprintMask = Config::kFingerprintMask;
			static constexpr uint32_t kGrowFactor = Config::kGrowFactor;
			static constexpr uint8_t kEmpty = Storage::kEmpty;
			static uint8_t MakeState(Hash64 hash)
			{
				uint8_t h2 = static_cast<uint8_t>(static_cast<uint64_t>(hash) & kFingerprintMask);
				return static_cast<uint8_t>(h2 + 1);
			}

			static bool MatchFingerprint(uint8_t state, Hash64 hash)
			{
				uint8_t h2 = static_cast<uint8_t>(static_cast<uint64_t>(hash) & kFingerprintMask);
				return state == static_cast<uint8_t>(h2 + 1);
			}

			template<typename T>
			static constexpr T&& ForwardArg(T&& v) noexcept
			{
				return static_cast<T&&>(v);
			}

			uint8_t GetState(IndexType index) const
			{
				return storage_.states[index];
			}

			void SetState(IndexType index, uint8_t state)
			{
				storage_.states[index] = state;
			}

			template<typename... ArgTypes>
			ValueType* EmplaceImpl(const KeyType& key, ArgTypes&&... args)
			{
				if (storage_.cap == 0)
				{
					Reserve(kMinCapacity);
				}

				if (static_cast<float>(storage_.size + 1) > static_cast<float>(storage_.cap) * kMaxLoad)
				{
					Rehash(storage_.cap * kGrowFactor);
				}

				Hash64 hash = hasher_(key);
				Bucket incoming{ hash, key, ValueType(ForwardArg(args)...) };
				return InsertBucket(utl::Move(incoming));
			}

			ValueType* InsertBucket(Bucket&& incoming)
			{
				IndexType mask = storage_.cap - 1;
				IndexType index = ProbeOps::CalcIndex(incoming.hash, mask);
				IndexType dist = 0;
				uint8_t incomingState = MakeState(incoming.hash);

				while (true)
				{
					if (GetState(index) == kEmpty)
					{
						new (storage_.buckets + index) Bucket(utl::Move(incoming));
						SetState(index, incomingState);
						++storage_.size;
						return &storage_.buckets[index].value;
					}

					Bucket& b = storage_.buckets[index];
					if (MatchFingerprint(GetState(index), incoming.hash) && b.hash == incoming.hash && (kKeyIsHash || b.key == incoming.key))
					{
						b.value = utl::Move(incoming.value);
						return &b.value;
					}

					IndexType bIdeal = ProbeOps::CalcIndex(b.hash, mask);
					IndexType bDist = ProbeOps::ProbeDist(index, bIdeal, mask);

					if (bDist < dist)
					{
						utl::Swap(incoming, b);
						uint8_t tmpState = GetState(index);
						SetState(index, incomingState);
						incomingState = tmpState;
						dist = bDist;
					}

					index = (index + 1) & mask;
					++dist;
				}
			}

			void Rehash(IndexType newCap)
			{
				Bucket* oldBuckets = storage_.buckets;
				uint8_t* oldStates = storage_.states;
				IndexType oldCap = storage_.cap;
				IndexType oldSize = storage_.size;

				storage_.Allocate(newCap);

				if (oldBuckets == nullptr)
				{
					return;
				}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
				try
				{
					for (IndexType i = 0; i < oldCap; ++i)
					{
						if (oldStates[i] != kEmpty)
						{
							Bucket& b = oldBuckets[i];
							if constexpr (std::is_copy_constructible_v<KeyType> && std::is_copy_constructible_v<ValueType>)
							{
								InsertBucket(Bucket{ b.hash, b.key, b.value });
							}
							else if constexpr (std::is_nothrow_move_constructible_v<KeyType> && std::is_nothrow_move_constructible_v<ValueType>)
							{
								InsertBucket(Bucket{ b.hash, utl::Move(b.key), utl::Move(b.value) });
							}
							else
							{
								static_assert(DependentFalse<KeyType>::value,
									"Rehash: non-copyable types require nothrow-move for exception safety.");
							}
						}
					}
				}
				catch (...)
				{
					if constexpr (!std::is_trivially_destructible_v<Bucket>)
					{
						for (IndexType i = 0; i < storage_.cap; ++i)
						{
							if (storage_.states[i] != kEmpty)
							{
								storage_.buckets[i].~Bucket();
							}
						}
					}
					storage_.alloc.Free(storage_.buckets);
					storage_.alloc.Free(storage_.states);
					storage_.buckets = oldBuckets;
					storage_.states = oldStates;
					storage_.cap = oldCap;
					storage_.size = oldSize;
					throw;
				}

				if constexpr (!std::is_trivially_destructible_v<Bucket>)
				{
					for (IndexType i = 0; i < oldCap; ++i)
					{
						if (oldStates[i] != kEmpty)
						{
							oldBuckets[i].~Bucket();
						}
					}
				}
				storage_.alloc.Free(oldBuckets);
				storage_.alloc.Free(oldStates);
#else
				for (IndexType i = 0; i < oldCap; ++i)
				{
					if (oldStates[i] != kEmpty)
					{
						Bucket& b = oldBuckets[i];
						InsertBucket(Bucket{ b.hash, utl::Move(b.key), utl::Move(b.value) });
						b.~Bucket();
					}
				}
				storage_.alloc.Free(oldBuckets);
				storage_.alloc.Free(oldStates);
#endif
			}

			void EraseAndShift(IndexType index)
			{
				IndexType mask = storage_.cap - 1;
				storage_.buckets[index].~Bucket();

				IndexType i = index;
				IndexType j = (i + 1) & mask;

				while (storage_.states[j] != kEmpty)
				{
					IndexType ideal = ProbeOps::CalcIndex(storage_.buckets[j].hash, mask);
					IndexType dist = ProbeOps::ProbeDist(j, ideal, mask);
					if (dist == 0)
					{
						break;
					}

					new (storage_.buckets + i) Bucket(utl::Move(storage_.buckets[j]));
					storage_.buckets[j].~Bucket();
					SetState(i, GetState(j));
					i = j;
					j = (j + 1) & mask;
				}

				SetState(i, kEmpty);
			}

		};

	}

	// 設定型の別名
	template<uint32_t MaxLoadNumerator = 1,
		uint32_t MaxLoadDenominator = 2,
		uint32_t MinCapacity = 8,
		uint32_t HashShift = 7,
		uint32_t FingerprintBits = 7,
		uint32_t GrowFactor = 2>
	using HashMapConfig = detail::HashMapConfig<MaxLoadNumerator, MaxLoadDenominator, MinCapacity, HashShift, FingerprintBits, GrowFactor>;

	// 実装クラスの別名
	template<typename KeyType, typename ValueType, typename HashPolicy, typename Config>
	using HashMapImpl = detail::HashMapCore<KeyType, ValueType, HashPolicy, Config>;

	// 取得優先（検索が速い）
	namespace detail
	{
		template<typename T1, typename T2, typename T3>
		struct HashMapSelector;

		template<typename ValueType>
		struct HashMapSelector<ValueType, HashMapDefaultTag, HashMapDefaultTag>
		{
			using type = HashMapImpl<Hash64, ValueType, HashMapDefaultPolicy<Hash64>, HashMapConfig<9, 10>>;
		};

		template<typename KeyType, typename ValueType>
		struct HashMapSelector<KeyType, ValueType, HashMapDefaultTag>
		{
			using type = HashMapImpl<KeyType, ValueType, HashMapDefaultPolicy<KeyType>, HashMapConfig<9, 10>>;
		};

		template<typename KeyType, typename ValueType, typename HashPolicy>
		struct HashMapSelector
		{
			using type = HashMapImpl<KeyType, ValueType, HashPolicy, HashMapConfig<9, 10>>;
		};
	}

	template<typename T1, typename T2 = detail::HashMapDefaultTag, typename T3 = detail::HashMapDefaultTag>
	using HashMap = typename detail::HashMapSelector<T1, T2, T3>::type;

	// 追加/削除優先
	template<typename KeyType, typename ValueType, typename HashPolicy = HashMapDefaultPolicy<KeyType>>
	using HashMapInsertErase = HashMapImpl<KeyType, ValueType, HashPolicy, HashMapConfig<1, 2>>;

	// 設定指定の別名（任意設定）
	template<typename KeyType,
		typename ValueType,
		typename HashPolicy = HashMapDefaultPolicy<KeyType>,
		uint32_t MaxLoadNumerator = 1,
		uint32_t MaxLoadDenominator = 2,
		uint32_t MinCapacity = 8,
		uint32_t HashShift = 7,
		uint32_t FingerprintBits = 7,
		uint32_t GrowFactor = 2>
	using HashMapCustom = HashMapImpl<KeyType, ValueType, HashPolicy,
		HashMapConfig<MaxLoadNumerator, MaxLoadDenominator, MinCapacity, HashShift, FingerprintBits, GrowFactor>>;

}
