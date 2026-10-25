#pragma once
#include <cstdint>
#include "../../Core/Hash.h"
#include "../../Core/Utility.h"
#include "../../Memory/Allocate/MemoryAllocatorAPI.h"
#include "../Vector/Vector.h"

namespace utl
{
	/// <summary>
	/// ハッシュ値を、0から連番のIDに対応させるテーブル
	/// 検索の表はIDとハッシュ値の一部だけを持つ8バイトの小さなスロットで、線形探索のオープンアドレス法で作る
	/// ハッシュ値の全体はID順の配列に1つだけ持つ
	/// 文字列や型名などをハッシュ値から整数のIDに置き換え、配列の添え字として使うために使用する
	/// HashMapはこのテーブルで、キーのハッシュ値から要素の位置(ID)を求める
	/// </summary>
	class HashTable
	{
	public:
		/// <summary>
		/// 無効なID
		/// </summary>
		static constexpr uint32_t INVALID_ID = UINT32_MAX;

		/// <summary>
		/// 使用するメモリアロケータを指定して作成する
		/// 要素を追加するまでメモリは確保しない
		/// </summary>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ。指定しなければC++標準の手段で確保する</param>
		explicit HashTable(const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: alloc_(alloc)
			, slots_(nullptr)
			, capacity_(0)
			, shift_(32)
			, idToHash_(alloc)
		{
		}

		HashTable(const HashTable& src)
			: HashTable(src.alloc_)
		{
			// 表の位置は大きさで決まるため、同じ大きさでコピーする
			if (src.idToHash_.Size() == 0 || !Rehash(src.capacity_))
			{
				return;
			}
			idToHash_.AppendRange(src.idToHash_.Begin(), src.idToHash_.Size());
			MemCopy(slots_, src.slots_, sizeof(Slot) * capacity_);
		}
		HashTable& operator=(const HashTable& src)
		{
			if (this != &src)
			{
				HashTable tmp(src);
				Swap(tmp);
			}
			return *this;
		}

		HashTable(HashTable&& src)noexcept
			: alloc_(src.alloc_)
			, slots_(src.slots_)
			, capacity_(src.capacity_)
			, shift_(src.shift_)
			, idToHash_(Move(src.idToHash_))
		{
			src.slots_ = nullptr;
			src.capacity_ = 0;
			src.shift_ = 32;
		}
		HashTable& operator=(HashTable&& src)noexcept
		{
			if (this != &src)
			{
				HashTable tmp(Move(src));
				Swap(tmp);
			}
			return *this;
		}

		~HashTable()
		{
			alloc_.Free(slots_);
		}

		void Swap(HashTable& other)noexcept
		{
			utl::Swap(alloc_, other.alloc_);
			utl::Swap(slots_, other.slots_);
			utl::Swap(capacity_, other.capacity_);
			utl::Swap(shift_, other.shift_);
			utl::Swap(idToHash_, other.idToHash_);
		}

		// 追加
		//----------------------

		/// <summary>
		/// ハッシュ値に対応するIDを取得する。なければ次の連番のIDを割り当てる
		/// </summary>
		/// <returns>ID。メモリを確保できなければINVALID_ID</returns>
		uint32_t GetOrAddId(const Hash64 hash)
		{
			const uint32_t found = FindId(hash);
			return found != INVALID_ID ? found : AddId(hash);
		}

		/// <summary>
		/// 同じハッシュ値があるかどうかを調べず、次の連番のIDを割り当てる
		/// 同じハッシュ値でも別のものとして扱いたいとき（HashMapなど）に使う
		/// </summary>
		/// <returns>ID。メモリを確保できなければINVALID_ID</returns>
		uint32_t AddId(const Hash64 hash)
		{
			const uint32_t id = idToHash_.Size();
			if (id == INVALID_ID)
			{
				return INVALID_ID;
			}
			// 追加後の使用率が1/2を超えるなら広げる
			if ((static_cast<uint64_t>(id) + 1) * 2 > capacity_)
			{
				if (capacity_ >= MAX_CAPACITY || !Rehash(capacity_ == 0 ? MIN_CAPACITY : capacity_ << 1))
				{
					return INVALID_ID;
				}
			}
			if (idToHash_.PushBack(hash) == nullptr)
			{
				return INVALID_ID;
			}
			InsertSlot(slots_, capacity_ - 1, shift_, Slot{ id, Tag(hash) });
			return id;
		}

		// 削除
		//----------------------

		/// <summary>
		/// IDを削除する
		/// IDを連番に保つため、最後のIDがこのIDの番号に移る
		/// </summary>
		/// <returns>削除したらtrue。割り当てていないIDならfalse</returns>
		bool SwapRemoveId(const uint32_t id)
		{
			const Hash64* hash = idToHash_.Get(id);
			return hash != nullptr && SwapRemoveId(id, *hash);
		}

		/// <summary>
		/// IDを削除する。そのIDのハッシュ値が分かっているときに使う（ハッシュ値を読む手間が省ける）
		/// </summary>
		/// <param name="id">割り当て済みのID</param>
		/// <param name="hash">idに対応するハッシュ値</param>
		bool SwapRemoveId(const uint32_t id, const Hash64 hash)
		{
			const uint32_t size = idToHash_.Size();
			if (id >= size)
			{
				return false;
			}
			const uint32_t last = size - 1;
			// 先にスロットを消して後ろ詰めしてから、最後のIDのスロットを探す
			EraseSlot(id, hash);
			if (id != last)
			{
				slots_[FindSlot(last)].id = id;
				*idToHash_.Get(id) = *idToHash_.Get(last);
			}
			idToHash_.PopBack();
			return true;
		}

		/// <summary>
		/// 全てのIDを破棄する。メモリは確保したままにする
		/// </summary>
		void Clear()
		{
			if (capacity_ != 0)
			{
				MemSet(slots_, 0xFF, sizeof(Slot) * capacity_);
			}
			idToHash_.Clear();
		}

		// メモリ操作
		//----------------------

		/// <summary>
		/// 再配置せずに格納できるID数を確保する
		/// </summary>
		/// <returns>確保できたらtrue</returns>
		bool Reserve(const uint32_t num)
		{
			// 使用率が1/2を超えない大きさにする
			const uint64_t need = static_cast<uint64_t>(num) * 2;
			if (need > MAX_CAPACITY)
			{
				return false;
			}
			uint32_t cap = MIN_CAPACITY;
			while (cap < need)
			{
				cap <<= 1;
			}
			idToHash_.Reserve(num);
			return cap <= capacity_ ? true : Rehash(cap);
		}

		// 取得
		//----------------------

		/// <summary>
		/// ハッシュ値に対応するIDを取得する。割り当てない
		/// </summary>
		/// <returns>ID。なければINVALID_ID</returns>
		uint32_t FindId(const Hash64 hash)const
		{
			return FindId(hash, [&](const uint32_t id) { return *idToHash_.Get(id) == hash; });
		}

		/// <summary>
		/// ハッシュ値が似ているIDのうち、条件に合うものを取得する
		/// ハッシュ値が同じでも別のものを区別したいとき（HashMapのキーの比較など）に使う
		/// 表にはハッシュ値の一部しかないため、候補のIDだけで isMatch を呼ぶ。ハッシュ値の全体の一致は isMatch で確かめること
		/// </summary>
		/// <param name="hash"></param>
		/// <param name="isMatch">IDを受け取り、探しているものならtrueを返す関数オブジェクト</param>
		/// <returns>ID。なければINVALID_ID</returns>
		template<typename MatchFunc>
		uint32_t FindId(const Hash64 hash, MatchFunc&& isMatch)const
		{
			if (idToHash_.Size() == 0)
			{
				return INVALID_ID;
			}
			const uint32_t tag = Tag(hash);
			const uint32_t mask = capacity_ - 1;
			uint32_t i = tag >> shift_;
			for (;;)
			{
				const Slot slot = slots_[i];
				if (slot.id == INVALID_ID)
				{
					return INVALID_ID;
				}
				// ハッシュ値の一部が違えば、IDの先を見ずに次へ進む
				if (slot.tag == tag && isMatch(slot.id))
				{
					return slot.id;
				}
				i = (i + 1) & mask;
			}
		}

		bool Contains(const Hash64 hash)const
		{
			return FindId(hash) != INVALID_ID;
		}

		/// <summary>
		/// IDに対応するハッシュ値を取得する
		/// </summary>
		/// <returns>ハッシュ値。割り当てていないIDならINVALID_HASH64</returns>
		Hash64 GetHash(const uint32_t id)const
		{
			const Hash64* hash = idToHash_.Get(id);
			return hash != nullptr ? *hash : INVALID_HASH64;
		}

		/// <summary>
		/// 割り当て済みのIDの数。次に割り当てるIDでもある
		/// </summary>
		uint32_t Size()const
		{
			return idToHash_.Size();
		}
		bool IsEmpty()const
		{
			return idToHash_.Size() == 0;
		}
		/// <summary>
		/// 検索の表の大きさ
		/// </summary>
		uint32_t Capacity()const
		{
			return capacity_;
		}

	private:

		// 検索の表の1マス。idがINVALID_IDなら空
		struct Slot
		{
			uint32_t id;
			// ハッシュ値の上位を混ぜた32ビット。表の位置もここから決める
			uint32_t tag;
		};

		static constexpr uint32_t MIN_CAPACITY = 8;
		static constexpr uint32_t MAX_CAPACITY = 1u << 30;

		// 下位だけが違うハッシュ値でも上位ビットに散らばるように、1回の乗算で混ぜる
		static uint32_t Tag(const Hash64 hash)
		{
			return static_cast<uint32_t>((hash * 0x9E3779B97F4A7C15ULL) >> 32);
		}

		static void InsertSlot(Slot* slots, const uint32_t mask, const uint32_t shift, const Slot slot)
		{
			uint32_t i = slot.tag >> shift;
			while (slots[i].id != INVALID_ID)
			{
				i = (i + 1) & mask;
			}
			slots[i] = slot;
		}

		// IDのスロットの位置を探す。必ず存在するIDだけを渡す
		uint32_t FindSlot(const uint32_t id)const
		{
			return FindSlot(id, *idToHash_.Get(id));
		}
		uint32_t FindSlot(const uint32_t id, const Hash64 hash)const
		{
			const uint32_t mask = capacity_ - 1;
			uint32_t i = Tag(hash) >> shift_;
			while (slots_[i].id != id)
			{
				i = (i + 1) & mask;
			}
			return i;
		}

		// スロットを消し、後ろの要素を前へ詰めて検索が途切れないようにする
		void EraseSlot(const uint32_t id, const Hash64 hash)
		{
			const uint32_t mask = capacity_ - 1;
			uint32_t hole = FindSlot(id, hash);
			slots_[hole].id = INVALID_ID;
			uint32_t j = hole;
			for (;;)
			{
				j = (j + 1) & mask;
				if (slots_[j].id == INVALID_ID)
				{
					return;
				}
				const uint32_t home = slots_[j].tag >> shift_;
				// 本来の位置が穴より前（循環を考慮）なら、穴へ移しても検索できる
				const bool movable = hole <= j ? (home <= hole || home > j) : (home <= hole && home > j);
				if (movable)
				{
					slots_[hole] = slots_[j];
					slots_[j].id = INVALID_ID;
					hole = j;
				}
			}
		}

		bool Rehash(const uint32_t newCapacity)
		{
			Slot* newSlots = alloc_.template Allocate<Slot>(newCapacity);
			if (newSlots == nullptr)
			{
				return false;
			}
			MemSet(newSlots, 0xFF, sizeof(Slot) * newCapacity);

			uint32_t log2 = 0;
			while ((1u << log2) < newCapacity)
			{
				++log2;
			}
			const uint32_t newShift = 32 - log2;
			for (uint32_t i = 0; i < capacity_; ++i)
			{
				if (slots_[i].id != INVALID_ID)
				{
					InsertSlot(newSlots, newCapacity - 1, newShift, slots_[i]);
				}
			}
			alloc_.Free(slots_);
			slots_ = newSlots;
			capacity_ = newCapacity;
			shift_ = newShift;
			return true;
		}

		MemoryAllocatorRef alloc_;
		// 検索の表。2の累乗の大きさ
		Slot* slots_;
		// 0ならメモリ未確保
		uint32_t capacity_;
		// tagから表の位置を求めるためのシフト量(32 - log2(capacity_))
		uint32_t shift_;
		// IDからハッシュ値。添え字がID
		Vector<Hash64> idToHash_;
	};
}
