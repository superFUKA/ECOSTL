#pragma once
#include <cassert>
#include "../../Core/Utility.h"
#include "../../Type/Type.h"
#include "../../Type/TypeTraits.h"
#include "../Allocate/MemoryAllocatorAPI.h"

namespace utl
{
	/// <summary>
	/// 共有しているインスタンスの参照カウンタ
	/// 参照カウンタの増減はスレッドセーフではない
	/// </summary>
	struct RefCounter
	{
		RefCounter(void* block, DeleteFunc deleteFunc, MemoryAllocatorDelegate* alloc)
			: block_(block)
			, delete_(deleteFunc)
			, alloc_(alloc)
			, counter_(0)
		{

		}

		// インスタンスとカウンタをまとめて確保したメモリブロックの先頭
		void* block_;

		// 作成したときの型でメモリブロックを破棄して開放する関数
		DeleteFunc delete_;

		MemoryAllocatorRef alloc_;

		int counter_;
	};

	/// <summary>
	/// インスタンスと参照カウンタを1回で確保するためのまとめ
	/// </summary>
	template<typename Type>
	struct RefCounterToIns
	{
	public:
		template<typename...ArgTypes>
		RefCounterToIns(MemoryAllocatorDelegate* alloc, ArgTypes&&...args)
			: ins(Forward<ArgTypes>(args)...)
			, typeCounter(this, &DeleteAs<RefCounterToIns>, alloc)
		{
		}

		Type ins;
		RefCounter typeCounter;
	};

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
		friend SharedPtr<SrcType> MakeSharedWithAlloc(MemoryAllocatorDelegate* alloc, ArgTypes&&...args);

		/// <summary>
		/// インスタンスとリソースのカウンタを渡して作成するコンストラクタ
		/// </summary>
		/// <param name="ptr"></param>
		/// <param name="ref"></param>
		SharedPtr(Type* ptr, RefCounter* ref)
			: ptr_(ptr)
			, ref_(ref)
		{
			++ref->counter_;
		}

	public:

		/// <summary>
		/// 空の共有ポインタを作成するコンストラクタ
		/// </summary>
		SharedPtr()noexcept
			: ptr_(nullptr)
			, ref_(nullptr)
		{

		}

		SharedPtr(NullptrT)noexcept
			: SharedPtr()
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
		SharedPtr(const SharedPtr& src)noexcept
			: ptr_(src.ptr_)
			, ref_(src.ref_)
		{
			AddRef();
		}
		/// <summary>
		/// 派生クラスの共有ポインタをキャストしてコピー
		/// 共有カウントは増える
		/// </summary>
		/// <typeparam name="SubType"></typeparam>
		/// <param name="src"></param>
		template<typename SubType, EnableIfType<IsConvertibleV<SubType*, Type*>, int> = 0>
		SharedPtr(const SharedPtr<SubType>& src)noexcept
			: ptr_(src.ptr_)
			, ref_(src.ref_)
		{
			AddRef();
		}

		/// <summary>
		/// 共有ポインタをムーブして作成するコンストラクタ
		/// 共有カウントは変わらない
		/// </summary>
		/// <param name="src"></param>
		SharedPtr(SharedPtr&& src)noexcept
			: ptr_(src.ptr_)
			, ref_(src.ref_)
		{
			src.ptr_ = nullptr;
			src.ref_ = nullptr;
		}
		/// <summary>
		/// 派生クラスからキャストしてムーブ
		/// </summary>
		/// <typeparam name="SubType"></typeparam>
		/// <param name="src"></param>
		template<typename SubType, EnableIfType<IsConvertibleV<SubType*, Type*>, int> = 0>
		SharedPtr(SharedPtr<SubType>&& src)noexcept
			: ptr_(src.ptr_)
			, ref_(src.ref_)
		{
			src.ptr_ = nullptr;
			src.ref_ = nullptr;
		}

		/// <summary>
		/// 共有ポインタをコピーする
		/// 共有カウントは増える
		/// すでに管理してたら参照を外す
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		SharedPtr& operator=(const SharedPtr& src)noexcept
		{
			// 一時オブジェクトと入れ替えるので、自分自身を代入しても安全
			SharedPtr(src).Swap(*this);
			return *this;
		}
		template<typename SubType, EnableIfType<IsConvertibleV<SubType*, Type*>, int> = 0>
		SharedPtr& operator=(const SharedPtr<SubType>& src)noexcept
		{
			SharedPtr(src).Swap(*this);
			return *this;
		}

		SharedPtr& operator=(SharedPtr&& src)noexcept
		{
			SharedPtr(Move(src)).Swap(*this);
			return *this;
		}
		template<typename SubType, EnableIfType<IsConvertibleV<SubType*, Type*>, int> = 0>
		SharedPtr& operator=(SharedPtr<SubType>&& src)noexcept
		{
			SharedPtr(Move(src)).Swap(*this);
			return *this;
		}

