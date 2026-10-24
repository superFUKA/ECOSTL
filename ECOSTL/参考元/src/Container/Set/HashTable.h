#pragma once
#include "../Map/HashMap.h"
#include "src/Utility/RecyclableID/RecyclableID.h"
#include <type_traits>

namespace utl
{
	// Hash64 -> 連番IDテーブル（衝突は同一ID扱い）
	template<typename IdType = uint32_t>
	class HashTable
	{
	public:
		static_assert(std::is_integral_v<IdType>, "HashTable: IdType は整数型のみ対応しています。");

		explicit HashTable(uint32_t capacity = kMinCapacity,
			MemoryAllocatorAPI* alloc = nullptr,
			IdType startId = IdType{})
			: storage_(alloc)
			, next_id_(startId)
		{
			Reserve(capacity);
		}

		HashTable(const HashTable& src) = delete;
		HashTable& operator=(const HashTable& src) = delete;
		HashTable(HashTable&& src) noexcept = default;
		HashTable& operator=(HashTable&& src) noexcept = default;

		~HashTable() = default;

		uint32_t Size() const { return storage_.size; }

		IdType* Find(Hash64 hash)
		{
			return const_cast<IdType*>(FindConst(hash));
		}

		const IdType* Find(Hash64 hash) const
		{
			return FindConst(hash);
		}

		IdType GetOrCreate(Hash64 hash)
		{
			if (storage_.cap == 0)
			{
				Reserve(kMinCapacity);
			}

			if (static_cast<float>(storage_.size + 1) > static_cast<float>(storage_.cap) * kMaxLoad)
			{
				Rehash(storage_.cap * kGrowFactor);
			}

			Hash64 h = hash;
			InsertResult result = InsertOrGet(h, next_id_);
			if (result.inserted)
			{
				++next_id_;
			}
			return *result.id;
		}

		bool Remove(Hash64 hash)
		{
			if (storage_.cap == 0) return false;
			Hash64 h = hash;
			bool removed = RemoveByHash(h);
			if (removed && storage_.size > 0)
			{
				Rehash(storage_.cap);
			}
			return removed;
		}

	private:
		using IndexType = uint32_t;
		using Config = HashMapConfig<1, 3>;

		struct Bucket
		{
			Hash64 hash;
			IdType id;
		};

		class Storage
		{
		public:
			static constexpr uint8_t kEmpty = 0;

			MemoryAllocatorAPIHolder alloc;
			Bucket* buckets;
			uint8_t* states;
			uint32_t size;
			uint32_t cap;

			explicit Storage(MemoryAllocatorAPI* allocator)
				: alloc(allocator)
				, buckets(nullptr)
				, states(nullptr)
				, size(0)
				, cap(0)
			{
			}

			Storage(const Storage& src) = delete;
			Storage& operator=(const Storage& src) = delete;
			Storage(Storage&& src) noexcept
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

			Storage& operator=(Storage&& src) noexcept
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

			~Storage()
			{
				Reset();
			}

