#pragma once
// ECOSTL dev の ChunkVector を土台にした（Chunk を Chunk と呼び替えた）。testGame 版の ChunkVector は、容量を指定するコンストラクタで未初期化のメンバーを使う・範囲外の Get が nullptr を返さない等の不具合があった。
#include <cassert>
#include <cstdint>
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Core/Memory/MemCopy.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include "ECOSTL/Container/Vector/Vector.h"

namespace utl
{
	/// <summary>
	/// メモリの再確保をチャンク単位で行う可変長配列
	/// チャンク内のメモリは連続し、チャンクをまたぐと連続しない
	/// 要素を追加してもチャンクの再確保は起きないため、追加済みの要素のアドレスは変わらない
	/// 末尾以外のチャンクは常に満杯で、末尾のチャンクは空でない
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class ChunkVector
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		/// <summary>
		/// チャンクのサイズと、管理用と要素用に共用するメモリアロケータを指定して作成
		/// </summary>
		/// <param name="chunkSize">1以上</param>
		/// <param name="alloc"></param>
		explicit ChunkVector(const uint32_t chunkSize = 64, const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: chunks_(alloc)
			, valueMemAlloc_(alloc)
			, chunkSize_(chunkSize)
		{
			assert(chunkSize_ > 0 && "チャンクのサイズは1以上にしてください");
		}

		/// <summary>
		/// チャンクのサイズ、管理用のメモリアロケータ、要素用のメモリアロケータを指定して作成
		/// </summary>
		/// <param name="chunkSize">1以上</param>
		/// <param name="alloc">チャンクの管理用</param>
		/// <param name="valueAlloc">要素用</param>
		ChunkVector(const uint32_t chunkSize, const MemoryAllocatorRef& alloc, const MemoryAllocatorRef& valueAlloc)
			: chunks_(alloc)
			, valueMemAlloc_(valueAlloc)
			, chunkSize_(chunkSize)
		{
			assert(chunkSize_ > 0 && "チャンクのサイズは1以上にしてください");
		}

		/// <summary>
		/// チャンクのサイズ、初期状態の容量、管理用と要素用に共用するメモリアロケータを指定して作成
		/// 容量の分のチャンクの管理領域を確保する
		/// </summary>
		/// <param name="chunkSize">1以上</param>
		/// <param name="capacity"></param>
		/// <param name="alloc"></param>
		ChunkVector(const uint32_t chunkSize, const uint32_t capacity, const MemoryAllocatorRef& alloc)
			: ChunkVector(chunkSize, capacity, alloc, alloc)
		{
		}

		/// <summary>
		/// チャンクのサイズ、初期状態の容量、管理用のメモリアロケータ、要素用のメモリアロケータを指定して作成
		/// 容量の分のチャンクの管理領域を確保する
		/// </summary>
		/// <param name="chunkSize">1以上</param>
		/// <param name="capacity"></param>
		/// <param name="alloc">チャンクの管理用</param>
		/// <param name="valueAlloc">要素用</param>
		ChunkVector(const uint32_t chunkSize, const uint32_t capacity, const MemoryAllocatorRef& alloc, const MemoryAllocatorRef& valueAlloc)
			: chunks_(alloc)
			, valueMemAlloc_(valueAlloc)
			, chunkSize_(chunkSize)
		{
			assert(chunkSize_ > 0 && "チャンクのサイズは1以上にしてください");
			chunks_.Reserve(capacity == 0 ? 0 : GetChunkIndex(capacity - 1) + 1);
		}

		//------------------------------------------
		// 代入
		//------------------------------------------

		/// <summary>
		/// コピーして作成する
		/// メモリアロケータもコピー元と同じものを使う
		/// </summary>
		/// <param name="src"></param>
		ChunkVector(const ChunkVector& src) = default;

		/// <summary>
		/// コピーして代入する
		/// すでに確保した要素は開放する
		/// メモリアロケータもコピー元のものを引き継ぐ
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		ChunkVector& operator=(const ChunkVector& src)
		{
			if (this == &src)
			{
				return *this;
			}
			Reset();
			chunks_ = src.chunks_;
			chunkSize_ = src.chunkSize_;
			valueMemAlloc_ = src.valueMemAlloc_;
			return *this;
		}

		/// <summary>
		/// 移動して作成する
		/// </summary>
		/// <param name="src"></param>
		ChunkVector(ChunkVector&& src)noexcept
			: chunks_(Move(src.chunks_))
			, valueMemAlloc_(src.valueMemAlloc_)
			, chunkSize_(src.chunkSize_)
		{
		}

		/// <summary>
		/// 移動して代入する
		/// すでに確保した要素は開放する
		/// メモリアロケータも移動元のものを引き継ぐ
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		ChunkVector& operator=(ChunkVector&& src)noexcept
		{
			if (this == &src)
			{
				return *this;
			}
			Reset();
			chunks_ = Move(src.chunks_);
			chunkSize_ = src.chunkSize_;
			valueMemAlloc_ = src.valueMemAlloc_;
			return *this;
		}

		//------------------------------------------
		// 開放
		//------------------------------------------

		~ChunkVector()
		{
			Reset();
		}

		/// <summary>
		/// 要素と確保したメモリを全て開放する
		/// </summary>
		void Clear()
		{
			chunks_.Clear();
		}

		/// <summary>
		/// 要素と確保したメモリを全て開放する
		/// </summary>
		void Reset()
		{
			chunks_.Reset();
		}

		/// <summary>
		/// 末尾の要素を削除する
		/// 末尾のチャンクが空になったらチャンクごと開放する
		/// </summary>
		void PopBack()
		{
			if (chunks_.IsEmpty())
			{
				return;
			}
			chunks_.Back()->PopBack();
			if (chunks_.Back()->IsEmpty())
			{
				chunks_.PopBack();
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
			// チャンクが無い、または末尾のチャンクに空きがなければ作成
			if (chunks_.IsEmpty() || chunks_.Back()->Size() >= chunkSize_)
			{
				AddChunk();
			}
			return chunks_.Back()->EmplaceBack(Forward<ArgTypes>(args)...);
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		/// <summary>
		/// 引数の添え字の要素が入るチャンクの添え字を求める
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline uint32_t GetChunkIndex(const uint32_t index)const
		{
			return index / chunkSize_;
		}

		/// <summary>
		/// 引数の要素がチャンク内で格納される添え字を求める
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline uint32_t GetValueIndex(const uint32_t index)const
		{
			return index % chunkSize_;
		}

		/// <summary>
		/// チャンク一つの大きさを求める
		/// </summary>
		/// <returns></returns>
		inline uint32_t GetChunkSize()const
		{
			return chunkSize_;
		}

		/// <summary>
		/// チャンクの数を求める
		/// </summary>
		/// <returns></returns>
		inline uint32_t GetChunkNum()const
		{
			return chunks_.Size();
		}

		/// <summary>
		/// チャンクを添え字を使い取得
		/// </summary>
		/// <param name="chunkIndex"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline const Vector<Type>* GetChunk(const uint32_t chunkIndex)const
		{
			return chunks_.Get(chunkIndex);
		}

		/// <summary>
		/// 添え字から要素のポインタを取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* Get(const uint32_t index)
		{
			Vector<Type>* chunk = chunks_.Get(GetChunkIndex(index));
			if (chunk == nullptr)
			{
				return nullptr;
			}
			return chunk->Get(GetValueIndex(index));
		}
		inline const Type* Get(const uint32_t index)const
		{
			const Vector<Type>* chunk = chunks_.Get(GetChunkIndex(index));
			if (chunk == nullptr)
			{
				return nullptr;
			}
			return chunk->Get(GetValueIndex(index));
		}

		/// <summary>
		/// 添え字から要素の参照を取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline Type& operator[](const uint32_t index)
		{
			assert(IsValidIndex(index) && "範囲外アクセスです");
			return chunks_[GetChunkIndex(index)][GetValueIndex(index)];
		}
		inline const Type& operator[](const uint32_t index)const
		{
			assert(IsValidIndex(index) && "範囲外アクセスです");
			return chunks_[GetChunkIndex(index)][GetValueIndex(index)];
		}

		/// <summary>
		/// 引数のチャンクの先頭アドレスを取得する
		/// </summary>
		/// <param name="chunkIndex"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* BeginFromChunk(const uint32_t chunkIndex = 0)
		{
			Vector<Type>* chunk = chunks_.Get(chunkIndex);
			return chunk == nullptr ? nullptr : chunk->Begin();
		}
		inline const Type* BeginFromChunk(const uint32_t chunkIndex = 0)const
		{
			const Vector<Type>* chunk = chunks_.Get(chunkIndex);
			return chunk == nullptr ? nullptr : chunk->Begin();
		}

		/// <summary>
		/// 先頭の要素のポインタを取得
		/// 先頭のチャンクの先頭の要素のポインタを取得
		/// チャンクを超えたら連続は途切れるため注意
		/// </summary>
		/// <returns>要素が無ければnullptr</returns>
		inline Type* Begin()
		{
			return BeginFromChunk(0);
		}
		inline const Type* Begin()const
		{
			return BeginFromChunk(0);
		}

		/// <summary>
		/// 引数のチャンクの末尾の要素のポインタを取得する
		/// </summary>
		/// <param name="chunkIndex"></param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* BackFromChunk(const uint32_t chunkIndex)
		{
			Vector<Type>* chunk = chunks_.Get(chunkIndex);
			return chunk == nullptr ? nullptr : chunk->Back();
		}
		inline const Type* BackFromChunk(const uint32_t chunkIndex)const
		{
			const Vector<Type>* chunk = chunks_.Get(chunkIndex);
			return chunk == nullptr ? nullptr : chunk->Back();
		}

		/// <summary>
		/// 末尾の要素を取得
		/// 末尾のチャンクの末尾の要素を取得
		/// </summary>
		/// <returns>要素が無ければnullptr</returns>
		inline Type* Back()
		{
			if (chunks_.IsEmpty())
			{
				return nullptr;
			}
			return chunks_.Back()->Back();
		}
		inline const Type* Back()const
		{
			if (chunks_.IsEmpty())
			{
				return nullptr;
			}
			return chunks_.Back()->Back();
		}

		/// <summary>
		/// 要素の総数を取得
		/// </summary>
		/// <returns></returns>
		inline uint32_t Size()const
		{
			if (chunks_.IsEmpty())
			{
				return 0;
			}
			return (chunks_.Size() - 1) * chunkSize_ + chunks_.Back()->Size();
		}

		/// <summary>
		/// 確保済みのチャンクに格納可能な要素の総数を取得
		/// </summary>
		/// <returns></returns>
		inline uint32_t Capacity()const
		{
			return chunks_.Size() * chunkSize_;
		}

		/// <summary>
		/// 要素が入って無ければTrue
		/// </summary>
		/// <returns></returns>
		inline bool IsEmpty()const
		{
			return chunks_.IsEmpty();
		}

		/// <summary>
		/// 引数の添え字が範囲内か返す関数
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline bool IsValidIndex(const uint32_t index)const
		{
			const uint32_t chunkIndex = GetChunkIndex(index);
			if (!chunks_.IsValidIndex(chunkIndex))
			{
				return false;
			}
			return chunks_[chunkIndex].IsValidIndex(GetValueIndex(index));
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
			const uint32_t chunkNum = size == 0 ? 0 : GetChunkIndex(size - 1) + 1;

			// 多すぎるチャンクを開放
			while (chunks_.Size() > chunkNum)
			{
				chunks_.PopBack();
			}
			// 足りないチャンクを作成
			while (chunks_.Size() < chunkNum)
			{
				AddChunk();
			}
			// 末尾以外は満杯、末尾は残りの個数にそろえる
			for (uint32_t i = 0; i < chunkNum; i++)
			{
				const uint32_t chunkElemNum = (i + 1 < chunkNum) ? chunkSize_ : GetValueIndex(size - 1) + 1;
				chunks_[i].Resize(chunkElemNum, args...);
			}
		}

	private:

		/// <summary>
		/// 末尾に空のチャンクを追加する
		/// </summary>
		void AddChunk()
		{
			Vector<Type>* chunk = chunks_.EmplaceBack(valueMemAlloc_);
			assert(chunk != nullptr && "チャンクを追加できませんでした");
			chunk->Reserve(chunkSize_);
		}


	public:
		//------------------------------------------
		// testGame 版から引き継いだ機能
		//------------------------------------------

		// 使うアロケータを指定して作る（所有しない。nullptr なら既定のアロケータ）
		ChunkVector(const uint32_t chunkSize, MemoryAllocatorDelegate* alloc)
			: ChunkVector(chunkSize, MemoryAllocatorRef(alloc))
		{
		}
		ChunkVector(const uint32_t chunkSize, const uint32_t capacity, MemoryAllocatorDelegate* alloc)
			: ChunkVector(chunkSize, capacity, MemoryAllocatorRef(alloc))
		{
		}

		/// <summary>
		/// 要素の入っていない後ろのチャンクを解放し、チャンクの一覧の容量も詰める（要素のアドレスは変わらない）
		/// </summary>
		void Fit()
		{
			ReleaseUnusedChunks();
			chunks_.Fit();
		}

		/// <summary>
		/// 要素の入っていない後ろのチャンクを解放し、チャンクの一覧の容量は少し余裕を残して詰める
		/// </summary>
		void FitSlack()
		{
			ReleaseUnusedChunks();
			chunks_.FitSlack();
		}

		// 範囲 for 用の、通し添え字で進むイテレータ。要素のアドレスが変わらないので、増減で無効にならない
		template<typename Owner, typename Elem>
		class IteratorT
		{
		public:
			IteratorT(Owner* owner, const uint32_t index) : owner_(owner), index_(index) {}
			Elem& operator*() const { return (*owner_)[index_]; }
			Elem* operator->() const { return &(*owner_)[index_]; }
			IteratorT& operator++() { ++index_; return *this; }
			bool operator==(const IteratorT& o) const { return index_ == o.index_; }
			bool operator!=(const IteratorT& o) const { return index_ != o.index_; }
		private:
			Owner* owner_;  // 走査対象のコンテナ（所有しない）
			uint32_t index_;  // 先頭からの通し添え字
		};

		// 範囲 for 文用（言語が要求する名前）
		IteratorT<ChunkVector, Type> begin() { return { this, 0 }; }
		IteratorT<ChunkVector, Type> end() { return { this, Size() }; }
		IteratorT<const ChunkVector, const Type> begin() const { return { this, 0 }; }
		IteratorT<const ChunkVector, const Type> end() const { return { this, Size() }; }

	private:
		// 要素の入っていない後ろのチャンクを解放する
		void ReleaseUnusedChunks()
		{
			const uint32_t used = Size() == 0 ? 0 : GetChunkIndex(Size() - 1) + 1;
			while (chunks_.Size() > used)
			{
				chunks_.PopBack();
			}
		}

	private:

		Vector<Vector<Type>> chunks_;
		MemoryAllocatorRef valueMemAlloc_;
		uint32_t chunkSize_;

	};
}
