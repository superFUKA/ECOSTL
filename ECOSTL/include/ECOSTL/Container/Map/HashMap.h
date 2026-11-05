#pragma once
// ecoSTL の開放アドレス（ロビンフッド）ハッシュマップ。検索重視・追加削除重視・任意設定の別名を提供する。
#include "ECOSTL/Core/Hash/Hash.h"
#include "ECOSTL/Core/Math/Math.h"
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Algorithm/Sort/Sort.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include <cstdint>
#include <cstring>

// ecoSTL の共通ユーティリティ名前空間。
namespace utl
{
	// HashMap の内部実装（外部から直接使わない）。
	namespace detail
	{
		// static_assert を型依存にして、使われた時だけ失敗させるための常に false の型。
		template<typename T>
		struct DependentFalse;

		// HashMap の型引数を省略したことを表すタグ（テンプレート特殊化の選択用）。
		struct HashMapDefaultTag {};
	}

	// HashMap用のデフォルトハッシュ
	template<typename KeyType>
	struct HashMapDefaultPolicy
	{
		// 64bit 値を混ぜ直すミキサ。constexpr でスレッド安全（状態を持たない）。
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
			if constexpr (utl::IsSameV<KeyType, Hash64>)
			{
				return Mix64(key);
			}
			else if constexpr (utl::IsIntegralV<KeyType> || utl::IsEnumV<KeyType>)
			{
				uint64_t x = static_cast<uint64_t>(key);
				return Mix64(static_cast<Hash64>(x));
			}
			else if constexpr (utl::IsTriviallyCopyableV<KeyType> && utl::HasUniqueObjectRepresentationsV<KeyType>)
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
		// 定義側。value は常に false で、テンプレート実体化されたときだけ static_assert に使う。
		template<typename T>
		struct DependentFalse
		{
			// 常に false（static_assert の条件が型依存になる）。
			static constexpr bool value = false;
		};

		// 1 要素分のバケット。ハッシュ値を保持し、再配置時の再計算と比較の高速化に使う。
		template<typename KeyType, typename ValueType>
		struct HashMapBucket
		{
			Hash64 hash;  // キーのハッシュ値（Rehash と比較の短絡用に保持）
			KeyType key;  // キー本体
			ValueType value;  // 値本体
		};

		// バケット配列と状態配列を確保・解放する所有者。コピー不可でムーブのみ。スレッド非安全。
		template<typename BucketType>
		class HashMapStorage
		{
		public:
			// バケットと状態配列の管理
			static constexpr uint8_t kEmpty = 0;  // 状態バイトが 0 のときは空きスロット

			MemoryAllocatorRef alloc;  // メモリ確保 API（nullptr なら既定のアロケータ）
			BucketType* buckets;  // バケット配列（容量 cap 個、未使用スロットは未構築）
			uint8_t* states;  // 各スロットの状態（0=空き、それ以外はフィンガープリント+1）
			uint32_t size;  // 使用中の要素数
			uint32_t cap;  // スロット数（常に 2 の冪、0 は未確保）

			explicit HashMapStorage(MemoryAllocatorDelegate* allocator)
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

			// newCap 個のスロットを確保し状態を空きで初期化する。既存の確保は解放しない（呼び出し側で退避すること）。
			void Allocate(uint32_t newCap)
			{
				buckets = static_cast<BucketType*>(alloc.Allocate(sizeof(BucketType) * newCap, alignof(BucketType)));
				states = static_cast<uint8_t*>(alloc.Allocate(sizeof(uint8_t) * newCap, alignof(uint8_t)));
				std::memset(states, 0, sizeof(uint8_t) * newCap);
				size = 0;
				cap = newCap;
			}

