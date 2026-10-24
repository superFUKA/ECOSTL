#pragma once
#include "Vector.h"

namespace utl
{
	/// <summary>
	/// メモリの再確保をチャンク単位で行うメモリが連続する可変長配列
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
		/// チャンクのサイズ、管理用のメモリアロケータ、要素用のメモリアロケータを指定して作成
		/// </summary>
		/// <param name="chunkSize"></param>
		/// <param name="alloc"></param>
		/// <param name="valAlloc"></param>
		ChunkVector(const uint32_t chunkSize, MemoryAllocatorAPI* alloc, MemoryAllocatorAPI* valAlloc)
			: chunks_(alloc)
			, valueMemAlloc_(valAlloc)
			, chunkSize_(chunkSize)
		{

		}
		/// <summary>
		/// チャンクのサイズと管理用、要素用の共用メモリアロケータを指定して作成
		/// </summary>
		/// <param name="chunkSize"></param>
		/// <param name="alloc"></param>
		ChunkVector(const uint32_t chunkSize = 64, MemoryAllocatorAPI* alloc = nullptr)
			: chunks_(alloc)
			, valueMemAlloc_(alloc)
			, chunkSize_(chunkSize)
		{

		}

		/// <summary>
		/// チャンクのサイズ、初期状態の容量、管理用のメモリアロケータ、要素用のメモリアロケータを指定して作成
		/// </summary>
		/// <param name="chunkSize"></param>
		/// <param name="capacity"></param>
		/// <param name="alloc"></param>
		/// <param name="valAlloc"></param>
		ChunkVector(const uint32_t chunkSize, const uint32_t capacity, MemoryAllocatorAPI* alloc, MemoryAllocatorAPI* valAlloc)
			: chunks_(GetChunkIndex(capacity) + 1, alloc)
			, valueMemAlloc_(valAlloc)
			, chunkSize_(chunkSize)
		{

		}
		/// <summary>
		/// チャンクのサイズと管理用、初期状態の容量、要素用の共用メモリアロケータを指定して作成
		/// </summary>
		/// <param name="chunkSize"></param>
		/// <param name="capacity"></param>
		/// <param name="alloc"></param>
		ChunkVector(const uint32_t chunkSize, const uint32_t capacity, MemoryAllocatorAPI* alloc = nullptr)
			: chunks_(GetChunkIndex(capacity) + 1, alloc)
			, valueMemAlloc_(alloc)
			, chunkSize_(chunkSize)
		{

		}

		//------------------------------------------
		// 代入
		//------------------------------------------

		/// <summary>
		/// コピーして作成するコンストラクタ
		/// </summary>
		/// <param name="src"></param>
		ChunkVector(const ChunkVector& src)
			:chunks_(src.chunks_)
			, valueMemAlloc_(src.valueMemAlloc_.Get())
			, chunkSize_(src.chunkSize_)
		{

		}
		/// <summary>
		/// すでに確保した要素を開放しコピーする
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		ChunkVector& operator=(const ChunkVector& src)
		{
			Clear();
			chunks_ = src.chunks_;
			chunkSize_ = src.chunkSize_;
			valueMemAlloc_ = src.valueMemAlloc_;
			return *this;
		}


		/// <summary>
		/// Srcを代入して作成する
		/// </summary>
		/// <param name="src"></param>
		ChunkVector(ChunkVector&& src)
			:chunks_(src.chunks_)
			, valueMemAlloc_(src.valueMemAlloc_.Get())
			, chunkSize_(src.chunkSize_)
		{

		}
		/// <summary>
		/// Srcを代入する
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		ChunkVector& operator=(ChunkVector&& src)
		{
			Clear();
			chunks_ = src.chunks_;
			chunkSize_ = src.chunkSize_;
			valueMemAlloc_ = src.valueMemAlloc_;
			return *this;
		}

		//------------------------------------------
		// 開放
		//------------------------------------------

		~ChunkVector()
		{
			Clear();
		}

		/// <summary>
		/// 確保した要素を全てを開放する
		/// </summary>
		void Clear()
		{
			chunks_.Clear();
		}

