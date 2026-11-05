#pragma once
// ハッシュ値(またはキーから求めたハッシュ値)で要素を引く密なマップ DenceDirectHashMap / DenceHashMap
#include "ECOSTL/Core/Template/Template.h"
#include "../../Set/RobinHoodTable.h"
#include "../DenceMap.h"
#include "../../Vector/Vector.h"

// ecoSTL の共通名前空間
namespace utl
{
	// DenceHashMap の内部用(外部からは使わない)
	namespace detail
	{
		// data()とsize()で中身の連続したメモリを取得できる型(std::string、std::string_viewなど)
		template<typename Type>
		concept DenceHashContiguousKey = requires(const Type& key)
		{
			{ key.data() };
			{ key.size() };
		};

		// メモリの並びがそのまま値を表し、バイト列からハッシュ値を求められる型
		template<typename Type>
		inline constexpr bool IsBytewiseHashable = utl::IsTriviallyCopyableV<Type> && utl::HasUniqueObjectRepresentationsV<Type>;

	}

	/// <summary>
	/// DenceHashMapのキーからハッシュ値を求めるデフォルトの方法
	/// オブジェクトのメモリをそのまま使うと、文字列はポインタや未初期化領域、構造体はパディングを含んでしまうため、
	/// 型ごとに値そのものからハッシュ値を求める
	/// </summary>
	/// <typeparam name="KeyType"></typeparam>
	template<typename KeyType>
	struct DenceHashMapDefaultPolicy
	{
		Hash64 operator()(const KeyType& key)const
		{
			if constexpr (detail::DenceHashContiguousKey<KeyType>)
			{
				// 連続したメモリにある中身からハッシュ値を求める
				using ElementType = utl::RemoveCVType<utl::RemovePointerType<decltype(key.data())>>;
				static_assert(detail::IsBytewiseHashable<ElementType>, "DenceHashMapDefaultPolicy: 要素がバイト列からハッシュ値を求められない型です。ハッシュの方法を指定してください。");
				const uint8_t* begin = reinterpret_cast<const uint8_t*>(key.data());
				return Hash64FromBytes(begin, static_cast<uint64_t>(key.size()) * sizeof(ElementType));
			}
			else if constexpr (utl::IsFloatingPointV<KeyType>)
			{
				// 0.0と-0.0は等しいため同じハッシュ値にする
				KeyType normalized = (key == static_cast<KeyType>(0)) ? static_cast<KeyType>(0) : key;
				return Hash64FromType<KeyType>(normalized);
			}
			else if constexpr (detail::IsBytewiseHashable<KeyType>)
			{
				return Hash64FromType<KeyType>(key);
			}
			else
			{
				static_assert(utl::DependentFalseV<KeyType>, "DenceHashMapDefaultPolicy: バイト列からハッシュ値を求められない型です。ハッシュの方法を指定してください。");
				return INVALID_HASH64;
			}
		}
	};

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

		DenceDirectHashMap(const uint32_t capacity, MemoryAllocatorDelegate* alloc = nullptr)
			:table_(capacity)
			, indexToValue_(capacity, alloc)
		{

		}

		DenceDirectHashMap(const uint32_t capacity, MemoryAllocatorDelegate* utlAlloc, MemoryAllocatorDelegate* valueAlloc)
			:table_(capacity)
			, indexToValue_(capacity, utlAlloc, valueAlloc)
		{

		}

		//------------------------------------------
		// 追加
		//------------------------------------------


		// ハッシュ値 key に value のコピーを紐づけて追加し、追加した要素のポインタを返す。平均 O(1)
		ValueType* Insert(Hash64 key, const ValueType& value)
		{
			RHID id = table_.Insert(key);
			return indexToValue_.Insert(id.index, value);
		}

		// ハッシュ値 key に args から構築した要素を追加し、そのポインタを返す。平均 O(1)
		template<typename...ArgTypes>
		ValueType* Insert(Hash64 key, ArgTypes&&...args)
		{
			RHID id = table_.Insert(key);
			return indexToValue_.Insert(id.index, args...);
		}


		//------------------------------------------
		// 取得
		//------------------------------------------

		// key の要素ポインタを返す。未登録なら nullptr。平均 O(1)
		ValueType* Find(Hash64 key)
		{
			RHID id = table_.Find(key);
			// 無効なIDならnullptrを返す
			if (!table_.CheckActive(id))
			{
				return nullptr;
			}

			return indexToValue_.Find(id.index);
		}
		const ValueType* Find(Hash64 key)const
		{
			return const_cast<utl::RemoveConstType<utl::RemoveReferenceType<decltype(*this)>>*>(this)->Find(key);
		}


		//------------------------------------------
		// 削除
		//------------------------------------------

		// key とその要素を削除する。平均 O(1)
		void Remove(Hash64 key)
		{
			RHID id = table_.Find(key);

			table_.Remove(key);
			indexToValue_.Remove(id.index);
		}





	private:

		// ハッシュ値を一意で連番なIDに変換する
		RobinHoodHashTable table_; // ハッシュ値から要素の密配列上の位置を決める表

		DenceMap<ValueType> indexToValue_; // table_ が返したインデックスから引ける要素本体

	};

	/// <summary>
	/// ハッシュ値
	/// </summary>
	/// <typeparam name="KeyType"></typeparam>
	/// <typeparam name="ValueType"></typeparam>
	/// <typeparam name="HashPolicy">キーからハッシュ値を求める方法 デフォルトは値そのものから求める</typeparam>
	template<typename KeyType, typename ValueType, typename HashPolicy = DenceHashMapDefaultPolicy<KeyType>>
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

		DenceHashMap(const uint32_t capacity, MemoryAllocatorDelegate* alloc = nullptr)
			:directMap_(capacity, alloc)
		{

		}

		DenceHashMap(const uint32_t capacity, MemoryAllocatorDelegate* utlAlloc, MemoryAllocatorDelegate* valueAlloc)
			:directMap_(capacity, utlAlloc, valueAlloc)
		{

		}


		//------------------------------------------
		// 追加
		//------------------------------------------


		ValueType* Insert(const KeyType& key, const ValueType& value)
		{
			Hash64 hash = hasher_(key);
			return directMap_.Insert(hash, value);
		}

		template<typename...ArgTypes>
		ValueType* Insert(const KeyType& key, ArgTypes&&...args)
		{
			Hash64 hash = hasher_(key);
			return directMap_.Insert(hash, args...);
		}


		//------------------------------------------
		// 取得
		//------------------------------------------

		ValueType* Find(const KeyType& key)
		{
			Hash64 hash = hasher_(key);
			return directMap_.Find(hash);
		}
		const ValueType* Find(const KeyType& key)const
		{
			return const_cast<utl::RemoveConstType<utl::RemoveReferenceType<decltype(*this)>>*>(this)->Find(key);
		}


		//------------------------------------------
		// 削除
		//------------------------------------------

		void Remove(const KeyType& key)
		{
			Hash64 hash = hasher_(key);
			return directMap_.Remove(hash);
		}





	private:

		DenceDirectHashMap<ValueType> directMap_;

		// キーからハッシュ値を求める方法
		HashPolicy hasher_;

	};
}