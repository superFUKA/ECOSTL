#pragma once
#pragma once
#include <utility>

namespace utl 
{
    template<class>
    class FunctionObject;                 // プライマリは未定義

    // 映像: R(Args...) 形式を分解
    template<class R, class... Args>
    class FunctionObject<R(Args...)>
    {
    public:
        // 呼び出し可能なら暗黙変換を許可
        template<
            class F,
            // キャプチャなし λ / 関数 / 静的メンバ関数は関数ポインタに変換可
            std::enable_if_t<std::is_convertible_v<F, R(*)(Args...)>, int> = 0
        >
        FunctionObject(F f) noexcept
            : obj_(nullptr), stub_(reinterpret_cast<R(*)(void*, Args...)>(f))
        {}

        // メンバ関数用コンストラクタ
        template<class C, R(C::* Method)(Args...)>
        static FunctionObject Bind(C* instance) noexcept
        {
            return FunctionObject{
                instance,
                [](void* o, Args... as) -> R
                {
                    return (static_cast<C*>(o)->*Method)(std::forward<Args>(as)...);
                }
            };
        }

        R operator()(Args... args) const
        {
            return stub_(obj_, std::forward<Args>(args)...);
        }

        // 呼び出し可否チェック
        explicit operator bool() const noexcept { return stub_ != nullptr; }

    private:
        FunctionObject(void* o, R(*s)(void*, Args...)) : obj_(o), stub_(s) {}

        void* obj_;
        R(*stub_)(void*, Args...);
    };

}