			// 使用中バケットを破棄してメモリを解放し、空の状態に戻す。
			void Reset()
			{
				if (buckets == nullptr)
				{
					return;
				}
				if constexpr (!utl::IsTriviallyDestructibleV<BucketType>)
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

		// 負荷率・最小容量・フィンガープリント幅などの調整値。最大負荷率 = 分子/分母。
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
			static constexpr float kMaxLoad = static_cast<float>(MaxLoadNumerator) / static_cast<float>(MaxLoadDenominator);  // 最大負荷率（これを超えると拡張）
			static_assert(MinCapacity >= 1, "MinCapacity must be >= 1.");
			static constexpr uint32_t kMinCapacity = MinCapacity;  // 最小容量（スロット数）
			static_assert(HashShift < 64, "HashShift must be < 64.");
			static constexpr uint32_t kHashShift = HashShift;  // ハッシュ値を右シフトしてから添字にする量
			static_assert(FingerprintBits >= 1 && FingerprintBits <= 7, "FingerprintBits must be 1..7.");
			static constexpr uint32_t kFingerprintBits = FingerprintBits;  // 状態バイトに入れるハッシュ下位ビット数（1..7）
			static constexpr uint8_t kFingerprintMask = static_cast<uint8_t>((1u << FingerprintBits) - 1u);  // フィンガープリントのビットマスク
			static_assert(GrowFactor >= 2, "GrowFactor must be >= 2.");
			static constexpr uint32_t kGrowFactor = GrowFactor;  // 拡張時に容量へ掛ける倍率
		};

		// ロビンフッド方式のオープンアドレス法ハッシュマップ本体。平均 O(1)、拡張時は全要素再配置 O(n) で、ポインタ・参照が無効になる。スレッド非安全。
		template<typename KeyType, typename ValueType, typename HashPolicy, typename Config>
		class HashMapCore
		{
		public:
			// ハッシュマップ中核実装
			HashMapCore(uint32_t capacity = Config::kMinCapacity, MemoryAllocatorDelegate* alloc = nullptr)
				: storage_(alloc)
				, hasher_()
			{
				Reserve(capacity);
			}
			// 使うアロケータを指定して作る（所有しない。nullptr なら既定のアロケータ）
			explicit HashMapCore(MemoryAllocatorDelegate* alloc)
				: HashMapCore(Config::kMinCapacity, alloc)
			{
			}
			explicit HashMapCore(const MemoryAllocatorRef& alloc)
				: HashMapCore(Config::kMinCapacity, alloc.Get())
			{
			}

			// コピーは元と同じアロケータを使い、要素を1つずつ入れ直す
			HashMapCore(const HashMapCore& src)
				: storage_(src.storage_.alloc.Get())
				, hasher_(src.hasher_)
			{
				CopyFrom(src);
			}
			HashMapCore& operator=(const HashMapCore& src)
			{
				if (this != &src)
				{
					Clear();
					CopyFrom(src);
				}
				return *this;
			}

			HashMapCore(HashMapCore&& src) noexcept = default;
			HashMapCore& operator=(HashMapCore&& src) noexcept = default;

			~HashMapCore() = default;

			// 少なくとも capacity 個のスロットを確保する（縮小はしない）。拡張時はポインタが無効になる。
			void Reserve(uint32_t capacity)
			{
				uint32_t newCap = ProbeOps::NextPow2(capacity < Config::kMinCapacity ? Config::kMinCapacity : capacity);
				if (newCap <= storage_.cap) return;
				Rehash(newCap);
			}

			/// <summary>
			/// key に、引数から作った値を入れる。同じキーがあれば値を上書きする。入れた値のポインタを返す
			/// </summary>
			template<typename... ArgTypes>
			ValueType* Insert(const KeyType& key, ArgTypes&&... args)
			{
				return EmplaceImpl(key, ForwardArg(args)...);
			}

			/// <summary>key の値を返す。無ければ既定の値で作ってから返す</summary>
			ValueType& GetOrAdd(const KeyType& key)
			{
				if (ValueType* found = Find(key))
				{
					return *found;
				}
				return *EmplaceImpl(key);
			}
			ValueType& operator[](const KeyType& key)
			{
				return GetOrAdd(key);
			}

			/// <summary>key を含むか</summary>
			bool Has(const KeyType& key) const
			{
				return Find(key) != nullptr;
			}

			// キーに対応する値のポインタを返す。見つからなければ nullptr。
			ValueType* Find(const KeyType& key)
			{
				if (storage_.cap == 0) return nullptr;

				Hash64 hash = hasher_(key);
				return FindByHash(key, hash);
			}

