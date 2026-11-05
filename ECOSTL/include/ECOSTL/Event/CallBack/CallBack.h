#pragma once

// ChainCallBack / ChainCallBackHolder: 確保ゼロのコールバック登録。
// - 呼ぶときに読むデータ (関数, data) は Holder 内の連続した配列に登録順で持つ(Call はそれを前から呼ぶだけ)。
//   登録・解除のときだけ使うデータ (番号・受け手の位置) は別の配列に分ける。連結リストはない。
// - Holder が持てる受け手は Capacity 個(既定 8、固定長で確保なし)。満杯なら Register / Add は false / 0 を返す。
// - 受け手(ChainCallBack)は 登録先 + 自分の番号 だけを持つ(未登録の間は関数と data)。ポインタでつながないので、
//   受け手は連続配列に並べてよい。ただし Holder の破棄時に受け手を未登録へ戻すため Holder は受け手のアドレスを覚える。
//   登録中の受け手はメモリ上で動かさない(動かすなら先に Unregister)。
// - ECS のコンポーネントのように動くものに持たせるときは、受け手でなくハンドル(番号 uint32_t)を使う:
//   id = holder.Add(data, func); ... holder.Remove(id);  ハンドルはアドレスを持たないので動いても壊れない。
//   Holder が先に消えたときの後始末は持ち主(システム)が Holder.Clear() で行う。
// - 呼ばれる順は登録順。関数が null は登録しない。Create は Register より前に行う(登録後の Create は反映されない)。
// - スレッド安全: Call は読むだけなので複数スレッドから並行に呼んでよい。
//   Register / Unregister / Clear / Create / Add / Remove は Call と同時に行わない前提。
// - 二重登録: 同じ受け手、または同じ (関数, data) の組は無視する(false / 0)。
// - 個別解除: Holder::Unregister(&cb) / cb.Unregister() / Holder::Remove(id)。登録順は保たれる。
// - 受け手・Holder の破棄は互いに安全。受け手はコピー/ムーブしても未登録になる。
// - 1つの受け手を登録できる Holder は1つだけ。
#include <cstdint>

// 汎用ユーティリティ名前空間
namespace utl
{
	// 前方宣言: 受け手と相互参照するため先に宣言する
	template<unsigned Capacity, typename... FuncArgTypes>
	class ChainCallBackHolderN;

	// 既定容量 8 の Holder
	template<typename... FuncArgTypes>
	using ChainCallBackHolder = ChainCallBackHolderN<8, FuncArgTypes...>;

	/// <summary>
	/// コールバックを表すクラス
	/// </summary>
	template<typename...FuncArgTypes>
	class ChainCallBack
	{
		using FuncType = void(*)(void*, const FuncArgTypes&...);
		using HolderType = ChainCallBackHolder<FuncArgTypes...>;
	public:
		ChainCallBack() : registered_(false) { u_.pending.func = nullptr; u_.pending.data = nullptr; }

		ChainCallBack(void* data, FuncType func) : registered_(false) { u_.pending.func = func; u_.pending.data = data; }

		// コピー・ムーブでは登録状態を引き継がない
		ChainCallBack(const ChainCallBack& other) : registered_(false)
		{
			FuncType f; void* d; other.Get(f, d);
			u_.pending.func = f; u_.pending.data = d;
		}

		ChainCallBack& operator=(const ChainCallBack& other)
		{
			if (this != &other)
			{
				FuncType f; void* d; other.Get(f, d);
				Unregister();
				u_.pending.func = f; u_.pending.data = d;
			}
			return *this;
		}

		~ChainCallBack() { Unregister(); }

		/// <summary>
		/// コールバックを作成する(登録前に呼ぶ。登録中は何もしない)
		/// </summary>
		inline void Create(void* data, FuncType func)
		{
			if (!registered_)
			{
				u_.pending.data = data;
				u_.pending.func = func;
			}
		}

		/// <summary>
		/// この受け手だけを呼び出す
		/// </summary>
		inline void Call(const FuncArgTypes&...args) const
		{
			FuncType f; void* d; Get(f, d);
			if (f != nullptr)
			{
				f(d, args...);
			}
		}

		inline bool Unregister()
		{
			return registered_ ? u_.reg.holder->Unregister(this) : false;
		}

		/// <summary>
		/// 互換: Unregister と同じ
		/// </summary>
		inline void Clear() { Unregister(); }

		inline bool IsRegistered() const { return registered_; }

	private:
		template<unsigned C, typename... A> friend class ChainCallBackHolderN; // Holder が登録状態(registered_, u_)を直接書き換えるため

		// 登録中は Holder から、未登録なら自分から関数と data を取り出す(O(Capacity))
		inline void Get(FuncType& f, void*& d) const
		{
			if (registered_) { u_.reg.holder->Find(u_.reg.id, f, d); }
			else { f = u_.pending.func; d = u_.pending.data; }
		}

		// 未登録: 関数と data / 登録中: 登録先と番号
		union U
		{
			struct { FuncType func; void* data; } pending; // 未登録時に持つ関数と data
			struct { HolderType* holder; uint32_t id; } reg; // 登録中の登録先と番号(0 は無効値)
		} u_; // registered_ で使うメンバが切り替わる(同時には使わない)
		bool registered_; // true の間は u_.reg、false の間は u_.pending が有効
	};

