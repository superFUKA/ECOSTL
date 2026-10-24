#pragma once

namespace utl
{
	//----------------------------
	// Memory
	//----------------------------

	/// <summary>
	/// 
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <param name="src"></param>
	/// <returns></returns>
	template<typename Type>
	constexpr inline Type&& Move(Type& src)
	{
		return static_cast<Type&&>(src);
	}

	template<typename Type>
	struct RemoveReference
	{
		using TypeValue = Type;
	};

	template<typename Type>
	struct RemoveReference<Type&>
	{
		using TypeValue = Type;
	};

	template<typename Type>
	struct RemoveReference<Type&&>
	{
		using TypeValue = Type;
	};

	template<typename Type>
	using RemoveReferenceType = RemoveReference<Type>::TypeValue;

	template<typename Type>
	inline Type&& Forward(RemoveReferenceType<Type>& src)
	{
		return static_cast<Type&&>(src);
	}

	template<typename Type>
	inline Type&& Forward(RemoveReferenceType<Type>&& src)
	{
		return static_cast<Type&&>(src);
	}


	//----------------------------
	// Const
	//----------------------------

	/// <summary>
	/// �e���v���[�g�����̌^��Const��t�^�����^��擾����
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	struct AddConst
	{
		using Type = const CheckType;
	};
	/// <summary>
	/// �e���v���[�g�����̌^��Const��t�^�����^
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	using AddConstType = AddConst<CheckType>::Type;

	/// <summary>
	/// �e���v���[�g�����̌^����Const��폜�����^��擾����
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	struct RemoveConst
	{
		using Type = CheckType;
	};
	/// <summary>
	/// �e���v���[�g�����̌^����Const��폜�����^��擾����
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	struct RemoveConst<const CheckType>
	{
		using Type = CheckType;
	};
	/// <summary>
	/// �e���v���[�g�����̌^����Const��폜�����^
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	using RemoveConstType = RemoveConst<CheckType>::Type;

	/// <summary>
	/// �e���v���[�g�����̌^��Const����ÓI�����o�Ɏ���
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	struct IsConst
	{
		inline static bool value = false;
	};
	/// <summary>
	/// �e���v���[�g�����̌^��Const����ÓI�����o�Ɏ���
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	struct IsConst<const CheckType>
	{
		inline static bool value = true;
	};
	/// <summary>
	/// �e���v���[�g�����̌^��Const��
	/// </summary>
	/// <typeparam name="CheckType"></typeparam>
	template<typename CheckType>
	inline static constexpr bool isConstValue = IsConst<CheckType>::value;
}