			// キーに対応する値のポインタを返す（const 版）。見つからなければ nullptr。
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

			// 計算済みハッシュ値で検索する本体。ロビンフッドの距離比較で早期に打ち切る。
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

			// キーを削除する。削除したら true。後続要素を前詰めするので墓石は残らない。
			bool Remove(const KeyType& key)
			{
				if (storage_.cap == 0) return false;

				Hash64 hash = hasher_(key);
				return RemoveByHash(key, hash);
			}

			// 計算済みハッシュ値でキーを削除する。削除したら true。
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
			bool IsEmpty() const { return storage_.size == 0; }

			/// <summary>全ての要素を消す（確保した領域は残す）</summary>
			void Clear()
			{
				for (IndexType i = 0; i < storage_.cap; ++i)
				{
					if (GetState(i) != kEmpty)
					{
						if constexpr (!utl::IsTriviallyDestructibleV<Bucket>)
						{
							storage_.buckets[i].~Bucket();
						}
						SetState(i, kEmpty);
					}
				}
				storage_.size = 0;
			}

			/// <summary>全要素を順に取り出す（範囲 for 用。要素は key と value を持つ。順番は決まっていない。追加・削除で無効になる）</summary>
			template<typename MapType, typename BucketType>
			class IteratorT
			{
			public:
				IteratorT(MapType* map, const uint32_t index) : map_(map), index_(index) { SkipEmpty(); }
				BucketType& operator*() const { return map_->storage_.buckets[index_]; }
				BucketType* operator->() const { return &map_->storage_.buckets[index_]; }
				IteratorT& operator++() { ++index_; SkipEmpty(); return *this; }
				bool operator==(const IteratorT& o) const { return index_ == o.index_; }
				bool operator!=(const IteratorT& o) const { return index_ != o.index_; }
			private:
				void SkipEmpty() { while (index_ < map_->storage_.cap && map_->GetState(index_) == kEmpty) { ++index_; } }
				MapType* map_;  // 回している表（所有しない）
				uint32_t index_;  // 今のスロット
			};
			using Iterator = IteratorT<HashMapCore, HashMapBucket<KeyType, ValueType>>;
			using ConstIterator = IteratorT<const HashMapCore, const HashMapBucket<KeyType, ValueType>>;

			// 範囲 for 文用（言語が要求する名前）
			Iterator begin() { return Iterator(this, 0); }
			Iterator end() { return Iterator(this, storage_.cap); }
			ConstIterator begin() const { return ConstIterator(this, 0); }
			ConstIterator end() const { return ConstIterator(this, storage_.cap); }

		private:
			using IndexType = uint32_t;
			using Bucket = HashMapBucket<KeyType, ValueType>;
			using Storage = HashMapStorage<Bucket>;
			static constexpr bool kKeyIsHash = utl::IsSameV<KeyType, Hash64>;

			// src の要素を全て入れる（自分は空であること）
			void CopyFrom(const HashMapCore& src)
			{
				Reserve(src.storage_.cap);
				for (IndexType i = 0; i < src.storage_.cap; ++i)
				{
					if (src.GetState(i) != kEmpty)
					{
						const Bucket& b = src.storage_.buckets[i];
						InsertBucket(Bucket{ b.hash, b.key, b.value });
					}
				}
			}

			template<typename MapType, typename BucketType>
			friend class IteratorT;  // キーがすでにハッシュ値なら、キー比較を省略できる

			// ハッシュ値からスロット添字・理想位置からの距離・2 の冪への切り上げを求める補助。
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

				// v 以上で最小の 2 の冪を返す（0 なら 1）。
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

			Storage storage_;  // バケット・状態配列の保持
			HashPolicy hasher_;  // キーからハッシュ値を作る方針オブジェクト

