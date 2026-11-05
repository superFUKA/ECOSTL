#pragma once
// 連続した要素の読み取り専用の窓（ArrayView<T>）。先頭のポインタと要素数だけを持ち、所有も確保もしない。
// コンテナの中身を、コンテナの型に依らずに渡すときに使う。元のコンテナが変わる・破棄されると無効になる。
#include <cstdint>

// 汎用ユーティリティ名前空間
namespace utl
{
	template<typename T>
	class ArrayView
	{
	public:
		constexpr ArrayView() = default;
		constexpr ArrayView(const T* begin, const uint32_t size)
			: begin_(begin)
			, size_(size)
		{
		}

		inline constexpr uint32_t Size()const { return size_; }
		inline constexpr bool IsEmpty()const { return size_ == 0; }
		inline constexpr const T& operator[](const uint32_t index)const { return begin_[index]; }
		inline constexpr const T* Begin()const { return begin_; }
		inline constexpr const T* End()const { return begin_ + size_; }

		// 範囲 for 文用
		inline constexpr const T* begin()const { return begin_; }
		inline constexpr const T* end()const { return begin_ + size_; }

	private:
		const T* begin_ = nullptr;  // 先頭（所有しない）
		uint32_t size_ = 0;  // 要素数
	};
}
