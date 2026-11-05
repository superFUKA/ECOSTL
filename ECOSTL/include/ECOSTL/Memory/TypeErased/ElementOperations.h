#pragma once
// 型を消去した配列要素の生存期間操作（作成・破棄・コピー・移動）。
// 要素の型を知らない側（例: ECS のチャンク）が、型ごとに選ばれた関数ポインタを通して要素の配列を操作するための部品。
//  - 平凡な型はバイト列の操作(memset / memcpy / memmove)にし、そうでない型はコンストラクタ・デストラクタ・代入を呼ぶ。
//  - 関数はすべて (先頭, 要素サイズ, 開始添え字, 個数) の形をとり、先頭が nullptr なら false、個数 0 なら何もせず true を返す。
// 標準ライブラリだけに依存する。
#include "ECOSTL/Core/Template/Template.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>

// ecoSTL の共通名前空間
namespace utl
{
	// 要素を作成・破棄・初期化する関数の型 (配列の先頭, 要素のバイト数, 開始添え字, 個数)
	using ElementLifetimeFunc = bool(*)(void* begin, uint32_t elementSize, uint32_t beginIndex, uint32_t num);
	// 要素を別の配列へコピー・移動する関数の型 (移動先の先頭, 要素のバイト数, 移動先の開始添え字, 移動元の先頭, 移動元の開始添え字, 個数)
	using ElementTransferFunc = bool(*)(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num);


	//-------------------------------------
	// 型情報を使用しない（バイト列としての）操作
	//-------------------------------------