			static constexpr float kMaxLoad = Config::kMaxLoad;  // 最大負荷率（Config の写し）
			static constexpr uint32_t kMinCapacity = Config::kMinCapacity;  // 最小容量（Config の写し）
			static constexpr uint32_t kHashShift = Config::kHashShift;  // ハッシュ右シフト量（Config の写し）
			static constexpr uint8_t kFingerprintMask = Config::kFingerprintMask;  // フィンガープリントのマスク（Config の写し）
			static constexpr uint32_t kGrowFactor = Config::kGrowFactor;  // 拡張倍率（Config の写し）
			static constexpr uint8_t kEmpty = Storage::kEmpty;  // 空きスロットを表す状態値
			// ハッシュ下位ビットから状態バイトを作る（0 は空き専用なので +1 する）。
			static uint8_t MakeState(Hash64 hash)
			{
				uint8_t h2 = static_cast<uint8_t>(static_cast<uint64_t>(hash) & kFingerprintMask);
				return static_cast<uint8_t>(h2 + 1);
			}

			// 状態バイトがハッシュのフィンガープリントと一致するか。キー比較の前段の絞り込み。
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

			// 必要なら拡張してから要素を構築して挿入する。同じキーがあれば値を上書きする。
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

			// ロビンフッド法で incoming を挿入する。距離の短い要素と入れ替えながら空きまで進む。
			ValueType* InsertBucket(Bucket&& incoming)
			{
				IndexType mask = storage_.cap - 1;
				IndexType index = ProbeOps::CalcIndex(incoming.hash, mask);
				IndexType dist = 0;
				uint8_t incomingState = MakeState(incoming.hash);
				// 入れ替えで新しい要素を置いた場所 入れ替え後に運んでいるのは押し出された別の要素
				ValueType* placed = nullptr;

				while (true)
				{
					if (GetState(index) == kEmpty)
					{
						new (storage_.buckets + index) Bucket(utl::Move(incoming));
						SetState(index, incomingState);
						++storage_.size;
						return placed != nullptr ? placed : &storage_.buckets[index].value;
					}

					Bucket& b = storage_.buckets[index];
					if (placed == nullptr && MatchFingerprint(GetState(index), incoming.hash) && b.hash == incoming.hash && (kKeyIsHash || b.key == incoming.key))
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
						if (placed == nullptr)
						{
							placed = &b.value;
						}
					}

					index = (index + 1) & mask;
					++dist;
				}
			}

			// 容量を newCap にして全要素を再配置する。例外時は元の配列に戻す。
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
							if constexpr (utl::IsCopyConstructibleV<KeyType> && utl::IsCopyConstructibleV<ValueType>)
							{
								InsertBucket(Bucket{ b.hash, b.key, b.value });
							}
							else if constexpr (utl::IsNothrowMoveConstructibleV<KeyType> && utl::IsNothrowMoveConstructibleV<ValueType>)
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
					if constexpr (!utl::IsTriviallyDestructibleV<Bucket>)
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

				if constexpr (!utl::IsTriviallyDestructibleV<Bucket>)
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

			// index の要素を破棄し、後続の理想位置でない要素を 1 つずつ前へ詰める（後退シフト削除）。
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
		// キー・値・方針の指定方法ごとに実装型を選ぶ（検索重視の既定設定）。
		template<typename T1, typename T2, typename T3>
		struct HashMapSelector;

		// HashMap<Value> の形。キーは Hash64 としてそのまま使う。
		template<typename ValueType>
		struct HashMapSelector<ValueType, HashMapDefaultTag, HashMapDefaultTag>
		{
			using type = HashMapImpl<Hash64, ValueType, HashMapDefaultPolicy<Hash64>, HashMapConfig<9, 10>>;
		};

		// HashMap<Key, Value> の形。方針は既定のハッシュ。
		template<typename KeyType, typename ValueType>
		struct HashMapSelector<KeyType, ValueType, HashMapDefaultTag>
		{
			using type = HashMapImpl<KeyType, ValueType, HashMapDefaultPolicy<KeyType>, HashMapConfig<9, 10>>;
		};

		// HashMap<Key, Value, Policy> の形。ハッシュ方針を指定する。
		template<typename KeyType, typename ValueType, typename HashPolicy>
		struct HashMapSelector
		{
			using type = HashMapImpl<KeyType, ValueType, HashPolicy, HashMapConfig<9, 10>>;
		};
	}

	// 検索優先の HashMap（最大負荷率 0.9）。
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
