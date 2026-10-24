#pragma once
#include "../../Set/RobinHoodTable.h"
#include "../DenceMap.h"
#include "../../Vector/Vector.h"

namespace utl
{
	/// <summary>
	/// ハッシュ値を直接キーとして使い、
	/// </summary>
	/// <typeparam name="ValueType"></typeparam>
	template<typename ValueType>
	class DenceDirectHashMap
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		DenceDirectHashMap()
			:table_(8)
			, indexToValue_(8)
		{

		}

		DenceDirectHashMap(const uint32_t capacity, MemoryAllocatorAPI* alloc = nullptr)
			:table_(capacity)
			, indexToValue_(capacity, alloc)
		{

		}

		DenceDirectHashMap(const uint32_t capacity, MemoryAllocatorAPI* utlAlloc, MemoryAllocatorAPI* valueAlloc)
			:table_(capacity)
			, indexToValue_(capacity, utlAlloc, valueAlloc)
		{

		}

		//------------------------------------------
		// 追加
		//------------------------------------------


		ValueType* Insert(Hash64 key, const ValueType& value)
		{
			RHID id = table_.Insert(key);
			return indexToValue_.Insert(id.index, value);
		}

		template<typename...ArgTypes>
		ValueType* Insert(Hash64 key, ArgTypes&&...args)
		{
			RHID id = table_.Insert(key);
			return indexToValue_.Insert(id.index, args...);
		}


		//------------------------------------------
		// 取得
		//------------------------------------------

		ValueType* Find(Hash64 key)const
		{
			RHID id = table_.Find(key);
			// 無効なIDならnullptrを返す
			if (!table_.CheckActive(id))
			{
				return nullptr;
			}

			return indexToValue_.Get(id.index);
		}


		//------------------------------------------
		// 削除
		//------------------------------------------

		void Remove(Hash64 key)
		{
			RHID id = table_.Find(key);

			table_.Remove(key);
			indexToValue_.Remove(id.index);
		}





	private:

		// ハッシュ値を一意で連番なIDに変換する
		RobinHoodHashTable table_;

		DenceMap<ValueType> indexToValue_;

	};

	/// <summary>
	/// ハッシュ値
	/// </summary>
	/// <typeparam name="KeyType"></typeparam>
	/// <typeparam name="ValueType"></typeparam>
	template<typename KeyType, typename ValueType>
	class DenceHashMap
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		DenceHashMap()
			:directMap_()
		{

		}

		DenceHashMap(const uint32_t capacity, MemoryAllocatorAPI* alloc = nullptr)
			:directMap_(capacity, alloc)
		{

		}

		DenceHashMap(const uint32_t capacity, MemoryAllocatorAPI* utlAlloc, MemoryAllocatorAPI* valueAlloc)
			:directMap_(capacity, utlAlloc, valueAlloc)
		{

		}


		//------------------------------------------
		// 追加
		//------------------------------------------


		ValueType* Insert(const KeyType& key, const ValueType& value)
		{
			Hash64 hash = Hash64FromType<KeyType>(key);
			return directMap_.Insert(hash, value);
		}

		template<typename...ArgTypes>
		ValueType* Insert(const KeyType& key, ArgTypes&&...args)
		{
			Hash64 hash = Hash64FromType<KeyType>(key);
			return directMap_.Insert(hash, args...);
		}


		//------------------------------------------
		// 取得
		//------------------------------------------

		ValueType* Find(const KeyType& key)const
		{
			Hash64 hash = Hash64FromType<KeyType>(key);
			return directMap_.Find(hash);
		}


		//------------------------------------------
		// 削除
		//------------------------------------------

		void Remove(const KeyType& key)
		{
			Hash64 hash = Hash64FromType<KeyType>(key);
			return directMap_.Remove(hash);
		}





	private:

		DenceDirectHashMap<ValueType> directMap_;

	};
}