			void Allocate(uint32_t newCap)
			{
				buckets = static_cast<Bucket*>(alloc.Allocate(sizeof(Bucket) * newCap, alignof(Bucket)));
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
				if constexpr (!std::is_trivially_destructible_v<Bucket>)
				{
					for (uint32_t i = 0; i < cap; ++i)
					{
						if (states[i] != kEmpty)
						{
							buckets[i].~Bucket();
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

		struct ProbeOps
		{
			static IndexType CalcIndex(Hash64 hash, IndexType mask)
			{
				return static_cast<IndexType>(hash >> kHashShift) & mask;
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

		struct InsertResult
		{
			IdType* id;
			bool inserted;
		};

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

		uint8_t GetState(IndexType index) const
		{
			return storage_.states[index];
		}

		void SetState(IndexType index, uint8_t state)
		{
			storage_.states[index] = state;
		}

		const IdType* FindConst(Hash64 hash) const
		{
			if (storage_.cap == 0) return nullptr;

			Hash64 h = hash;
			IndexType mask = storage_.cap - 1;
			IndexType index = ProbeOps::CalcIndex(h, mask);

			const uint8_t targetState = MakeState(h);
			for (IndexType dist = 0; dist < storage_.cap; ++dist)
			{
				uint8_t state = GetState(index);
				if (state == kEmpty)
				{
					return nullptr;
				}

				const Bucket& b = storage_.buckets[index];
				if (state == targetState && b.hash == h)
				{
					return &b.id;
				}

				IndexType bIdeal = ProbeOps::CalcIndex(b.hash, mask);
				IndexType bDist = ProbeOps::ProbeDist(index, bIdeal, mask);
				if (bDist < dist)
				{
					return nullptr;
				}

				index = (index + 1) & mask;
			}

			return nullptr;
		}

		InsertResult InsertOrGet(Hash64 hash, IdType id)
		{
			IndexType mask = storage_.cap - 1;
			IndexType index = ProbeOps::CalcIndex(hash, mask);
			IndexType dist = 0;
			uint8_t incomingState = MakeState(hash);
			Bucket incoming{ hash, id };

			while (true)
			{
				if (GetState(index) == kEmpty)
				{
					new (storage_.buckets + index) Bucket(utl::Move(incoming));
					SetState(index, incomingState);
					++storage_.size;
					return { &storage_.buckets[index].id, true };
				}

				Bucket& b = storage_.buckets[index];
				if (MatchFingerprint(GetState(index), hash) && b.hash == hash)
				{
					return { &b.id, false };
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

		bool RemoveByHash(Hash64 hash)
		{
			IndexType mask = storage_.cap - 1;
			IndexType index = ProbeOps::CalcIndex(hash, mask);

			const uint8_t targetState = MakeState(hash);
			for (IndexType dist = 0; dist < storage_.cap; ++dist)
			{
				uint8_t state = GetState(index);
				if (state == kEmpty)
				{
					return false;
				}

				Bucket& b = storage_.buckets[index];
				if (state == targetState && b.hash == hash)
				{
					EraseAndShift(index);
					--storage_.size;
					return true;
				}

				IndexType bIdeal = ProbeOps::CalcIndex(b.hash, mask);
				IndexType bDist = ProbeOps::ProbeDist(index, bIdeal, mask);
				if (bDist < dist)
				{
					return false;
				}

				index = (index + 1) & mask;
			}

			return false;
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
						InsertOrGet(b.hash, b.id);
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
					InsertOrGet(b.hash, b.id);
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

		void Reserve(uint32_t capacity)
		{
			uint32_t newCap = ProbeOps::NextPow2(capacity < kMinCapacity ? kMinCapacity : capacity);
			if (newCap <= storage_.cap) return;
			Rehash(newCap);
		}

		Storage storage_;
		IdType next_id_;
	};

	// Hash64 -> RecyclableIDテーブル（衝突は同一ID扱い）
	class HashTableRecycle
	{
	public:
		using IdType = RecyclableID;

		explicit HashTableRecycle(uint32_t capacity = kMinCapacity,
			MemoryAllocatorAPI* alloc = nullptr)
			: storage_(alloc)
			, id_allocator_()
		{
			Reserve(capacity);
		}

		HashTableRecycle(const HashTableRecycle& src) = delete;
		HashTableRecycle& operator=(const HashTableRecycle& src) = delete;
		HashTableRecycle(HashTableRecycle&& src) noexcept = default;
		HashTableRecycle& operator=(HashTableRecycle&& src) noexcept = default;

		~HashTableRecycle() = default;

		uint32_t Size() const { return storage_.size; }

		IdType* Find(Hash64 hash)
		{
			return const_cast<IdType*>(FindConst(hash));
		}

		const IdType* Find(Hash64 hash) const
		{
			return FindConst(hash);
		}

		IdType GetOrCreate(Hash64 hash)
		{
			if (storage_.cap == 0)
			{
				Reserve(kMinCapacity);
			}

			if (static_cast<float>(storage_.size + 1) > static_cast<float>(storage_.cap) * kMaxLoad)
			{
				Rehash(storage_.cap * kGrowFactor);
			}

			Hash64 h = hash;
			InsertResult result = InsertOrGet(h, id_allocator_.Allocate());
			return *result.id;
		}

		bool Remove(Hash64 hash)
		{
			if (storage_.cap == 0) return false;
			Hash64 h = hash;
			RemoveResult result = RemoveByHash(h);
			if (result.removed)
			{
				id_allocator_.Deallocate(result.removed_id);
				if (storage_.size > 0)
				{
					Rehash(storage_.cap);
				}
			}
			return result.removed;
		}

	private:
		using IndexType = uint32_t;
		using Config = HashMapConfig<1, 3>;

		struct Bucket
		{
			Hash64 hash;
			IdType id;
		};

		class Storage
		{
		public:
			static constexpr uint8_t kEmpty = 0;

			MemoryAllocatorAPIHolder alloc;
			Bucket* buckets;
			uint8_t* states;
			uint32_t size;
			uint32_t cap;

			explicit Storage(MemoryAllocatorAPI* allocator)
				: alloc(allocator)
				, buckets(nullptr)
				, states(nullptr)
				, size(0)
				, cap(0)
			{
			}

			Storage(const Storage& src) = delete;
			Storage& operator=(const Storage& src) = delete;
			Storage(Storage&& src) noexcept
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

			Storage& operator=(Storage&& src) noexcept
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

			~Storage()
			{
				Reset();
			}

			void Allocate(uint32_t newCap)
			{
				buckets = static_cast<Bucket*>(alloc.Allocate(sizeof(Bucket) * newCap, alignof(Bucket)));
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
				if constexpr (!std::is_trivially_destructible_v<Bucket>)
				{
					for (uint32_t i = 0; i < cap; ++i)
					{
						if (states[i] != kEmpty)
						{
							buckets[i].~Bucket();
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

		struct ProbeOps
		{
			static IndexType CalcIndex(Hash64 hash, IndexType mask)
			{
				return static_cast<IndexType>(hash >> kHashShift) & mask;
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

		struct InsertResult
		{
			IdType* id;
			bool inserted;
		};

		struct RemoveResult
		{
			IdType removed_id;
			bool removed;
		};

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

		uint8_t GetState(IndexType index) const
		{
			return storage_.states[index];
		}

		void SetState(IndexType index, uint8_t state)
		{
			storage_.states[index] = state;
		}

		const IdType* FindConst(Hash64 hash) const
		{
			if (storage_.cap == 0) return nullptr;

			Hash64 h = hash;
			IndexType mask = storage_.cap - 1;
			IndexType index = ProbeOps::CalcIndex(h, mask);

			const uint8_t targetState = MakeState(h);
			for (IndexType dist = 0; dist < storage_.cap; ++dist)
			{
				uint8_t state = GetState(index);
				if (state == kEmpty)
				{
					return nullptr;
				}

				const Bucket& b = storage_.buckets[index];
				if (state == targetState && b.hash == h)
				{
					return &b.id;
				}

				IndexType bIdeal = ProbeOps::CalcIndex(b.hash, mask);
				IndexType bDist = ProbeOps::ProbeDist(index, bIdeal, mask);
				if (bDist < dist)
				{
					return nullptr;
				}

				index = (index + 1) & mask;
			}

			return nullptr;
		}

		InsertResult InsertOrGet(Hash64 hash, IdType id)
		{
			IndexType mask = storage_.cap - 1;
			IndexType index = ProbeOps::CalcIndex(hash, mask);
			IndexType dist = 0;
			uint8_t incomingState = MakeState(hash);
			Bucket incoming{ hash, id };

			while (true)
			{
				if (GetState(index) == kEmpty)
				{
					new (storage_.buckets + index) Bucket(utl::Move(incoming));
					SetState(index, incomingState);
					++storage_.size;
					return { &storage_.buckets[index].id, true };
				}

				Bucket& b = storage_.buckets[index];
				if (MatchFingerprint(GetState(index), hash) && b.hash == hash)
				{
					return { &b.id, false };
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

		RemoveResult RemoveByHash(Hash64 hash)
		{
			IndexType mask = storage_.cap - 1;
			IndexType index = ProbeOps::CalcIndex(hash, mask);

			const uint8_t targetState = MakeState(hash);
			for (IndexType dist = 0; dist < storage_.cap; ++dist)
			{
				uint8_t state = GetState(index);
				if (state == kEmpty)
				{
					return { {}, false };
				}

				Bucket& b = storage_.buckets[index];
				if (state == targetState && b.hash == hash)
				{
					IdType removed_id = b.id;
					EraseAndShift(index);
					--storage_.size;
					return { removed_id, true };
				}

				IndexType bIdeal = ProbeOps::CalcIndex(b.hash, mask);
				IndexType bDist = ProbeOps::ProbeDist(index, bIdeal, mask);
				if (bDist < dist)
				{
					return { {}, false };
				}

				index = (index + 1) & mask;
			}

			return { {}, false };
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
						InsertOrGet(b.hash, b.id);
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
					InsertOrGet(b.hash, b.id);
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

		void Reserve(uint32_t capacity)
		{
			uint32_t newCap = ProbeOps::NextPow2(capacity < kMinCapacity ? kMinCapacity : capacity);
			if (newCap <= storage_.cap) return;
			Rehash(newCap);
		}

		Storage storage_;
		RecyclableIDAllocator id_allocator_;
	};
}
