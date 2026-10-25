#pragma once
#include <cassert>
#include <cstdint>
#include "../../Core/Utility.h"
#include "../../Memory/Allocate/MemoryAllocatorAPI.h"
#include "Vector.h"

namespace utl
{
	/// <summary>
	/// メモリの再確保をブロック単位で行う可変長配列
	/// ブロック内のメモリは連続し、ブロックをまたぐと連続しない
	/// 要素を追加してもブロックの再確保は起きないため、追加済みの要素のアドレスは変わらない
	/// 末尾以外のブロックは常に満杯で、末尾のブロックは空でない
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class BlockVector
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		/// <summary>
		/// ブロックのサイズと、管理用と要素用に共用するメモリアロケータを指定して作成
		/// </summary>
		/// <param name="blockSize">1以上</param>
		/// <param name="alloc"></param>
		explicit BlockVector(const uint32_t blockSize = 64, const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: blocks_(alloc)
			, valueMemAlloc_(alloc)
			, blockSize_(blockSize)
		{
			assert(blockSize_ > 0 && "ブロックのサイズは1以上にしてください");
		}

		/// <summary>
		/// ブロックのサイズ、管理用のメモリアロケータ、要素用のメモリアロケータを指定して作成
		/// </summary>
		/// <param name="blockSize">1以上</param>
		/// <param name="alloc">ブロックの管理用</param>
		/// <param name="valueAlloc">要素用</param>
		BlockVector(const uint32_t blockSize, const MemoryAllocatorRef& alloc, const MemoryAllocatorRef& valueAlloc)
			: blocks_(alloc)
			, valueMemAlloc_(valueAlloc)
			, blockSize_(blockSize)
		{
			assert(blockSize_ > 0 && "ブロックのサイズは1以上にしてください");
		}

		/// <summary>
		/// ブロックのサイズ、初期状態の容量、管理用と要素用に共用するメモリアロケータを指定して作成
		/// 容量の分のブロックの管理領域を確保する
		/// </summary>
		/// <param name="blockSize">1以上</param>
		/// <param name="capacity"></param>
		/// <param name="alloc"></param>
		BlockVector(const uint32_t blockSize, const uint32_t capacity, const MemoryAllocatorRef& alloc)
			: BlockVector(blockSize, capacity, alloc, alloc)
		{
		}

		/// <summary>
		/// ブロックのサイズ、初期状態の容量、管理用のメモリアロケータ、要素用のメモリアロケータを指定して作成
		/// 容量の分のブロックの管理領域を確保する
		/// </summary>
		/// <param name="blockSize">1以上</param>
		/// <param name="capacity"></param>
		/// <param name="alloc">ブロックの管理用</param>
		/// <param name="valueAlloc">要素用</param>
		BlockVector(const uint32_t blockSize, const uint32_t capacity, const MemoryAllocatorRef& alloc, const MemoryAllocatorRef& valueAlloc)
			: blocks_(alloc)
			, valueMemAlloc_(valueAlloc)
			, blockSize_(blockSize)
		{
			assert(blockSize_ > 0 && "ブロックのサイズは1以上にしてください");
			blocks_.Reserve(capacity == 0 ? 0 : GetBlockIndex(capacity - 1) + 1);
		}

		//------------------------------------------
		// 代入
		//------------------------------------------

		/// <summary>
		/// コピーして作成する
		/// メモリアロケータもコピー元と同じものを使う
		/// </summary>
		/// <param name="src"></param>
		BlockVector(const BlockVector& src) = default;

		/// <summary>
		/// コピーして代入する
		/// すでに確保した要素は開放する
		/// メモリアロケータもコピー元のものを引き継ぐ
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		BlockVector& operator=(const BlockVector& src)
		{
			if (this == &src)
			{
				return *this;
			}
			Reset();
			blocks_ = src.blocks_;
			blockSize_ = src.blockSize_;
			valueMemAlloc_ = src.valueMemAlloc_;
			return *this;
		}

		/// <summary>
		/// 移動して作成する
		/// </summary>
		/// <param name="src"></param>
		BlockVector(BlockVector&& src)noexcept
			: blocks_(Move(src.blocks_))
			, valueMemAlloc_(src.valueMemAlloc_)
			, blockSize_(src.blockSize_)
		{
		}

		/// <summary>
		/// 移動して代入する
		/// すでに確保した要素は開放する
		/// メモリアロケータも移動元のものを引き継ぐ
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		BlockVector& operator=(BlockVector&& src)noexcept
		{
			if (this == &src)
			{
				return *this;
			}
			Reset();
			blocks_ = Move(src.blocks_);
			blockSize_ = src.blockSize_;
			valueMemAlloc_ = src.valueMemAlloc_;
			return *this;
		}

		//------------------------------------------
		// 開放
		//------------------------------------------

		~BlockVector()
		{
			Reset();
		}

		/// <summary>
		/// 要素と確保したメモリを全て開放する
		/// </summary>
		void Clear()
		{
			blocks_.Clear();
		}

		/// <summary>
		/// 要素と確保したメモリを全て開放する
		/// </summary>
		void Reset()
		{
			blocks_.Reset();
		}

		/// <summary>
		/// 末尾の要素を削除する
		/// 末尾のブロックが空になったらブロックごと開放する
		/// </summary>
		void PopBack()
		{
			if (blocks_.IsEmpty())
			{
				return;
			}
			blocks_.Back()->PopBack();
			if (blocks_.Back()->IsEmpty())
			{
				blocks_.PopBack();
			}
		}

		//------------------------------------------
		// 追加
		//------------------------------------------

		/// <summary>
		/// 参照から末尾に要素を追加する
		/// </summary>
		/// <param name="src"></param>
		/// <returns>追加した要素のポインタ</returns>
		Type* PushBack(const Type& src)
		{
			return EmplaceBack(src);
		}
		/// <summary>
		/// 移動して末尾に要素を追加する
		/// </summary>
		/// <param name="src"></param>
		/// <returns>追加した要素のポインタ</returns>
		Type* PushBack(Type&& src)
		{
			return EmplaceBack(Move(src));
		}
		/// <summary>
		/// 末尾に空の要素を追加
		/// </summary>
		/// <returns>追加した要素のポインタ</returns>
		Type* PushBack()
		{
			return EmplaceBack();
		}

		/// <summary>
		/// コンストラクタ引数から末尾に要素を追加する
		/// </summary>
		/// <typeparam name="...ArgTypes"></typeparam>
		/// <param name="...args"></param>
		/// <returns>追加した要素のポインタ</returns>
		template<typename...ArgTypes>
		Type* EmplaceBack(ArgTypes&&...args)
		{
			// ブロックが無い、または末尾のブロックに空きがなければ作成
			if (blocks_.IsEmpty() || blocks_.Back()->Size() >= blockSize_)
			{
				AddBlock();
			}
			return blocks_.Back()->EmplaceBack(Forward<ArgTypes>(args)...);
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		/// <summary>
		/// 引数の添え字の要素が入るブロックの添え字を求める
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline uint32_t GetBlockIndex(const uint32_t index)const
		{
			return index / blockSize_;
		}

		/// <summary>
		/// 引数の要素がブロック内で格納される添え字を求める
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline uint32_t GetValueIndex(const uint32_t index)const
		{
			return index % blockSize_;
		}

		/// <summary>
		/// ブロック一つの大きさを求める
		/// </summary>
		/// <returns></returns>
		inline uint32_t GetBlockSize()const
		{
			return blockSize_;
		}

		/// <summary>
		/// ブロックの数を求める
		/// </summary>
		/// <returns></returns>
		inline uint32_t GetBlockNum()const
		{
			return blocks_.Size();
		}

		/// <summary>
		/// ブロックを添え字を使い取得
		/// </summary>
		/// <param name="blockIndex"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline const Vector<Type>* GetBlock(const uint32_t blockIndex)const
		{
			return blocks_.Get(blockIndex);
		}

		/// <summary>
		/// 添え字から要素のポインタを取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* Get(const uint32_t index)
		{
			Vector<Type>* block = blocks_.Get(GetBlockIndex(index));
			if (block == nullptr)
			{
				return nullptr;
			}
			return block->Get(GetValueIndex(index));
		}
		inline const Type* Get(const uint32_t index)const
		{
			const Vector<Type>* block = blocks_.Get(GetBlockIndex(index));
			if (block == nullptr)
			{
				return nullptr;
			}
			return block->Get(GetValueIndex(index));
		}

		/// <summary>
		/// 添え字から要素の参照を取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline Type& operator[](const uint32_t index)
		{
			assert(IsValidIndex(index) && "範囲外アクセスです");
			return blocks_[GetBlockIndex(index)][GetValueIndex(index)];
		}
		inline const Type& operator[](const uint32_t index)const
		{
			assert(IsValidIndex(index) && "範囲外アクセスです");
			return blocks_[GetBlockIndex(index)][GetValueIndex(index)];
		}

		/// <summary>
		/// 引数のブロックの先頭アドレスを取得する
		/// </summary>
		/// <param name="blockIndex"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* BeginFromBlock(const uint32_t blockIndex = 0)
		{
			Vector<Type>* block = blocks_.Get(blockIndex);
			return block == nullptr ? nullptr : block->Begin();
		}
		inline const Type* BeginFromBlock(const uint32_t blockIndex = 0)const
		{
			const Vector<Type>* block = blocks_.Get(blockIndex);
			return block == nullptr ? nullptr : block->Begin();
		}

		/// <summary>
		/// 先頭の要素のポインタを取得
		/// 先頭のブロックの先頭の要素のポインタを取得
		/// ブロックを超えたら連続は途切れるため注意
		/// </summary>
		/// <returns>要素が無ければnullptr</returns>
		inline Type* Begin()
		{
			return BeginFromBlock(0);
		}
		inline const Type* Begin()const
		{
			return BeginFromBlock(0);
		}

		/// <summary>
		/// 引数のブロックの末尾の要素のポインタを取得する
		/// </summary>
		/// <param name="blockIndex"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* BackFromBlock(const uint32_t blockIndex)
		{
			Vector<Type>* block = blocks_.Get(blockIndex);
			return block == nullptr ? nullptr : block->Back();
		}
		inline const Type* BackFromBlock(const uint32_t blockIndex)const
		{
			const Vector<Type>* block = blocks_.Get(blockIndex);
			return block == nullptr ? nullptr : block->Back();
		}

		/// <summary>
		/// 末尾の要素を取得
		/// 末尾のブロックの末尾の要素を取得
		/// </summary>
		/// <returns>要素が無ければnullptr</returns>
		inline Type* Back()
		{
			if (blocks_.IsEmpty())
			{
				return nullptr;
			}
			return blocks_.Back()->Back();
		}
		inline const Type* Back()const
		{
			if (blocks_.IsEmpty())
			{
				return nullptr;
			}
			return blocks_.Back()->Back();
		}

		/// <summary>
		/// 要素の総数を取得
		/// </summary>
		/// <returns></returns>
		inline uint32_t Size()const
		{
			if (blocks_.IsEmpty())
			{
				return 0;
			}
			return (blocks_.Size() - 1) * blockSize_ + blocks_.Back()->Size();
		}

		/// <summary>
		/// 確保済みのブロックに格納可能な要素の総数を取得
		/// </summary>
		/// <returns></returns>
		inline uint32_t Capacity()const
		{
			return blocks_.Size() * blockSize_;
		}

		/// <summary>
		/// 要素が入って無ければTrue
		/// </summary>
		/// <returns></returns>
		inline bool IsEmpty()const
		{
			return blocks_.IsEmpty();
		}

		/// <summary>
		/// 引数の添え字が範囲内か返す関数
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline bool IsValidIndex(const uint32_t index)const
		{
			const uint32_t blockIndex = GetBlockIndex(index);
			if (!blocks_.IsValidIndex(blockIndex))
			{
				return false;
			}
			return blocks_[blockIndex].IsValidIndex(GetValueIndex(index));
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		/// <summary>
		/// 要素の個数を変更
		/// 増える要素は引数から作成する
		/// </summary>
		/// <typeparam name="...ArgTypes"></typeparam>
		/// <param name="size"></param>
		/// <param name="...args">増える要素のコンストラクタ引数</param>
		template<typename...ArgTypes>
		void Resize(const uint32_t size, const ArgTypes&...args)
		{
			const uint32_t blockNum = size == 0 ? 0 : GetBlockIndex(size - 1) + 1;

			// 多すぎるブロックを開放
			while (blocks_.Size() > blockNum)
			{
				blocks_.PopBack();
			}
			// 足りないブロックを作成
			while (blocks_.Size() < blockNum)
			{
				AddBlock();
			}
			// 末尾以外は満杯、末尾は残りの個数にそろえる
			for (uint32_t i = 0; i < blockNum; i++)
			{
				const uint32_t blockElemNum = (i + 1 < blockNum) ? blockSize_ : GetValueIndex(size - 1) + 1;
				blocks_[i].Resize(blockElemNum, args...);
			}
		}

	private:

		/// <summary>
		/// 末尾に空のブロックを追加する
		/// </summary>
		void AddBlock()
		{
			Vector<Type>* block = blocks_.EmplaceBack(valueMemAlloc_);
			assert(block != nullptr && "ブロックを追加できませんでした");
			block->Reserve(blockSize_);
		}

	private:

		Vector<Vector<Type>> blocks_;
		MemoryAllocatorRef valueMemAlloc_;
		uint32_t blockSize_;

	};
}
