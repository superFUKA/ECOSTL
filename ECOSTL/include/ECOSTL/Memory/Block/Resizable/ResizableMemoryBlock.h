#pragma once
// 実行時にサイズを変えられる生メモリブロック。スレッドセーフではない。
#include "../../Allocator/MemoryAllocator.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// サイズの変更が可能なメモリブロック
	/// </summary>
	class ResizableMemoryBlock
	{
	public:

		//-----------------------------------------
		// 作成
		//-----------------------------------------

		/// <summary>
		/// 空のメモリブロックを作成する
		/// </summary>
		/// <param name="alloc"></param>
		ResizableMemoryBlock(MemoryAllocatorDelegate* alloc = nullptr);

		/// <summary>
		/// メモリサイズを指定しながらメモリブロックを作成する
		/// </summary>
		/// <param name="size"></param>
		/// <param name="alloc"></param>
		ResizableMemoryBlock(const uint32_t size, MemoryAllocatorDelegate* alloc = nullptr);

		//-----------------------------------------
		// 代入
		//-----------------------------------------

		/// <summary>
		/// 引数の管理するメモリのコピーをこのブロックに作成する
		/// </summary>
		/// <param name="src"></param>
		ResizableMemoryBlock(const ResizableMemoryBlock& src);

		/// <summary>
		/// 引数の管理するメモリのコピーをこのブロックに作成する
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		ResizableMemoryBlock& operator=(const ResizableMemoryBlock& src);

		//-----------------------------------------
		// 開放
		//-----------------------------------------

		/// <summary>
		/// 確保してるメモリを開放する
		/// </summary>
		void Reset();

		~ResizableMemoryBlock();

		//-----------------------------------------
		// 操作
		//-----------------------------------------

		/// <summary>
		/// 使用するメモリアロケータを変更する
		/// </summary>
		/// <param name="alloc"></param>
		void ChangeMemoryAllocator(MemoryAllocatorDelegate* alloc);

		/// <summary>
		/// 管理するメモリのサイズを変更する
		/// </summary>
		/// <param name="size"></param>
		/// <returns></returns>
		bool Resize(const uint32_t capacity);

		/// <summary>
		/// データをコピーする
		/// </summary>
		/// <param name="begin"></param>
		/// <param name="memSize"></param>
		void Copy(const void* begin, const uint32_t memSize);

		//-----------------------------------------
		// 取得
		//-----------------------------------------

		/// <summary>
		/// 使用してるメモリアロケータを取得
		/// </summary>
		/// <returns></returns>
		inline MemoryAllocatorDelegate* GetMemoryAllocatorAPI()const
		{
			return alloc_.Get();
		}

		/// <summary>
		/// 確保したメモリの先頭アドレスを取得
		/// </summary>
		/// <returns></returns>
		inline void* Data()const
		{
			return mem_;
		}

		/// <summary>
		/// ブロックの容量を取得
		/// </summary>
		/// <returns></returns>
		inline uint32_t Capacity()const
		{
			return capacity_;
		}

	private:

		MemoryAllocatorRef alloc_; // 確保・開放に使うアロケータ(未指定なら malloc/free)

		void* mem_; // 確保済みメモリの先頭(未確保は nullptr。このクラスが所有)

		uint32_t capacity_; // 確保しているバイト数
	};
}
