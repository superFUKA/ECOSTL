#pragma once
#include "../../../Core/Hash.h"
#include "../../../Core/Utility.h"
#include "../../../Memory/Allocate/MemoryAllocatorAPI.h"
#include "../../../Type/TypeTraits.h"
#include "../../Set/HashTable.h"
#include "../../Vector/Vector.h"

namespace utl
{
	/// <summary>
	/// 整数・列挙型・ポインタのキーを64ビットのハッシュ値にする
	/// HashTableが上位ビットへ散らす処理をするため、値をそのまま使う
	/// 他の型のキーを使うときは、同じ形の関数オブジェクトをHashMapに指定する
	/// </summary>
	/// <typeparam name="Key"></typeparam>
	template<typename Key>
	struct KeyHasher
	{
		static_assert(IsIntegralV<Key> || IsEnumV<Key> || IsPointerV<Key>,
			"このキーの型には、ハッシュ関数を指定してください");

		Hash64 operator()(const Key& key)const noexcept
		{
			if constexpr (IsPointerV<Key>)
			{
				return static_cast<Hash64>(reinterpret_cast<uintptr_t>(key));
			}
			else
			{
				return static_cast<Hash64>(key);
			}
		}
	};

	/// <summary>
	/// キーと要素を詰めて並べ、HashTableでキーの位置を探すハッシュマップ
	/// 要素は連続したメモリに並ぶため、全ての要素をたどる処理が速い
	/// 要素を追加・削除すると要素のアドレスが変わることがある
	/// 削除すると最後の要素が空いた位置へ移るため、要素の並びは追加順とは限らない
	/// </summary>
	/// <typeparam name="Key">ムーブ構築できる型</typeparam>
	/// <typeparam name="Value">ムーブ構築できる型</typeparam>
	/// <typeparam name="Hasher">キーからHash64を返す関数オブジェクト</typeparam>
	template<typename Key, typename Value, typename Hasher = KeyHasher<Key>>
	class HashMap
	{
	public:
		/// <summary>
		/// 格納するキーと要素の組
		/// キーを書き換えるとマップが壊れるため、書き換えないこと
		/// </summary>
		struct Entry
		{
			template<typename...ArgTypes>
			explicit Entry(const Key& k, ArgTypes&&...args)
				: key(k)
				, value(Forward<ArgTypes>(args)...)
			{
			}
			Entry(const Entry& src)
				: key(src.key)
				, value(src.value)
			{
			}
			Entry(Entry&& src)noexcept(IsNothrowMoveConstructibleV<Key>&& IsNothrowMoveConstructibleV<Value>)
				: key(Move(src.key))
				, value(Move(src.value))
			{
			}

			Key key;
			Value value;
		};

		using Iterator = Entry*;
		using ConstIterator = const Entry*;

		// 作成
		//----------------------