	/// <summary>
	/// 最大 Capacity 個の受け手を登録順に持ち、まとめて呼ぶ(確保なし)
	/// </summary>
	template<unsigned Capacity, typename...FuncArgTypes>
	class ChainCallBackHolderN
	{
		using NodeType = ChainCallBack<FuncArgTypes...>;
		using FuncType = void(*)(void*, const FuncArgTypes&...);
		static_assert(Capacity > 0, "Capacity");
	public:
		ChainCallBackHolderN() : count_(0), nextId_(1) {}

		ChainCallBackHolderN(const ChainCallBackHolderN&) = delete;
		ChainCallBackHolderN& operator=(const ChainCallBackHolderN&) = delete;

		~ChainCallBackHolderN() { Clear(); }

		/// <summary>
		/// 受け手を登録する。登録済み・同じ (関数, data) が登録済み・満杯なら false
		/// </summary>
		inline bool Register(NodeType* callback)
		{
			static_assert(Capacity == 8, "受け手(ChainCallBack)を登録できるのは既定の Capacity(8) の Holder。他は Add / Remove のハンドルを使う");
			if (callback == nullptr || callback->registered_)
			{
				return false;
			}
			const uint32_t id = Add(callback->u_.pending.data, callback->u_.pending.func);
			if (id == 0)
			{
				return false;
			}
			callback->u_.reg.holder = this;
			callback->u_.reg.id = id;
			callback->registered_ = true;
			owner_[count_ - 1] = callback;
			return true;
		}

		/// <summary>
		/// 番号で登録する(動くものに持たせるハンドル用)。0 は失敗(null 関数・二重・満杯)
		/// </summary>
		inline uint32_t Add(void* data, FuncType func)
		{
			if (func == nullptr || count_ >= Capacity)
			{
				return 0;
			}
			for (unsigned i = 0; i < count_; ++i)
			{
				if (flat_[i].func == func && flat_[i].data == data)
				{
					return 0;
				}
			}
			uint32_t id = nextId_++;
			if (nextId_ == 0) { nextId_ = 1; }
			flat_[count_].func = func;
			flat_[count_].data = data;
			id_[count_] = id;
			owner_[count_] = nullptr;
			++count_;
			return id;
		}

		/// <summary>
		/// 受け手を個別に解除する。登録順は保たれる
		/// </summary>
		inline bool Unregister(NodeType* callback)
		{
			if (callback == nullptr || !callback->registered_ || callback->u_.reg.holder != this)
			{
				return false;
			}
			const uint32_t id = callback->u_.reg.id;
			FuncType f; void* d;
			Find(id, f, d);
			callback->u_.pending.func = f;
			callback->u_.pending.data = d;
			callback->registered_ = false;
			return Remove(id);
		}

		/// <summary>
		/// 番号で解除する
		/// </summary>
		inline bool Remove(uint32_t id)
		{
			for (unsigned i = 0; i < count_; ++i)
			{
				if (id_[i] != id)
				{
					continue;
				}
				for (unsigned j = i + 1; j < count_; ++j)
				{
					flat_[j - 1] = flat_[j];
					id_[j - 1] = id_[j];
					owner_[j - 1] = owner_[j];
				}
				--count_;
				return true;
			}
			return false;
		}

		/// <summary>
		/// 登録順に呼ぶ(確保なし・間接参照なし)
		/// </summary>
		inline void Call(const FuncArgTypes&...args) const
		{
			for (unsigned i = 0; i < count_; ++i)
			{
				flat_[i].func(flat_[i].data, args...);
			}
		}

		/// <summary>
		/// 全登録を解除する。登録中の受け手は未登録へ戻す(O(Count))
		/// </summary>
		inline void Clear()
		{
			for (unsigned i = 0; i < count_; ++i)
			{
				if (owner_[i] != nullptr)
				{
					NodeType* n = owner_[i];
					n->u_.pending.func = flat_[i].func;
					n->u_.pending.data = flat_[i].data;
					n->registered_ = false;
				}
			}
			count_ = 0;
		}

		inline bool Contains(const NodeType* callback) const
		{
			return callback != nullptr && callback->registered_ && callback->u_.reg.holder == this;
		}

		// 番号 id が登録中か(O(Count))
		inline bool Contains(uint32_t id) const
		{
			for (unsigned i = 0; i < count_; ++i) { if (id_[i] == id) { return true; } }
			return false;
		}

		inline unsigned Count() const { return count_; }

	private:
		friend class ChainCallBack<FuncArgTypes...>;

		// 番号から関数と data を探す。無ければ null を入れて false(O(Count))
		inline bool Find(uint32_t id, FuncType& f, void*& d) const
		{
			for (unsigned i = 0; i < count_; ++i)
			{
				if (id_[i] == id) { f = flat_[i].func; d = flat_[i].data; return true; }
			}
			f = nullptr; d = nullptr;
			return false;
		}

		// Call で読む1件分(関数と data)
		struct Entry
		{
			FuncType func; // 呼ぶ関数
			void* data;    // 関数の第1引数に渡す利用者データ(所有しない)
		};

		// 呼ぶときに読む部分
		Entry flat_[Capacity];
		unsigned count_;  // 登録数(0〜Capacity)
		uint32_t nextId_; // 次に払い出す番号(1 から。0 は失敗値なので飛ばす)
		// 登録・解除のときだけ使う部分
		uint32_t id_[Capacity];
		NodeType* owner_[Capacity]; // 受け手経由の登録なら受け手、ハンドル経由なら nullptr
	};
}
