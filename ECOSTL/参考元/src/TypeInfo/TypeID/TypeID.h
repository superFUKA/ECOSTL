#pragma once
#include <atomic>
#include <string>
#include <unordered_map>

#if defined(_MSC_VER)
# define TYPE_FUNC_NAME __FUNCSIG__
#else
# define TYPE_FUNC_NAME __PRETTY_FUNCTION__
#endif

namespace utl
{
	// 型ごとに一意なID
	using TypeID = uint32_t;

	/// <summary>
	/// 異なる静的領域でも同じ型に同じIDを割り当てるためのクラス
	/// </summary>
	class TypeIDAllocator
	{
	public:

		/// <summary>
		/// 
		/// </summary>
		/// <typeparam name="Type"></typeparam>
		/// <returns></returns>
		template<typename...Types>
		TypeID GetID()
		{
			const char* keyName = TYPE_FUNC_NAME;
			auto itr = umNameToTypeID_.find(keyName);
			if (itr == umNameToTypeID_.end())
			{
				Allocate(keyName);
				itr = umNameToTypeID_.find(keyName);
			}
			return itr->second;
		}

	private:

		/// <summary>
		/// 新しい型IDを割り当てる
		/// </summary>
		/// <param name="a_keyName"></param>
		void Allocate(const char* a_keyName);

	private:

		std::atomic<TypeID> counter_ = 0;

		std::unordered_map<const char*, TypeID> umNameToTypeID_;
	};

	/// <summary>
	/// 
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename...Types>
	class TypeIDContainer
	{
	private:
		template<typename...T>
		friend TypeID GetTypeID(TypeIDAllocator& a_allocator);

		TypeIDContainer()
		{
		}

	public:

		/// <summary>
		/// 
		/// </summary>
		/// <param name="a_allocator"></param>
		/// <returns></returns>
		inline TypeID GetID(TypeIDAllocator& a_allocator)
		{
			if (id_ == 0)
			{
				id_ = a_allocator.GetID<Types...>();
			}
			return id_;
		}

		inline TypeID GetID()
		{
			return id_;
		}

	private:

		TypeID id_ = 0;

	};

	class StaticTypeIDAllocator
	{
	public:
		inline static TypeID Allocate()
		{
			return nextGeneID_++;
		}

	private:

		inline static TypeID nextGeneID_ = 0;
	};


	template<typename...Types>
	struct StaticTypeIDContainer
	{
		inline static TypeID id = StaticTypeIDAllocator::Allocate();
	};

	/// <summary>
	/// 静的領域間で同期する型IDを取得する
	/// 共通のTypeIDAllocatorを渡す必要がある
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="a_pAllocator"></param>
	/// <returns></returns>
	template<typename...Types>
	inline static TypeID GetTypeID(TypeIDAllocator& allocator)
	{
		static TypeIDContainer<Types...> idContainer;
		return idContainer.GetID(allocator);
	}

	/// <summary>
	/// 静的領域間で同期しない型IDを取得する
	/// </summary>
	/// <typeparam name="...Types"></typeparam>
	/// <returns></returns>
	template<typename...Types>
	inline static TypeID GetTypeID()
	{
		return StaticTypeIDContainer<Types...>::id;
	}

}