#pragma once
#include <cstdint>

namespace utl
{
	// 64ビットで表現されるハッシュ値の型エイリアス
	using Hash64 = uint64_t;
	// ハッシュの無効値
	constexpr Hash64 INVALID_HASH64 = UINT64_MAX;

	// 初期のハッシュ値
	inline constexpr Hash64 HASH_OFFSET = 0xcbf29ce484222325ULL;
	// 
	inline constexpr uint64_t HASH_PRIME = 0x00000100000001B3ULL;


	/// <summary>
	/// メモリの塊からハッシュ値を求める
	/// コンパイル時にサイズを指定できる場合に指定する
	/// </summary>
	/// <typeparam name="N"></typeparam>
	/// <param name="data"></param>
	/// <param name="seed"></param>
	/// <returns></returns>
	template<size_t Size>
	constexpr Hash64 Hash64FromBytes(const uint8_t(&data)[Size], Hash64 seed = HASH_OFFSET) noexcept
	{
		for (size_t i = 0; i < Size; ++i)
		{
			seed ^= data[i];
			seed *= HASH_PRIME;
		}
		return seed;
	}
	/// <summary>
	/// メモリの塊からハッシュ値を求める
	/// </summary>
	/// <typeparam name="N"></typeparam>
	/// <param name="data"></param>
	/// <param name="seed"></param>
	/// <returns></returns>
	template<size_t Size>
	constexpr Hash64 Hash64FromBytes(const uint8_t* data, Hash64 seed = HASH_OFFSET) noexcept
	{
		for (size_t i = 0; i < Size; ++i)
		{
			seed ^= data[i];
			seed *= HASH_PRIME;
		}
		return seed;
	}
	/// <summary>
	/// メモリの塊からハッシュ値を求める
	/// </summary>
	/// <param name="data"></param>
	/// <param name="size"></param>
	/// <param name="seed"></param>
	/// <returns></returns>
	constexpr Hash64 Hash64FromBytes(const uint8_t* data, uint64_t size, Hash64 seed = HASH_OFFSET) noexcept
	{
		for (size_t i = 0; i < size; ++i)
		{
			seed ^= *(data + i);
			seed *= HASH_PRIME;
		}
		return seed;
	}

	/// <summary>
	/// 特定のオブジェクトの型から
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="src"></param>
	/// <param name="seed"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr Hash64 Hash64FromType(const Type& src, Hash64 seed = HASH_OFFSET) noexcept
	{
		const uint8_t* data = reinterpret_cast<const uint8_t*>(&src);
		return Hash64FromBytes<sizeof(Type)>(data, seed);
	}

	/// <summary>
	/// 文字列からハッシュ値を求める
	/// </summary>
	/// <typeparam name="N"></typeparam>
	/// <param name="str"></param>
	/// <param name="seed"></param>
	/// <returns></returns>
	template<size_t Size>
	constexpr Hash64 Hash64FromStrgin(const char(&str)[Size], Hash64 seed = HASH_OFFSET) noexcept
	{
		for (size_t i = 0; i < Size - 1; ++i)
		{
			seed ^= static_cast<uint8_t>(str[i]);
			seed *= HASH_PRIME;
		}
		return seed;
	}

	/// <summary>
	/// 複数の型インスタンスからハッシュ値を求める
	/// </summary>
	/// <typeparam name="...Types"></typeparam>
	/// <param name="...src"></param>
	/// <param name="seed"></param>
	/// <returns></returns>
	template<typename...Types>
	constexpr Hash64 Hash64FromTypes(Types&...src) noexcept
	{
		Hash64 result = HASH_OFFSET;
		((result = Hash64FromType<Types>(src, result)), ...);
		return result;
	}
}
