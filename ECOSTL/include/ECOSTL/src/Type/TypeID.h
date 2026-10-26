#pragma once
#include <cassert>
#include <cstdint>
#include "../Container/Map/Hash/HashMap.h"
#include "../Core/Hash.h"

#if defined(_MSC_VER)
# define TYPE_FUNC_NAME __FUNCSIG__
#else
# define TYPE_FUNC_NAME __PRETTY_FUNCTION__
#endif

namespace utl
{
	// 型ごとに一意なID (0は無効値)
	using TypeID = uint32_t;

	/// <summary>
	/// 無効な型ID
	/// </summary>
	inline constexpr TypeID INVALID_TYPE_ID = 0;

	/// <summary>
	/// 異なる静的領域でも同じ型に同じIDを割り当てるためのクラス
	/// スレッドセーフではない
	/// 新しい型の登録(初回のGetID)は1つのスレッドで順番に行うこと
	/// 登録済みの型のIDを引くだけなら、複数スレッドから呼んでよい
	/// 型の組の順序が違えば別の型として扱う
	/// </summary>
	class TypeIDAllocator
	{
	public:

		TypeIDAllocator() = default;
		// コピーするとIDの空間が分かれてしまうため禁止する
		TypeIDAllocator(const TypeIDAllocator&) = delete;
		TypeIDAllocator& operator=(const TypeIDAllocator&) = delete;
		TypeIDAllocator(TypeIDAllocator&&) = delete;
		TypeIDAllocator& operator=(TypeIDAllocator&&) = delete;

		/// <summary>
		/// 型の組に対応するIDを取得する。未登録なら新しく割り当てる
		/// 関数シグネチャから型名の部分だけを取り出してキーにする
		/// </summary>
		/// <typeparam name="Types">IDを取得する型</typeparam>
		/// <returns>型ID (1以上)</returns>
		template<typename...Types>
		TypeID GetID()
		{
			const char* name = TYPE_FUNC_NAME;
			const char* begin = name;
			const char* end = name;
			while (*end != '\0')
			{
				++end;
			}
#if defined(_MSC_VER)
			// "...TypeIDAllocator::GetID<型名>(void)" の <> の中
			const char* last = end;
			while (last != name && *last != '>')
			{
				--last;
			}
			end = last;
			for (const char* p = name; p != end; ++p)
			{
				if (*p == '<')
				{
					begin = p + 1;
					break;
				}
			}
#else
			// "...GetID() [with Types = {型名}]" の "Types = " から最後の ']' の手前まで
			const char* last = end;
			while (last != name && *last != ']')
			{
				--last;
			}
			end = last;
			for (const char* p = name; p + 8 <= end; ++p)
			{
				if (p[0] == 'T' && p[1] == 'y' && p[2] == 'p' && p[3] == 'e' && p[4] == 's' && p[5] == ' ' && p[6] == '=' && p[7] == ' ')
				{
					begin = p + 8;
					break;
				}
			}
#endif
			return GetID(begin, static_cast<size_t>(end - begin));
		}

	private:

		/// <summary>
		/// 型を一意に表す文字列に対応するIDを取得する。未登録なら新しく割り当てる
		/// </summary>
		/// <param name="a_keyName">文字列の先頭</param>
		/// <param name="a_length">文字列の長さ</param>
		/// <returns>型ID (1以上)</returns>
		TypeID GetID(const char* a_keyName, const size_t a_length)
		{
			const Hash64 key = Hash64FromBytes(reinterpret_cast<const uint8_t*>(a_keyName), a_length);
			if (const TypeID* found = umNameToTypeID_.Get(key))
			{
				return *found;
			}
			const TypeID id = counter_ + 1;
			TypeID* inserted = umNameToTypeID_.Insert(key, id);
			assert(inserted != nullptr && "メモリを確保できませんでした");
			(void)inserted;
			counter_ = id;
			return id;
		}

		/// <summary>
		/// 終端が'\0'の文字列に対応するIDを取得する
		/// </summary>
		TypeID GetID(const char* a_keyName)
		{
			size_t length = 0;
			while (a_keyName[length] != '\0')
			{
				++length;
			}
			return GetID(a_keyName, length);
		}

		TypeID counter_ = INVALID_TYPE_ID;

		// 型名の文字列の中身から求めたハッシュ値(64bit)をキーにする
		// ポインタのアドレスは静的領域ごとに違うため使わない。衝突は無視できる確率として扱う
		HashMap<Hash64, TypeID> umNameToTypeID_;
	};

	/// <summary>
	/// 静的領域ごとに型IDをキャッシュするコンテナ
	/// </summary>
	/// <typeparam name="Types">IDを保持する型</typeparam>
	template<typename...Types>
	class TypeIDContainer
	{
	public:

		/// <summary>
		/// 型IDを取得する。未取得ならアロケータから取得してキャッシュする
		/// </summary>
		/// <param name="a_allocator">共通のアロケータ</param>
		/// <returns>型ID</returns>
		inline TypeID GetID(TypeIDAllocator& a_allocator)
		{
			if (id_ == INVALID_TYPE_ID)
			{
				id_ = a_allocator.GetID<Types...>();
			}
			return id_;
		}

		/// <summary>
		/// キャッシュ済みの型IDを取得する (未取得なら無効値)
		/// </summary>
		inline TypeID GetID() const
		{
			return id_;
		}

	private:

		TypeID id_ = INVALID_TYPE_ID;
	};

	/// <summary>
	/// 静的領域内でのみ一意な型IDを割り当てるクラス
	/// </summary>
	class StaticTypeIDAllocator
	{
	public:
		inline static TypeID Allocate()
		{
			static TypeID nextID = INVALID_TYPE_ID;
			return ++nextID;
		}
	};

	/// <summary>
	/// 静的領域間で同期する型IDを取得する
	/// 共通のTypeIDAllocatorを渡す必要がある
	/// 同期しない GetTypeID&lt;Types...&gt;() が返すIDとは無関係なので、混ぜて使わないこと
	/// </summary>
	/// <typeparam name="Types">IDを取得する型</typeparam>
	/// <param name="a_allocator">共通のアロケータ</param>
	/// <returns>型ID</returns>
	template<typename...Types>
	inline TypeID GetTypeID(TypeIDAllocator& a_allocator)
	{
		static TypeIDContainer<Types...> idContainer;
		return idContainer.GetID(a_allocator);
	}

	/// <summary>
	/// 静的領域間で同期しない型IDを取得する
	/// 初回の呼び出しで割り当てるため、静的初期化の順序に依存しない
	/// アロケータを渡す GetTypeID&lt;Types...&gt;(allocator) が返すIDとは無関係なので、混ぜて使わないこと
	/// </summary>
	/// <typeparam name="Types">IDを取得する型</typeparam>
	/// <returns>型ID</returns>
	template<typename...Types>
	inline TypeID GetTypeID()
	{
		static const TypeID id = StaticTypeIDAllocator::Allocate();
		return id;
	}

}