		SharedPtr& operator=(NullptrT)noexcept
		{
			Reset();
			return *this;
		}

		/// <summary>
		/// 管理しているインスタンスを入れ替える
		/// </summary>
		/// <param name="other"></param>
		void Swap(SharedPtr& other)noexcept
		{
			utl::Swap(ptr_, other.ptr_);
			utl::Swap(ref_, other.ref_);
		}

		//---------------------------------
		// 開放
		//---------------------------------

		/// <summary>
		/// 参照をやめる
		/// インスタンスを参照してる存在がいなくなれば、作成したときの型で開放する
		/// </summary>
		void Reset()noexcept
		{
			if (ref_ == nullptr)
			{
				return;
			}

			// 先に空にしてから破棄する（破棄中にこのポインタが使われても開放済みのものを指さない）
			RefCounter* ref = ref_;
			ptr_ = nullptr;
			ref_ = nullptr;

			--ref->counter_;
			assert(ref->counter_ >= 0 && "参照カウンタが０より小さいです");

			// カウンタが０以下になったらインスタンスを開放する
			if (ref->counter_ <= 0)
			{
				// カウンタ自体も同じメモリブロックにあり一緒に破棄されるため、アロケータを先に取り出しておく
				MemoryAllocatorRef alloc = ref->alloc_;
				const bool freed = ref->delete_(alloc, ref->block_);
				assert(freed && "メモリの開放に失敗しました");
				(void)freed;
			}
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
			return ptr_;
		}

		/// <summary>
		/// 管理してるインスタンスのポインタにアクセスする
		/// </summary>
		/// <returns></returns>
		Type* operator->()const
		{
			assert(ptr_ != nullptr && "nullptrの実体にアクセスしようとしています");
			return Get();
		}

		/// <summary>
		/// 管理しているインスタンスの実体を返す
		/// </summary>
		/// <returns></returns>
		Type& operator*()const
		{
			assert(ptr_ != nullptr && "nullptrの実体にアクセスしようとしています");
			return *Get();
		}

		/// <summary>
		/// 有効なインスタンスを管理しているか
		/// </summary>
		explicit operator bool()const
		{
			return ptr_ != nullptr;
		}

		/// <summary>
		/// 管理しているインスタンスを参照している数（空なら0）
		/// </summary>
		/// <returns></returns>
		int UseCount()const
		{
			if (ref_ == nullptr)
			{
				return 0;
			}
			return ref_->counter_;
		}

		//---------------------------------
		// 比較
		//---------------------------------

		/// <summary>
		/// 生ポインタと管理しているインスタンスのポインタが一致すればtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		bool operator==(const Type* ptr)const
		{
			return ptr_ == ptr;
		}
		/// <summary>
		/// SharedPtrが同じインスタンスを管理していればtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		bool operator==(const SharedPtr<Type>& ptr)const
		{
			return ptr_ == ptr.ptr_;
		}

		/// <summary>
		/// 生ポインタと管理しているインスタンスのポインタが一致しなければtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		bool operator!=(const Type* ptr)const
		{
			return !((*this) == ptr);
		}
		/// <summary>
		/// SharedPtrが同じインスタンスを管理していなければtrue
		/// </summary>
		/// <param name="ptr"></param>
		/// <returns></returns>
		bool operator!=(const SharedPtr<Type>& ptr)const
		{
			return !((*this) == ptr);
		}

	private:

		void AddRef()noexcept
		{
			if (ref_ != nullptr)
			{
				++ref_->counter_;
			}
		}

		// 管理しているインスタンス（派生クラスから変換すると基底クラスの部分を指す）
		Type* ptr_;

		RefCounter* ref_;

	};


	/// <summary>
	/// アロケータを指定してSharedPtrを作成する
	/// インスタンスとカウンタは1回でまとめて確保する。確保できなければ空のSharedPtrを返す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="...ArgTypes"></typeparam>
	/// <param name="alloc"></param>
	/// <param name="...args"></param>
	/// <returns></returns>
	template<typename Type, typename...ArgTypes>
	inline SharedPtr<Type> MakeSharedWithAlloc(MemoryAllocatorDelegate* alloc, ArgTypes&&...args)
	{
		MemoryAllocatorRef allocHolder(alloc);

		RefCounterToIns<Type>* holder = allocHolder.New<RefCounterToIns<Type>>(alloc, Forward<ArgTypes>(args)...);
		if (holder == nullptr)
		{
			return SharedPtr<Type>();
		}

		return SharedPtr<Type>(&holder->ins, &holder->typeCounter);
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
		return MakeSharedWithAlloc<Type>(nullptr, Forward<ArgTypes>(args)...);
	}
}
