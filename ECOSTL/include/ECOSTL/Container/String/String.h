#pragma once
// 小さな文字列クラス String（確保はアロケータ経由）と ToString。
// 名前は ECOSTL の流儀（CStr・Size・IsEmpty・Find・Substr など）。std::string と同じ名前の別名は持たない（範囲 for 用の begin/end だけは持つ）。
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// 最小限の文字列（ログ・ファイル名・UI 文字列用）。末尾は常に '\0'。空のときは確保しない。
	/// アロケータは作るときに渡す（所有しない。nullptr なら既定のアロケータ）。コピー・連結の結果は元（左側）と同じアロケータを使う。
	/// </summary>
	class String
	{
	public:
		// 「見つからない／長さ指定なし」を表す値
		static constexpr size_t NOT_FOUND = static_cast<size_t>(-1);

		String() = default;
		explicit String(MemoryAllocatorDelegate* alloc) : alloc_(alloc) {}
		String(const char* text, MemoryAllocatorDelegate* alloc = nullptr) : alloc_(alloc) { Assign(text, text != nullptr ? ::strlen(text) : 0); }
		String(const char* text, const size_t length, MemoryAllocatorDelegate* alloc = nullptr) : alloc_(alloc) { Assign(text, length); }
		String(const size_t count, const char c, MemoryAllocatorDelegate* alloc = nullptr) : alloc_(alloc) { Resize(count, c); }
		// コピーは元と同じアロケータを使う
		String(const String& src) : alloc_(src.alloc_) { Assign(src.data_, src.size_); }
		String(String&& src) noexcept : alloc_(src.alloc_), data_(src.data_), size_(src.size_), capacity_(src.capacity_)
		{
			src.data_ = nullptr; src.size_ = 0; src.capacity_ = 0;
		}
		~String() { alloc_.Free(data_); }

		// コピー代入は自分のアロケータのまま中身を写す
		String& operator=(const String& src)
		{
			if (this != &src) { Assign(src.data_, src.size_); }
			return *this;
		}
		String& operator=(String&& src) noexcept
		{
			if (this != &src)
			{
				alloc_.Free(data_);
				alloc_ = src.alloc_;
				data_ = src.data_; size_ = src.size_; capacity_ = src.capacity_;
				src.data_ = nullptr; src.size_ = 0; src.capacity_ = 0;
			}
			return *this;
		}
		String& operator=(const char* text) { Assign(text, text != nullptr ? ::strlen(text) : 0); return *this; }

		//--------------------------------
		// 取得
		//--------------------------------

		// '\0' 終端の文字列（空なら ""。nullptr は返さない）
		const char* CStr() const { return data_ != nullptr ? data_ : ""; }
		uint32_t Size() const { return static_cast<uint32_t>(size_); }
		bool IsEmpty() const { return size_ == 0; }
		char& operator[](const size_t i) { return data_[i]; }
		const char& operator[](const size_t i) const { return data_[i]; }
		// 最後の文字（空なら assert）
		char Back() const { assert(size_ > 0); return data_[size_ - 1]; }
		// 確保に使うアロケータ
		const MemoryAllocatorRef& MemAllocator() const { return alloc_; }

		// 範囲 for 文用（言語が要求する名前）
		const char* begin() const { return CStr(); }
		const char* end() const { return CStr() + size_; }

		//--------------------------------
		// 変更
		//--------------------------------

		// 空にする（確保済みの領域は残す）
		void Clear() { size_ = 0; if (data_ != nullptr) { data_[0] = '\0'; } }
		// 文字数 n 以上のキャパシティを確保する（足りない時のみ確保し直す）。長さは変えない
		void Reserve(const size_t n) { if (n > capacity_) { Grow(n); } }
		// 長さを n にする。伸ばす分は文字 c で埋める
		void Resize(const size_t n, const char c = '\0')
		{
			if (n > capacity_) { Grow(n); }
			if (n > size_) { ::memset(data_ + size_, c, n - size_); }
			size_ = n;
			if (data_ != nullptr) { data_[size_] = '\0'; }
		}
		void PushBack(const char c) { Append(&c, 1); }
		// text の先頭 length 文字を末尾に追加する。足りなければ 1.5 倍で確保し直す。text が自分自身の中を指していても安全
		String& Append(const char* text, const size_t length)
		{
			if (length == 0) { return *this; }
			const size_t need = size_ + length;
			if (need > capacity_)
			{
				// text が自分の中を指していても壊れないように、確保し直す前にオフセットで覚える
				const bool self = data_ != nullptr && text >= data_ && text < data_ + size_;
				const size_t off = self ? static_cast<size_t>(text - data_) : 0;
				Grow(need + (need >> 1));
				if (self) { text = data_ + off; }
			}
			::memmove(data_ + size_, text, length);
			size_ = need;
			data_[size_] = '\0';
			return *this;
		}
		String& operator+=(const String& s) { return Append(s.data_, s.size_); }
		String& operator+=(const char* s) { return s != nullptr ? Append(s, ::strlen(s)) : *this; }
		String& operator+=(const char c) { return Append(&c, 1); }

		//--------------------------------
		// 検索・切り出し
		//--------------------------------

		// pos から count 文字を切り出す（自分と同じアロケータを使う）
		String Substr(const size_t pos, const size_t count = NOT_FOUND) const
		{
			if (pos >= size_) { return String(alloc_.Get()); }
			const size_t n = count < size_ - pos ? count : size_ - pos;
			return String(data_ + pos, n, alloc_.Get());
		}
		// 文字 c を pos 以降で探し、最初の添え字を返す。無ければ NOT_FOUND。O(size)
		size_t Find(const char c, const size_t pos = 0) const
		{
			for (size_t i = pos; i < size_; ++i) { if (data_[i] == c) { return i; } }
			return NOT_FOUND;
		}
		// 部分文字列 text を pos 以降で探し、最初の添え字を返す。無い・text が nullptr なら NOT_FOUND
		size_t Find(const char* text, const size_t pos = 0) const
		{
			if (pos > size_ || text == nullptr) { return NOT_FOUND; }
			const char* p = ::strstr(CStr() + pos, text);
			return p != nullptr ? static_cast<size_t>(p - data_) : NOT_FOUND;
		}
		// 文字 c を末尾から探し、最後の添え字を返す。無ければ NOT_FOUND。O(size)
		size_t FindLast(const char c) const
		{
			for (size_t i = size_; i > 0; --i) { if (data_[i - 1] == c) { return i - 1; } }
			return NOT_FOUND;
		}

		//--------------------------------
		// 比較・連結（連結の結果は左側と同じアロケータを使う）
		//--------------------------------

		friend bool operator==(const String& a, const String& b) { return a.size_ == b.size_ && ::memcmp(a.CStr(), b.CStr(), a.size_) == 0; }
		friend bool operator==(const String& a, const char* b) { return ::strcmp(a.CStr(), b != nullptr ? b : "") == 0; }
		friend bool operator!=(const String& a, const String& b) { return !(a == b); }
		friend bool operator!=(const String& a, const char* b) { return !(a == b); }
		friend bool operator<(const String& a, const String& b) { return ::strcmp(a.CStr(), b.CStr()) < 0; }
		friend String operator+(const String& a, const String& b) { String r(a.alloc_.Get()); r.Reserve(a.size_ + b.size_); r.Append(a.data_, a.size_); r.Append(b.data_, b.size_); return r; }
		friend String operator+(const String& a, const char* b) { String r(a); r += b; return r; }
		friend String operator+(const char* a, const String& b) { String r(a, b.alloc_.Get()); r += b; return r; }
		friend String operator+(const String& a, const char b) { String r(a); r += b; return r; }
		friend String operator+(const char a, const String& b) { String r(1, a, b.alloc_.Get()); r += b; return r; }

	private:
		// キャパシティを cap 文字（終端 '\0' 用に +1 バイト）に確保し直して中身を写す。失敗時は abort
		void Grow(const size_t cap)
		{
			char* p = static_cast<char*>(alloc_.Allocate(static_cast<uint32_t>(cap + 1)));
			if (p == nullptr) { ::abort(); }
			if (data_ != nullptr) { ::memcpy(p, data_, size_ + 1); alloc_.Free(data_); }
			else { p[0] = '\0'; }
			data_ = p;
			capacity_ = cap;
		}
		// text の先頭 length 文字で内容を置き換える。length が 0 なら空にする（確保済みバッファは保持）
		void Assign(const char* text, const size_t length)
		{
			size_ = 0;
			if (length == 0) { if (data_ != nullptr) { data_[0] = '\0'; } return; }
			if (length > capacity_) { Grow(length); }
			::memmove(data_, text, length);
			size_ = length;
			data_[size_] = '\0';
		}

		// 確保に使うアロケータ（所有しない。未指定なら既定のアロケータ）
		MemoryAllocatorRef alloc_;
		// 文字列バッファ（'\0' 終端）。空のとき nullptr（確保しない）。String が所有し、デストラクタで alloc_ に返す
		char* data_ = nullptr;
		// 文字数（終端を含まない）
		size_t size_ = 0;
		// data_ に確保済みの文字数（終端 '\0' を含まない）
		size_t capacity_ = 0;
	};

	// 数値を10進の文字列にする（snprintf。std::to_string の代わり）。alloc は結果の String に使うアロケータ
	inline String ToString(const long long v, MemoryAllocatorDelegate* alloc = nullptr) { char b[32]; const int n = ::snprintf(b, sizeof(b), "%lld", v); return String(b, static_cast<size_t>(n), alloc); }
	inline String ToString(const unsigned long long v, MemoryAllocatorDelegate* alloc = nullptr) { char b[32]; const int n = ::snprintf(b, sizeof(b), "%llu", v); return String(b, static_cast<size_t>(n), alloc); }
	inline String ToString(const int v, MemoryAllocatorDelegate* alloc = nullptr) { return ToString(static_cast<long long>(v), alloc); }
	inline String ToString(const long v, MemoryAllocatorDelegate* alloc = nullptr) { return ToString(static_cast<long long>(v), alloc); }
	inline String ToString(const unsigned v, MemoryAllocatorDelegate* alloc = nullptr) { return ToString(static_cast<unsigned long long>(v), alloc); }
	inline String ToString(const unsigned long v, MemoryAllocatorDelegate* alloc = nullptr) { return ToString(static_cast<unsigned long long>(v), alloc); }
	// double を "%f" 形式（小数6桁）の文字列にする
	inline String ToString(const double v, MemoryAllocatorDelegate* alloc = nullptr) { char b[64]; const int n = ::snprintf(b, sizeof(b), "%f", v); return String(b, static_cast<size_t>(n), alloc); }
	inline String ToString(const float v, MemoryAllocatorDelegate* alloc = nullptr) { return ToString(static_cast<double>(v), alloc); }
}