		/// <summary>
		/// 使用するメモリアロケータを指定して作成する
		/// 要素を追加するまでメモリは確保しない
		/// </summary>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ。指定しなければC++標準の手段で確保する</param>
		explicit HashMap(const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: table_(alloc)
			, entries_(alloc)
		{
		}

		/// <summary>
		/// 要素数を指定して作成する
		/// </summary>
		/// <param name="reserveSize">再配置せずに格納できる要素数</param>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ</param>
		HashMap(const uint32_t reserveSize, const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: HashMap(alloc)
		{
			Reserve(reserveSize);
		}

		HashMap(const HashMap&) = default;
		HashMap& operator=(const HashMap&) = default;
		HashMap(HashMap&&)noexcept = default;
		HashMap& operator=(HashMap&&)noexcept = default;
		~HashMap() = default;

		// 追加
		//----------------------

		/// <summary>
		/// キーに紐づけて要素を追加する
		/// すでにキーが追加されていたら上書きする
		/// </summary>
		/// <param name="key"></param>
		/// <param name="...args">要素のコンストラクタ引数</param>
		/// <returns>追加した要素。確保できなければnullptr</returns>
		template<typename...ArgTypes>
		Value* Insert(const Key& key, ArgTypes&&...args)
		{
			const Hash64 hash = hasher_(key);
			const uint32_t found = FindId(key, hash);
			if (found != HashTable::INVALID_ID)
			{
				Value& value = entries_.Get(found)->value;
				value = Value(Forward<ArgTypes>(args)...);
				return &value;
			}
			Entry* entry = AddNew(key, hash, Forward<ArgTypes>(args)...);
			return entry != nullptr ? &entry->value : nullptr;
		}

		/// <summary>
		/// キーに紐づく要素を取得する。なければデフォルトの要素を追加して返す
		/// </summary>
		Value& GetOrAdd(const Key& key)
		{
			const Hash64 hash = hasher_(key);
			const uint32_t found = FindId(key, hash);
			if (found != HashTable::INVALID_ID)
			{
				return entries_.Get(found)->value;
			}
			Entry* entry = AddNew(key, hash);
			assert(entry != nullptr && "メモリを確保できませんでした");
			return entry->value;
		}

		Value& operator[](const Key& key)
		{
			return GetOrAdd(key);
		}

		// 削除
		//----------------------

		/// <summary>
		/// キーに紐づく要素を削除する
		/// 無効なキーなら何もしない
		/// </summary>
		/// <returns>削除したらtrue</returns>
		bool Remove(const Key& key)
		{
			const Hash64 hash = hasher_(key);
			const uint32_t id = FindId(key, hash);
			if (id == HashTable::INVALID_ID)
			{
				return false;
			}
			// HashTableと同じく、最後の要素を空いた位置へ移す
			const uint32_t last = entries_.Size() - 1;
			table_.SwapRemoveId(id, hash);
			if (id != last)
			{
				Entry* dest = entries_.Get(id);
				dest->~Entry();
				new (dest) Entry(Move(*entries_.Get(last)));
			}
			entries_.PopBack();
			return true;
		}

		/// <summary>
		/// 全ての要素を削除する
		/// メモリは確保したままにする
		/// </summary>
		void Clear()
		{
			table_.Clear();
			entries_.Clear();
		}

		// メモリ操作
		//----------------------

		/// <summary>
		/// 再配置せずに格納できる要素数を確保する
		/// </summary>
		/// <returns>確保できたらtrue</returns>
		bool Reserve(const uint32_t num)
		{
			entries_.Reserve(num);
			return table_.Reserve(num);
		}

		// 取得
		//----------------------

		/// <summary>
		/// キーに紐づく要素を取得する
		/// </summary>
		/// <returns>無効なキーならnullptr</returns>
		Value* Get(const Key& key)
		{
			const uint32_t id = FindId(key, hasher_(key));
			return id == HashTable::INVALID_ID ? nullptr : &entries_.Get(id)->value;
		}
		const Value* Get(const Key& key)const
		{
			const uint32_t id = FindId(key, hasher_(key));
			return id == HashTable::INVALID_ID ? nullptr : &entries_.Get(id)->value;
		}

		bool Contains(const Key& key)const
		{
			return FindId(key, hasher_(key)) != HashTable::INVALID_ID;
		}

		/// <summary>
		/// 要素数
		/// </summary>
		uint32_t Size()const
		{
			return entries_.Size();
		}
		bool IsEmpty()const
		{
			return entries_.Size() == 0;
		}
		/// <summary>
		/// 検索の表の大きさ
		/// </summary>
		uint32_t Capacity()const
		{
			return table_.Capacity();
		}

		// 反復
		//----------------------

		Iterator begin() { return entries_.Begin(); }
		Iterator end() { return entries_.End(); }
		ConstIterator begin()const { return entries_.Begin(); }
		ConstIterator end()const { return entries_.End(); }

		void Swap(HashMap& other)noexcept
		{
			table_.Swap(other.table_);
			utl::Swap(entries_, other.entries_);
			utl::Swap(hasher_, other.hasher_);
		}

	private:

		uint32_t FindId(const Key& key, const Hash64 hash)const
		{
			return table_.FindId(hash, [&](const uint32_t id) { return entries_.Get(id)->key == key; });
		}

		// キーが無いことが分かっている状態で追加する
		template<typename...ArgTypes>
		Entry* AddNew(const Key& key, const Hash64 hash, ArgTypes&&...args)
		{
			Entry* entry = entries_.EmplaceBack(key, Forward<ArgTypes>(args)...);
			if (entry == nullptr)
			{
				return nullptr;
			}
			if (table_.AddId(hash) == HashTable::INVALID_ID)
			{
				entries_.PopBack();
				return nullptr;
			}
			return entry;
		}

		// ハッシュ値からIDを求める表。IDは要素の並びの添え字と同じ
		HashTable table_;
		// 要素の並び
		Vector<Entry> entries_;
		Hasher hasher_;
	};
}
