#pragma once
#include <cassert>
#include "../../Allocator/MemoryAllocator.h"

namespace utl
{

	struct RefCounter
	{
		RefCounter() = default;

		RefCounter(void* ptr, size_t size, MemoryAllocatorAPI* alloc)
			: ptr_(ptr)
			, alloc_(alloc)
			, size_(size)
			, counter_(0)
		{

		}

		void* ptr_;

		size_t size_;

		MemoryAllocatorAPIHolder alloc_;

		int counter_;
	};
	template<typename Type>
	struct RefCounterToIns
	{
	public:
		RefCounterToIns() = delete;

		Type ins;
		RefCounter typeCounter;
	};

	template<typename Type, typename...ArgTypes>
	inline RefCounter* MakeRefCounterWithAlloc(MemoryAllocatorAPI* alloc, ArgTypes&&...args)
	{
		MemoryAllocatorAPIHolder allocHolder(alloc);

		// RefCounterToInsを使い、RefCounterとTypeのメモリを同時に確保
		void* mem = allocHolder.Allocate(sizeof(RefCounterToIns<Type>));
		RefCounterToIns<Type>* allocResult = static_cast<RefCounterToIns<Type>*>(mem);
		// 確保したメモリにインスタンスを作成
		new (&allocResult->ins) Type(args...);
		new (&allocResult->typeCounter) RefCounter(&allocResult->ins, sizeof(Type), alloc);

		return &allocResult->typeCounter;
	}
	/// <summary>
	/// 共有ポインタ
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class SharedPtr
	{
	private:
		template<typename SrcType>
		friend class SharedPtr;




	private:

		//---------------------------------
		// 作成
		//---------------------------------

		template<typename SrcType, typename...ArgTypes>
		friend SharedPtr<SrcType> MakeSharedWithAlloc(MemoryAllocatorAPI* alloc, ArgTypes&&...args);

		/// <summary>
		/// リソースのカウンタを渡して作成するコンストラクタ
		/// </summary>
		/// <param name="ref"></param>
		SharedPtr(RefCounter* ref)
			:ref_(ref)
		{
			++ref->counter_;
		}

	public:

		/// <summary>
		/// 空の共有ポインタを作成するコンストラクタ
		/// </summary>
		SharedPtr()
			:ref_(nullptr)
		{

		}


		//---------------------------------
		// 代入
		//---------------------------------



		/// <summary>
		/// 共有ポインタをコピーして作成するコンストラクタ
		/// 共有カウントは増える
		/// </summary>
		/// <param name="src"></param>
		SharedPtr(const SharedPtr& src)
			:ref_(src.ref_)
		{
			if (ref_ == nullptr)
			{
				return;
			}

			++ref_->counter_;
		}
		/// <summary>
		/// 共有ポインタをコピーする
		/// 共有カウントは増える
		/// すでに管理してたら参照を外す
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		SharedPtr<Type>& operator=(const SharedPtr& src)
		{
			// 自分自身を代入してないか確認
			if (this == &src || this->ref_ == src.ref_)
			{
				return *this;
			}

			// すでに扱ってるリソースがあったら開放
			Reset();

			ref_ = src.ref_;
			if (ref_ == nullptr)
			{
				return *this;
			}

			++ref_->counter_;
			return *this;
		}


		/// <summary>
		/// 派生クラスからキャストしてムーブ
		/// </summary>
		/// <typeparam name="BaseType"></typeparam>
		/// <param name=""></param>
		template<typename SubType, std::enable_if_t<std::is_convertible_v<SubType*, Type*>, int> = 0>
		SharedPtr(SharedPtr<SubType>&& src) noexcept
			:ref_(nullptr)
		{
			ref_ = src.ref_;
			src.ref_ = nullptr;
		}

		template<typename SubType, std::enable_if_t<std::is_convertible_v<SubType*, Type*>, int> = 0>
		SharedPtr& operator=(SharedPtr<SubType>&& src)noexcept
		{
			// すでに扱ってるリソースがあったら開放
			Reset();

			ref_ = src.ref_;
			src.ref_ = nullptr;
			return *this;
		}


		SharedPtr(nullptr_t)noexcept
			:ref_(nullptr)
		{

		}
		SharedPtr& operator=(nullptr_t)noexcept
		{
			Reset();
			return *this;
		}

		//---------------------------------
		// 開放
		//---------------------------------

		/// <summary>
		/// 参照をやめる
		/// インスタンスを参照してる存在がいなくなれば開放する
		/// </summary>
		void Reset()
		{
			if (ref_ == nullptr)
			{
				return;
			}

			--ref_->counter_;

			// カウンタが０以下になったらインスタンスを開放する
			if (ref_->counter_ <= 0)
			{
				assert(ref_->counter_ >= 0 && "参照カウンタが０より小さいです");
				static_cast<Type*>(ref_->ptr_)->~Type();
				// Ref事態も同じアロケータの同じメモリブロックに制しえされているためこれでまとめて開放
				ref_->alloc_.Free(ref_->ptr_);
			}

			ref_ = nullptr;
		}

		~SharedPtr()
		{
			Reset();
		}

		//---------------------------------
		// 取得
		//---------------------------------

		/// <summary>
		/// 管理してるインスタンスのポインタを返す
		/// </summary>
		/// <returns></returns>
		Type* Get()const
		{
			if (ref_ == nullptr)
			{
				return nullptr;
			}
			return static_cast<Type*>(ref_->ptr_);
		}

		/// <summary>
		/// 管理してるインスタンスのポインタにアクセスする
		/// </summary>
		/// <returns></returns>
		Type* operator->()const
		{
			assert(ref_ != nullptr && "nullptrの実体にアクセスしようとしています");
			assert(ref_->ptr_ != nullptr && "nullptrの実体にアクセスしようとしています");
			return Get();
		}

		/// <summary>
		/// 管理しているインスタンスの実体を返す
		/// </summary>
		/// <returns></returns>
		Type& operator*()const
		{
			assert(ref_ != nullptr && "nullptrの実体にアクセスしようとしています");
			return *Get();
		}

		/// <summary>
		/// 有効なインスタンスを管理しているか
		/// </summary>
		operator bool()const
		{
			if (ref_ == nullptr)
			{
				return false;
			}
			return ref_->GetPtr() != nullptr;
		}

		//---------------------------------
		// 比較
		//---------------------------------

		/// <summary>
		/// 生ポインタと管理しているインスタンスのポインタが一致すればtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		const bool operator==(const Type* ptr)const
		{
			if (ref_ == nullptr)
			{
				return false;
			}

			return ref_->GetPtr() == ptr;
		}
		/// <summary>
		/// SharedPtrが同じインスタンスを管理していればtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		const bool operator==(const SharedPtr<Type>& ptr)const
		{
			if (ref_ == ptr.ref_)
			{
				return true;
			}

			if (ptr.ref_ == nullptr)
			{
				return false;
			}

			return (*this) == ptr.Get();
		}

		/// <summary>
		/// 生ポインタと管理しているインスタンスのポインタが一致しなければtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		const bool operator!=(const Type* ptr)const
		{
			return !((*this) == ptr);
		}
		/// <summary>
		/// SharedPtrが同じインスタンスを管理していなければtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		const bool operator!=(const SharedPtr<Type>& ptr)const
		{
			return !((*this) == ptr);
		}

	private:

		RefCounter* ref_;

	};


	/// <summary>
	/// アロケータを指定してSharedPtrを作成する
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="...ArgTypes"></typeparam>
	/// <param name="alloc"></param>
	/// <param name="...args"></param>
	/// <returns></returns>
	template<typename Type, typename...ArgTypes>
	inline SharedPtr<Type> MakeSharedWithAlloc(MemoryAllocatorAPI* alloc, ArgTypes&&...args)
	{
		RefCounter* counter = MakeRefCounterWithAlloc<Type>(alloc, args...);
		// Viewを作る
		return SharedPtr<Type>(counter);
	}

	/// <summary>
	/// SharedPtrを作成する
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="...ArgTypes"></typeparam>
	/// <param name="...args"></param>
	/// <returns></returns>
	template<typename Type, typename...ArgTypes>
	inline SharedPtr<Type> MakeShared(ArgTypes&&...args)
	{
		return MakeSharedWithAlloc<Type>(nullptr, args...);
	}
}
