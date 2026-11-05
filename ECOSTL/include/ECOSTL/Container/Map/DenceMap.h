#pragma once
// キーから要素を引ける、要素が連続メモリに並ぶ密なマップ DenceMap
#include "../Set/SparseSet.h"
#include "ECOSTL/Algorithm/Sort/Sort.h"

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// メモリが密集するマップ
	/// ハッシュ値など、無茶な大きさのキーは使用不可
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class DenceMap
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		/// <summary>
		/// 
		/// </summary>
		/// <param name="alloc"></param>
		DenceMap(MemoryAllocatorDelegate* alloc = nullptr)
			:table_(alloc)
			, values_(alloc)
		{

		}
		/// <summary>
		/// キャパシティとメモリアロケータを指定して作成するコンストラクタ
		/// </summary>
		/// <param name="capacity">キャパシティ</param>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ</param>
		DenceMap(const uint32_t capacity, MemoryAllocatorDelegate* alloc = nullptr)
			:table_(alloc)
			, values_(capacity, alloc)
		{

		}
		/// <summary>
		/// キャパシティと雑多な用途で使用するメモリアロケータと要素を格納するのに使うメモリアロケータを指定して作成するコンストラクタ
		/// </summary>
		/// <param name="capacity">キャパシティ</param>
		/// <param name="utlAlloc">キーと添え字の紐づけなど、雑多な用途で使用するメモリを提供するアロケータ</param>
		/// <param name="valueAlloc">要素のインスタンスを格納するメモリを提供するアロケータ</param>
		DenceMap(const uint32_t capacity, MemoryAllocatorDelegate* utlAlloc, MemoryAllocatorDelegate* valueAlloc)
			:table_(utlAlloc)
			, values_(capacity, valueAlloc)
		{

		}
		DenceMap(const uint32_t capacity, const MemoryAllocatorRef& utlAlloc, const MemoryAllocatorRef& valueAlloc)
			: DenceMap(capacity, utlAlloc.Get(), valueAlloc.Get())
		{
		}

		// コピーは元と同じアロケータを使う
		DenceMap(const DenceMap& src) = default;
		DenceMap& operator=(const DenceMap& src) = default;
		DenceMap(DenceMap&& src) noexcept = default;

		/// <summary>
		/// 2つのキーの値を入れ替える（値の場所は動かさず、キーが指す先を入れ替える。ECOSTL dev から）
		/// 片方だけ登録済みなら、その値を、もう片方のキーに付け替える。両方未登録なら false
		/// </summary>
		bool SwapKeys(const SparseKey keyA, const SparseKey keyB)
		{
			return table_.SwapKeys(keyA, keyB);
		}

		//------------------------------------------
		// 追加
		//------------------------------------------


		// key に src のコピーを紐づけて追加する。既存キーなら上書き。末尾追加のため平均 O(1)。追加した要素のポインタを返す(拡張時に既存ポインタは無効化されうる)
		Type* Insert(const SparseKey key, const Type& src)
		{
			DenceIndex index = NULL_VALUE;

			// すでにキーが追加されていたら上書きする
			if (table_.CheckActive(key))
			{
				index = table_[key];
				values_[index] = src;
				return &values_[index];
			}

			index = table_.Insert(key);
			// 要素をコンテナに追加
			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			return values_.EmplaceBack(src);
		}


		// key に args から構築した要素を紐づけて追加する。既存キーなら上書き。平均 O(1)。追加した要素のポインタを返す
		template<typename...ArgTypes>
		Type* Insert(const SparseKey key, ArgTypes&&...args)
		{
			DenceIndex index = NULL_VALUE;

			// すでにキーが追加されていたら上書きする
			if (table_.CheckActive(key))
			{
				index = table_[key];
				values_[index] = Type(args...);
				return &values_[index];
			}

			index = table_.Insert(key);
			// 要素をコンテナに追加
			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			return values_.EmplaceBack(args...);
		}

		//------------------------------------------
		// 削除
		//------------------------------------------

		// key の要素を削除する。末尾の要素を削除位置へ移して密を保つため O(1) だが順序は保たれない。未登録なら何もしない
		void Remove(const SparseKey removeKey)
		{
			// すでに無効なら何もしない
			if (!table_.CheckActive(removeKey))
			{
				return;
			}

			// SparseSet::Remove は削除したキーのあった位置を返すため、
			// 末尾の位置は削除の前に取っておく
			const DenceIndex remIndex = table_.GetIndex(removeKey);
			const DenceIndex backIndex = table_.GetBackIndex();

			// テーブルからキーを削除（末尾のキーが削除位置へ移る）
			table_.Remove(removeKey);

			// 削除する要素の末尾の要素を上書き
			if (remIndex != backIndex)
			{
				values_[remIndex] = utl::Move(values_[backIndex]);
			}

			// 末尾の要素を無効化
			values_.PopBack();
		}

		//------------------------------------------
		// 並べ替え
		//------------------------------------------

		/// <summary>
		/// 並べ替えの作業用要素。呼び出し側が Size() 個以上の配列を渡す（Sort 内で確保しない）
		/// </summary>
		template<typename KeyType = uint32_t>
		struct SortEntry
		{
			// keyFunc が返した比較用の鍵
			KeyType sortKey;
			// その要素に紐づくキー
			SparseKey key;
			// 並べ替え前の密配列上の添え字(同鍵のとき元の順を保つ決め手にも使う)
			DenceIndex index;
		};

		/// <summary>
		/// 鍵を返す関数で昇順に並べ替える（鍵が同じなら今の並びを保つ）
		/// 外から持っているキー（SparseKey）はそのまま使える。
		/// 値は1要素につき1回だけ動かす（巡回の起点のみ+1回）
		/// </summary>
		/// <param name="keyFunc">const Type& を受けて比較可能な鍵を返す</param>
		/// <param name="work">Size() 個以上の作業用配列</param>
		template<typename KeyFunc, typename KeyType>
		void SortByKey(KeyFunc&& keyFunc, SortEntry<KeyType>* work)
		{
			const uint32_t num = static_cast<uint32_t>(values_.Size());
			if (num < 2)
			{
				return;
			}

			const SparseKey* keys = table_.GetKeys().Begin();
			Type* values = values_.Begin();
			for (uint32_t i = 0; i < num; ++i)
			{
				work[i].sortKey = keyFunc(static_cast<const Type&>(values[i]));
				work[i].key = keys[i];
				work[i].index = i;
			}

			utl::Sort(work, num, [](const SortEntry<KeyType>& a, const SortEntry<KeyType>& b)
			{
				if (a.sortKey < b.sortKey) { return true; }
				if (b.sortKey < a.sortKey) { return false; }
				return a.index < b.index;
			});

			ApplySorted(work, num);
		}

		/// <summary>
		/// 比較関数 less(const Type&, const Type&) で並べ替える（鍵が同じなら今の並びを保つ）
		/// work は SortByKey と同じ（KeyType は使わないので何でもよい）
		/// </summary>
		template<typename Less, typename KeyType>
		void Sort(Less&& less, SortEntry<KeyType>* work)
		{
			const uint32_t num = static_cast<uint32_t>(values_.Size());
			if (num < 2)
			{
				return;
			}
			// 値そのものを比べるため、一旦 index と key だけ詰めて index 経由で比べる
			const SparseKey* keys = table_.GetKeys().Begin();
			Type* values = values_.Begin();
			for (uint32_t i = 0; i < num; ++i)
			{
				work[i].key = keys[i];
				work[i].index = i;
			}
			utl::Sort(work, num, [&](const SortEntry<KeyType>& a, const SortEntry<KeyType>& b)
			{
				const Type& va = values[a.index];
				const Type& vb = values[b.index];
				if (less(va, vb)) { return true; }
				if (less(vb, va)) { return false; }
				return a.index < b.index;
			});
			ApplySorted(work, num);
		}

		// 全要素とキーを破棄する(確保済みメモリは残す)
		void Clear()
		{
			table_.Reset();
			values_.Clear();
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		// キー表・値配列の確保サイズを現在の要素数に切り詰める
		void Fit()
		{
			table_.Fit();
			values_.Fit();
		}

		// キー表・値配列の確保サイズを要素数に少し余裕を持たせた大きさに調整する
		void FitSlack()
		{
			table_.FitSlack();
			values_.FitSlack();
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		Type* Begin()
		{
			return values_.Begin();
		}
		const Type* Begin()const
		{
			return values_.Begin();
		}

		// key の要素を探してポインタを返す。未登録なら nullptr。O(1)
		Type* Find(const SparseKey key)
		{
			if (!table_.CheckActive(key))
			{
				return nullptr;
			}
			DenceIndex index = table_.GetIndex(key);
			return &values_[index];
		}
		const Type* Find(const SparseKey key)const
		{
			if (!table_.CheckActive(key))
			{
				return nullptr;
			}
			DenceIndex index = table_.GetIndex(key);
			return &values_[index];
		}

		size_t Size()const
		{
			return table_.Size();
		}

		// key の要素を返す。未登録ならデフォルト構築で末尾に追加してから返す。O(1)
		Type& GetOrAdd(const SparseKey key)
		{
			DenceIndex index = NULL_VALUE;
			if (table_.CheckActive(key))
			{
				index = table_[key];
				return values_[index];
			}

			index = table_.Insert(key);
			// 要素をコンテナに追加
			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			return *values_.EmplaceBack();
		}

		Type& operator[](const SparseKey key)
		{
			return GetOrAdd(key);
		}

		DenceMap& operator=(DenceMap&& src) noexcept
		{
			table_ = utl::Move(src.table_);
			values_ = utl::Move(src.values_);
			return *this;
		}


		bool CheckActive(const SparseKey key)const
		{
			return table_.CheckActive(key);
		}

		const Vector<SparseKey>& GetKeys()const
		{
			return table_.GetKeys();
		}

		const uint32_t GetKeyNum()const
		{
			return static_cast<uint32_t>(table_.Size());
		}


	private:

		/// <summary>
		/// 並べ替え済みの work[i]（元の添え字 index・元のキー key）に従ってキーと値を入れ替える
		/// </summary>
		template<typename Entry>
		void ApplySorted(Entry* work, const uint32_t num) // 並べ替え済みの work[0..num) に従ってキー表と値配列を並べ替える
		{
			Type* values = values_.Begin();

			// キーの付け替え（添え字 i には、元の添え字 work[i].index のキーが来る）
			for (uint32_t i = 0; i < num; ++i)
			{
				table_.Rebind(i, work[i].key);
			}

			// 値の並べ替え（巡回に沿って1回ずつ動かす）。済んだ位置は index を自分自身にして印を付ける
			for (uint32_t start = 0; start < num; ++start)
			{
				if (work[start].index == start)
				{
					continue;
				}
				Type tmp = utl::Move(values[start]);
				uint32_t dst = start;
				while (work[dst].index != start)
				{
					const uint32_t srcIndex = work[dst].index;
					values[dst] = utl::Move(values[srcIndex]);
					work[dst].index = dst;
					dst = srcIndex;
				}
				values[dst] = utl::Move(tmp);
				work[dst].index = dst;
			}
		}

		// キー(SparseKey)から値配列上の添え字への対応表
		SparseSet table_;

		// 要素本体。有効な要素を前詰めで連続して持つ
		Vector<Type> values_;

	};
}