	// 要素の範囲を 0 で埋める
	inline bool ZeroFillElements(void* begin, uint32_t elementSize, uint32_t beginIndex, uint32_t num)
	{
		if (begin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		size_t beginCharIndex = static_cast<size_t>(elementSize) * static_cast<size_t>(beginIndex);
		char* beginPtr = static_cast<char*>(begin) + (beginCharIndex);

		size_t length = static_cast<size_t>(elementSize) * static_cast<size_t>(num);
		std::memset(beginPtr, 0, length);

		return true;
	}

	// 何もしない（平凡に破棄できる型の破棄に使う）
	inline bool NoOpElements(void* begin, uint32_t elementSize, uint32_t beginIndex, uint32_t num)
	{
		return true;
	}

	// 要素の範囲をバイト列としてコピーする（範囲は重ならないこと）
	inline bool CopyElementBytes(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num)
	{
		if (dstBegin == nullptr || srcBegin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		size_t dstBeginCharIndex = static_cast<size_t>(elementSize) * static_cast<size_t>(dstBeginIndex);
		char* dstPtr = static_cast<char*>(dstBegin) + dstBeginCharIndex;

		size_t srcBeginCharIndex = static_cast<size_t>(elementSize) * static_cast<size_t>(srcBeginIndex);
		const char* srcPtr = static_cast<const char*>(srcBegin) + srcBeginCharIndex;

		size_t length = static_cast<size_t>(elementSize) * static_cast<size_t>(num);
		std::memcpy(static_cast<void*>(dstPtr), static_cast<const void*>(srcPtr), length);

		return true;
	}

	// 要素の範囲をバイト列として移動する（範囲が重なってもよい）
	inline bool MoveElementBytes(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num)
	{
		if (dstBegin == nullptr || srcBegin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		size_t dstBeginCharIndex = static_cast<size_t>(elementSize) * static_cast<size_t>(dstBeginIndex);
		char* dstPtr = static_cast<char*>(dstBegin) + dstBeginCharIndex;

		size_t srcBeginCharIndex = static_cast<size_t>(elementSize) * static_cast<size_t>(srcBeginIndex);
		const char* srcPtr = static_cast<const char*>(srcBegin) + srcBeginCharIndex;

		size_t length = static_cast<size_t>(elementSize) * static_cast<size_t>(num);
		std::memmove(dstPtr, srcPtr, length);

		return true;
	}


	//-------------------------------------
	// 型情報を使用した操作
	//-------------------------------------

	// 既定のコンストラクタで要素を作成する
	template<typename T>
	bool DefaultConstructElements(void* begin, uint32_t elementSize, uint32_t beginIndex, uint32_t num)
	{
		if (begin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		T* beginPtr = static_cast<T*>(begin);
		for (uint32_t addIndex = 0; addIndex < num; ++addIndex)
		{
			new (beginPtr + (beginIndex + addIndex)) T();
		}
		return true;
	}

	// デストラクタで要素を破棄する
	template<typename T>
	bool DestructElements(void* begin, uint32_t elementSize, uint32_t beginIndex, uint32_t num)
	{
		if (begin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		T* beginPtr = static_cast<T*>(begin);
		for (uint32_t index = 0; index < num; ++index)
		{
			(beginPtr + (beginIndex + index))->~T();
		}
		return true;
	}

	// コピーコンストラクタで要素を作成する
	template<typename T>
	bool CopyConstructElements(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num)
	{
		if (dstBegin == nullptr || srcBegin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		T* dstPtr = static_cast<T*>(dstBegin);
		const T* srcPtr = static_cast<const T*>(srcBegin);
		for (uint32_t index = 0; index < num; ++index)
		{
			new (dstPtr + (dstBeginIndex + index)) T(*(srcPtr + (srcBeginIndex + index)));
		}
		return true;
	}

	// ムーブコンストラクタで要素を作成する
	template<typename T>
	bool MoveConstructElements(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num)
	{
		if (dstBegin == nullptr || srcBegin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		T* dstPtr = static_cast<T*>(dstBegin);
		T* srcPtr = static_cast<T*>(srcBegin);
		for (uint32_t index = 0; index < num; ++index)
		{
			new (dstPtr + (dstBeginIndex + index)) T(utl::Move(*(srcPtr + (srcBeginIndex + index))));
		}
		return true;
	}

	// コピー代入で要素を上書きする
	template<typename T>
	bool CopyAssignElements(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num)
	{
		if (dstBegin == nullptr || srcBegin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		T* dstPtr = static_cast<T*>(dstBegin);
		const T* srcPtr = static_cast<const T*>(srcBegin);
		for (uint32_t index = 0; index < num; ++index)
		{
			(*(dstPtr + (dstBeginIndex + index))) = (*(srcPtr + (srcBeginIndex + index)));
		}
		return true;
	}

	// ムーブ代入で要素を上書きする
	template<typename T>
	bool MoveAssignElements(void* dstBegin, uint32_t elementSize, uint32_t dstBeginIndex, void* srcBegin, uint32_t srcBeginIndex, uint32_t num)
	{
		if (dstBegin == nullptr || srcBegin == nullptr)
		{
			return false;
		}
		if (num == 0)
		{
			return true;
		}

		T* dstPtr = static_cast<T*>(dstBegin);
		T* srcPtr = static_cast<T*>(srcBegin);
		for (uint32_t index = 0; index < num; ++index)
		{
			(*(dstPtr + (dstBeginIndex + index))) = utl::Move(*(srcPtr + (srcBeginIndex + index)));
		}
		return true;
	}


	//-------------------------------------
	// 操作の組
	//-------------------------------------

	/// <summary>
	/// 1 つの型の要素配列を操作する関数ポインタの組
	/// </summary>
	struct ElementOperations
	{
		// 新しく作成する
		ElementLifetimeFunc construct = nullptr;
		// 破棄する
		ElementLifetimeFunc destroy = nullptr;
		// 初期状態に戻す
		ElementLifetimeFunc reset = nullptr;
		// コピーして作成する
		ElementTransferFunc copyConstruct = nullptr;
		// 移動して作成する
		ElementTransferFunc moveConstruct = nullptr;
		// コピーして代入する
		ElementTransferFunc copyAssign = nullptr;
		// 移動して代入する
		ElementTransferFunc moveAssign = nullptr;
	};

	/// <summary>
	/// 型情報を使わない（すべてバイト列として扱う）操作の組を作る。0 埋めが初期状態になる型用
	/// </summary>
	inline ElementOperations MakeByteElementOperations()
	{
		ElementOperations result;
		result.construct = &ZeroFillElements;
		result.destroy = &NoOpElements;
		result.reset = &ZeroFillElements;
		result.copyConstruct = &CopyElementBytes;
		result.moveConstruct = &MoveElementBytes;
		result.copyAssign = &CopyElementBytes;
		result.moveAssign = &MoveElementBytes;
		return result;
	}

	/// <summary>
	/// 型 T の操作の組を作る。バイト列として扱ってよい操作はバイト列の操作にする
	/// T は既定のコンストラクタを持つこと
	/// </summary>
	template<typename T>
	ElementOperations MakeElementOperations()
	{
		ElementOperations result;

		static_assert(utl::IsDefaultConstructibleV<T>, "要素の型には既定のコンストラクタが必要");

		constexpr bool mustConstructor = !utl::IsTriviallyConstructibleV<T>;
		constexpr bool mustDestructor = !utl::IsTriviallyDestructibleV<T>;
		constexpr bool enableCopyCreate = utl::IsTriviallyCopyableV<T>;
		constexpr bool enableCopyOverwrite = enableCopyCreate && !mustDestructor;

		if constexpr (mustConstructor)
		{
			result.construct = &DefaultConstructElements<T>;
		}
		else
		{
			result.construct = &ZeroFillElements;
		}

		if constexpr (mustDestructor)
		{
			result.destroy = &DestructElements<T>;
		}
		else
		{
			result.destroy = &NoOpElements;
		}

		if constexpr (enableCopyCreate)
		{
			result.copyConstruct = &CopyElementBytes;
			result.moveConstruct = &MoveElementBytes;
		}
		else
		{
			result.copyConstruct = &CopyConstructElements<T>;
			result.moveConstruct = &MoveConstructElements<T>;
		}

		if constexpr (enableCopyOverwrite)
		{
			result.copyAssign = &CopyElementBytes;
			result.moveAssign = &MoveElementBytes;
		}
		else
		{
			result.copyAssign = &CopyAssignElements<T>;
			result.moveAssign = &MoveAssignElements<T>;
		}

		return result;
	}
}