		/// <summary>
		/// 末尾の要素を削除する
		/// 末尾のチャンクが空になったらチャンクごとポップする
		/// </summary>
		void PopBack()
		{
			if (chunks_.Empty())
			{
				return;
			}
			// 末尾のチャンクの末尾の要素をポップ
			chunks_.Back()->PopBack();
			// 末尾のチャンクが空になったら末尾のチャンクをポップ
			if (chunks_.Back()->Empty())
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
		/// <returns></returns>
		Type* PushBack(const Type& src)
		{
			// チャンクが無ければ作成
			if (chunks_.Empty())
			{
				chunks_.EmplaceBack(chunkSize_, valueMemAlloc_.Get());
			}
			// 末尾のチャンクに空きがなければ作成
			if (chunks_.Back()->Size() >= chunkSize_)
			{
				chunks_.EmplaceBack(chunkSize_, valueMemAlloc_.Get());
			}
			return chunks_.Back()->PushBack(src);
		}
		/// <summary>
		/// 末尾に空の要素を追加
		/// </summary>
		/// <returns></returns>
		Type* PushBack()
		{
			// チャンクが無ければ作成
			if (chunks_.Empty())
			{
				chunks_.EmplaceBack(chunkSize_, valueMemAlloc_.Get());
			}
			// 末尾のチャンクに空きがなければ作成
			if (chunks_.Back()->Size() >= chunkSize_)
			{
				chunks_.EmplaceBack(chunkSize_, valueMemAlloc_.Get());
			}
			return chunks_.Back()->PushBack();
		}

		/// <summary>
		/// コンストラクタ引数から末尾に要素を追加する
		/// </summary>
		/// <typeparam name="...ArgTypes"></typeparam>
		/// <param name="...args"></param>
		/// <returns></returns>
		template<typename...ArgTypes>
		Type* EmplaceBack(ArgTypes&&...args)
		{
			// チャンクが無ければ作成
			if (chunks_.Empty())
			{
				chunks_.EmplaceBack(chunkSize_, valueMemAlloc_.Get());
			}
			// 末尾のチャンクに空きがなければ作成
			if (chunks_.Back()->Size() >= chunkSize_)
			{
				chunks_.EmplaceBack(chunkSize_, valueMemAlloc_.Get());
			}
			return chunks_.Back()->EmplaceBack(args...);
		}

		//------------------------------------------
		// 取得
		//------------------------------------------


		/// <summary>
		/// 引数の添え字の要素が入るチャンクの添え字を求める
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline constexpr uint32_t GetChunkIndex(const uint32_t index)const
		{
			return index / chunkSize_;
		}

		/// <summary>
		/// 引数の要素がチャンク内で格納される添え字を求める
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline constexpr uint32_t GetValueIndex(const uint32_t index)const
		{
			return index % chunkSize_;
		}

		/// <summary>
		/// チャンク一つの大きさを求める
		/// </summary>
		/// <returns></returns>
		inline constexpr uint32_t GetChunkSize()const
		{
			return chunkSize_;
		}

		/// <summary>
		/// チャンクの数を求める
		/// </summary>
		/// <returns></returns>
		inline constexpr uint32_t GetChunkNum()const
		{
			return chunks_.Size();
		}

		/// <summary>
		/// チャンクを添え字を使い取得
		/// </summary>
		/// <param name="chunkIndex"></param>
		/// <returns></returns>
		const Vector<Type>* GetChunk(const uint32_t chunkIndex)const
		{
			return chunks_.Get(chunkIndex);
		}

		/// <summary>
		/// 添え字から要素のポインタを取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		Type* Get(const uint32_t index)const
		{
			const uint32_t chunkIndex = GetChunkIndex(index);
			const uint32_t valueIndex = GetValueIndex(index);

			const Vector<Type>* backChunk = GetChunk(chunkIndex);
			assert(backChunk != nullptr && "範囲外アクセスです");

			Type* ptr = backChunk->Get(valueIndex);
			assert(ptr != nullptr && "範囲外アクセスです");

			return ptr;
		}

		/// <summary>
		/// 添え字から要素の参照を取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		Type& operator[](const uint32_t index)const
		{
			return chunks_[GetChunkIndex(index)][GetValueIndex(index)];
		}
		/// <summary>
		/// 添え字から要素の参照を取得
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		Type& operator[](const uint32_t index)
		{
			return chunks_[GetChunkIndex(index)][GetValueIndex(index)];
		}

		/// <summary>
		/// 引数のブロックの先頭アドレスを取得する
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline constexpr Type* BeginFromChunk(const uint32_t index = 0)const
		{
			if (!chunks_.IsContain(index))
			{
				return nullptr;
			}
			return chunks_[index].Begin();
		}
		/// <summary>
		/// 先頭の要素のポインタを取得
		/// 先頭のチャンクの先頭の要素のポインタを取得
		/// チャンクを超えたら連続は途切れるため注意
		/// </summary>
		/// <returns></returns>
		Type* Begin()const
		{
			return BeginFromChunk(0);
		}

		inline constexpr Type* BackFromChunk(const uint32_t index = 0)const
		{
			if (!chunks_.IsContain(index))
			{
				return nullptr;
			}
			return chunks_[index].Back();
		}
		/// <summary>
		/// 末尾の要素を取得
		/// 末尾のチャンクの末尾の要素を取得
		/// チャンクを超えたら連続は途切れるため注意
		/// </summary>
		/// <returns></returns>
		Type* Back()const
		{
			if (chunks_.Empty())
			{
				return nullptr;
			}
			return BackFromChunk(chunks_.Size() - 1);
		}

		/// <summary>
		/// 要素の総数を取得
		/// </summary>
		/// <returns></returns>
		uint32_t Size()const
		{
			uint32_t result = 0;
			if (chunks_.Empty())
			{
				return result;
			}
			if (chunks_.Size() >= 2)
			{
				result += (chunks_.Size() - 1) * chunkSize_;
			}
			result += chunks_.Back()->Size();

			return result;
		}

		/// <summary>
		/// 格納可能な容量の総数を取得
		/// </summary>
		/// <returns></returns>
		uint32_t Capacity()const
		{
			return chunks_.Capacity() * chunkSize_;
		}

		/// <summary>
		/// 要素が入って無ければTrue
		/// </summary>
		/// <returns></returns>
		bool Empty()const
		{
			if (chunks_.Empty())
			{
				return true;
			}
			return chunks_.Back()->Empty();
		}

		/// <summary>
		/// 引数の添え字が範囲内か返す関数
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		bool IsContain(const uint32_t index)const
		{
			const uint32_t chunkIndex = GetChunkIndex(index);
			const uint32_t valueIndex = GetValueIndex(index);

			if (!chunks_.IsContain(chunkIndex))
			{
				return false;
			}

			return chunks_[chunkIndex].IsContain(valueIndex);
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		/// <summary>
		/// 要素の個数を変更
		/// 
		/// </summary>
		/// <typeparam name="...ArgTypes"></typeparam>
		/// <param name="size"></param>
		/// <param name="...args"></param>
		template<typename...ArgTypes>
		void Resize(const uint32_t size, ArgTypes&&...args)
		{
			if (size == 0)
			{
				Clear();
				return;
			}

			uint32_t beforeChunkNum = chunks_.Size();
			uint32_t afterChunkNum = GetChunkIndex(size - 1) + 1;
			// チャンクの数を変更
			chunks_.Resize(afterChunkNum, chunkSize_, valueMemAlloc_.Get());
			// チャンクが増えたなら初期化する
			for (uint32_t i = beforeChunkNum; i < afterChunkNum; i++)
			{
				chunks_[i].Resize(chunkSize_, args...);
			}

			// 末尾のチャンクの整合性を取る
			chunks_.Back()->Resize(GetValueIndex(size - 1) + 1, args...);
		}

		void Resize(const uint32_t size, const Type& src)
		{
			if (size == 0)
			{
				Clear();
				return;
			}

			uint32_t beforeChunkNum = chunks_.Size();
			uint32_t afterChunkNum = GetChunkIndex(size - 1) + 1;
			// チャンクの数を変更
			chunks_.Resize(afterChunkNum, chunkSize_, valueMemAlloc_.Get());
			// チャンクが増えたなら初期化する
			for (uint32_t i = beforeChunkNum; i < afterChunkNum; i++)
			{
				chunks_[i].Resize(chunkSize_, src);
			}

			// 末尾のチャンクの整合性を取る
			chunks_.Back()->Resize(GetValueIndex(size - 1) + 1, src);
		}



	private:

		Vector<Vector<Type>> chunks_;
		MemoryAllocatorAPIHolder valueMemAlloc_;
		uint32_t chunkSize_;

	};
}