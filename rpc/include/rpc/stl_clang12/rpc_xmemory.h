// xmemory internal header

// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#ifndef _XMEMORY_
#define _XMEMORY_
#include <rpc/stl_clang12/rpc_yvals_core.h>
#if _STL_COMPILER_PREPROCESSOR
#include <cstdint>
#include <stdexcept>
#include <cstdlib>
#include <limits>
#include <new>
#include <rpc/stl_clang12/rpc_xatomic.h>
#include <rpc/stl_clang12/rpc_xutility.h>



#if _HAS_CXX20
#include <tuple>
#endif // _HAS_CXX20

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
    #pragma GCC diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#else
#pragma pack(push, _CRT_PACKING)
#pragma warning(push, _STL_WARNING_LEVEL)
#pragma warning(disable : _STL_DISABLED_WARNINGS)
_STL_DISABLE_CLANG_WARNINGS
#pragma push_macro("new")
#endif
#undef new

_RPC_BEGIN

class bad_array_new_length : public ::std::exception {
public:
    bad_array_new_length() {} 
    virtual ~bad_array_new_length() {}
    virtual const char* what() const throw() {return "bad array new length";}
};

template <class _Ty>
struct _NODISCARD _Tidy_guard { // class with destructor that calls _Tidy
    _Ty* _Target;
    _CONSTEXPR20 ~_Tidy_guard() {
        if (_Target) {
            _Target->_Tidy();
        }
    }
};

template <class _Ty>
struct _NODISCARD _Tidy_deallocate_guard { // class with destructor that calls _Tidy_deallocate
    _Ty* _Target;
    _CONSTEXPR20 ~_Tidy_deallocate_guard() {
        if (_Target) {
            _Target->_Tidy_deallocate();
        }
    }
};

template <class _Keycmp, class _Lhs, class _Rhs>
_INLINE_VAR constexpr bool _Nothrow_compare = noexcept(
    static_cast<bool>(_STD declval<const _Keycmp&>()(_STD declval<const _Lhs&>(), _STD declval<const _Rhs&>())));

template <size_t _Ty_size>
_NODISCARD constexpr size_t _Get_size_of_n(const size_t _Count) {
    constexpr bool _Overflow_is_possible = _Ty_size > 1;

    if constexpr (_Overflow_is_possible) {
        constexpr size_t _Max_possible = static_cast<size_t>(-1) / _Ty_size;
        if (_Count > _Max_possible) {
            throw bad_array_new_length(); // multiply overflow
        }
    }

    return _Count * _Ty_size;
}

template <class _Ty>
_INLINE_VAR constexpr size_t _New_alignof = (_STD max) (alignof(_Ty),
    static_cast<size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__) // TRANSITION, VSO-522105
);

struct _Default_allocate_traits {
    static
#ifdef __clang__ // Clang and MSVC implement P0784R7 differently; see GH-1532
        _CONSTEXPR20
#endif // __clang__
        void* _Allocate(const size_t _Bytes) {
        return ::operator new(_Bytes);
    }

#ifdef __cpp_aligned_new
    static
#ifdef __clang__ // Clang and MSVC implement P0784R7 differently; see GH-1532
        _CONSTEXPR20
#endif // __clang__
        void* _Allocate_aligned(const size_t _Bytes, const size_t _Align) {
#ifdef __clang__ // Clang and MSVC implement P0784R7 differently; see GH-1532
#if _HAS_CXX20
        if (_STD is_constant_evaluated()) {
            return ::operator new(_Bytes);
        } else
#endif // _HAS_CXX20
#endif // __clang__
        {
            return ::operator new (_Bytes, ::std::align_val_t{_Align});
        }
    }
#endif // __cpp_aligned_new
};

constexpr bool _Is_pow_2(const size_t _Value) noexcept {
    return _Value != 0 && (_Value & (_Value - 1)) == 0;
}

#if defined(_M_IX86) || defined(_M_X64)
constexpr size_t _Big_allocation_threshold = 4096;
constexpr size_t _Big_allocation_alignment = 32;

static_assert(2 * sizeof(void*) <= _Big_allocation_alignment,
    "Big allocation alignment should at least match vector register alignment");
static_assert(_Is_pow_2(_Big_allocation_alignment), "Big allocation alignment must be a power of two");

#ifdef _DEBUG
constexpr size_t _Non_user_size = 2 * sizeof(void*) + _Big_allocation_alignment - 1;
#else // _DEBUG
constexpr size_t _Non_user_size           = sizeof(void*) + _Big_allocation_alignment - 1;
#endif // _DEBUG

#ifdef _WIN64
constexpr size_t _Big_allocation_sentinel = 0xFAFAFAFAFAFAFAFAULL;
#else // ^^^ _WIN64 ^^^ // vvv !_WIN64 vvv
constexpr size_t _Big_allocation_sentinel = 0xFAFAFAFAUL;
#endif // _WIN64

template <class _Traits>
void* _Allocate_manually_vector_aligned(const size_t _Bytes) {
    // allocate _Bytes manually aligned to at least _Big_allocation_alignment
    const size_t _Block_size = _Non_user_size + _Bytes;
    if (_Block_size <= _Bytes) {
        throw bad_array_new_length(); // add overflow
    }

    const uintptr_t _Ptr_container = reinterpret_cast<uintptr_t>(_Traits::_Allocate(_Block_size));
    _STL_VERIFY(_Ptr_container != 0, "invalid argument"); // validate even in release since we're doing p[-1]
    void* const _Ptr = reinterpret_cast<void*>((_Ptr_container + _Non_user_size) & ~(_Big_allocation_alignment - 1));
    static_cast<uintptr_t*>(_Ptr)[-1] = _Ptr_container;

#ifdef _DEBUG
    static_cast<uintptr_t*>(_Ptr)[-2] = _Big_allocation_sentinel;
#endif // _DEBUG
    return _Ptr;
}

inline void _Adjust_manually_vector_aligned(void*& _Ptr, size_t& _Bytes) {
    // adjust parameters from _Allocate_manually_vector_aligned to pass to operator delete
    _Bytes += _Non_user_size;

    const uintptr_t* const _Ptr_user = static_cast<uintptr_t*>(_Ptr);
    const uintptr_t _Ptr_container   = _Ptr_user[-1];

    // If the following asserts, it likely means that we are performing
    // an aligned delete on memory coming from an unaligned allocation.
    assert(_Ptr_user[-2] == _Big_allocation_sentinel ? true : !"invalid argument");

    // Extra paranoia on aligned allocation/deallocation; ensure _Ptr_container is
    // in range [_Min_back_shift, _Non_user_size]
#ifdef _DEBUG
    constexpr uintptr_t _Min_back_shift = 2 * sizeof(void*);
#else // ^^^ _DEBUG ^^^ // vvv !_DEBUG vvv
    constexpr uintptr_t _Min_back_shift = sizeof(void*);
#endif // _DEBUG
    const uintptr_t _Back_shift = reinterpret_cast<uintptr_t>(_Ptr) - _Ptr_container;
    _STL_VERIFY(_Back_shift >= _Min_back_shift && _Back_shift <= _Non_user_size, "invalid argument");
    _Ptr = reinterpret_cast<void*>(_Ptr_container);
}
#endif // defined(_M_IX86) || defined(_M_X64)

#ifdef __cpp_aligned_new
template <size_t _Align, class _Traits = _Default_allocate_traits,
    enable_if_t<(_Align > __STDCPP_DEFAULT_NEW_ALIGNMENT__), int> = 0>
_CONSTEXPR20 void* _Allocate(const size_t _Bytes) {
    // allocate _Bytes when __cpp_aligned_new && _Align > __STDCPP_DEFAULT_NEW_ALIGNMENT__
    if (_Bytes == 0) {
        return nullptr;
    }

#if _HAS_CXX20 // TRANSITION, GH-1532
    if (_STD is_constant_evaluated()) {
        return _Traits::_Allocate(_Bytes);
    } else
#endif // _HAS_CXX20
    {
        size_t _Passed_align = _Align;
#if defined(_M_IX86) || defined(_M_X64)
        if (_Bytes >= _Big_allocation_threshold) {
            // boost the alignment of big allocations to help autovectorization
            _Passed_align = (_STD max) (_Align, _Big_allocation_alignment);
        }
#endif // defined(_M_IX86) || defined(_M_X64)
        return _Traits::_Allocate_aligned(_Bytes, _Passed_align);
    }
}


template <size_t _Align, enable_if_t<(_Align > __STDCPP_DEFAULT_NEW_ALIGNMENT__), int> = 0>
_CONSTEXPR20 void _Deallocate(void* _Ptr, const size_t _Bytes) noexcept {
    // deallocate storage allocated by _Allocate when __cpp_aligned_new && _Align > __STDCPP_DEFAULT_NEW_ALIGNMENT__
#if _HAS_CXX20 // TRANSITION, GH-1532
    if (_STD is_constant_evaluated()) {
        ::operator delete(_Ptr);
    } else
#endif // _HAS_CXX20
    {
        size_t _Passed_align = _Align;
#if defined(_M_IX86) || defined(_M_X64)
        if (_Bytes >= _Big_allocation_threshold) { // boost the alignment of big allocations to help autovectorization
            _Passed_align = (_STD max) (_Align, _Big_allocation_alignment);
        }
#endif // defined(_M_IX86) || defined(_M_X64)
///I hope this does not leak
//        ::operator delete (_Ptr, _Bytes, ::std::align_val_t{_Passed_align});
        ::operator delete (_Ptr, ::std::align_val_t{_Passed_align});
    }
}

#define _HAS_ALIGNED_NEW 1
#else // ^^^ __cpp_aligned_new ^^^ / vvv !__cpp_aligned_new vvv
#define _HAS_ALIGNED_NEW 0
#endif // __cpp_aligned_new

template <size_t _Align, class _Traits = _Default_allocate_traits,
    enable_if_t<(!_HAS_ALIGNED_NEW || _Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__), int> = 0>
_CONSTEXPR20 void* _Allocate(const size_t _Bytes) {
    // allocate _Bytes when !_HAS_ALIGNED_NEW || _Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__
#if defined(_M_IX86) || defined(_M_X64)
#if _HAS_CXX20 // TRANSITION, GH-1532
    if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
    {
        if (_Bytes >= _Big_allocation_threshold) { // boost the alignment of big allocations to help autovectorization
            return _Allocate_manually_vector_aligned<_Traits>(_Bytes);
        }
    }
#endif // defined(_M_IX86) || defined(_M_X64)

    if (_Bytes != 0) {
        return _Traits::_Allocate(_Bytes);
    }

    return nullptr;
}

template <size_t _Align, enable_if_t<(!_HAS_ALIGNED_NEW || _Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__), int> = 0>
_CONSTEXPR20 void _Deallocate(void* _Ptr, size_t _Bytes) noexcept {
    // deallocate storage allocated by _Allocate when !_HAS_ALIGNED_NEW || _Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__
#if _HAS_CXX20 // TRANSITION, GH-1532
    if (_STD is_constant_evaluated()) {
        ::operator delete(_Ptr);
    } else
#endif // _HAS_CXX20
    {
#if defined(_M_IX86) || defined(_M_X64)
        if (_Bytes >= _Big_allocation_threshold) { // boost the alignment of big allocations to help autovectorization
            _Adjust_manually_vector_aligned(_Ptr, _Bytes);
        }
#endif // defined(_M_IX86) || defined(_M_X64)
///I hope this does not leak
//        ::operator delete(_Ptr, _Bytes);
        ::operator delete(_Ptr);
    }
}

#undef _HAS_ALIGNED_NEW

template <class _Ty, class... _Types>
_Ty* _Global_new(_Types&&... _Args) { // acts as "new" while disallowing user overload selection
    struct _NODISCARD _Guard_type {
        void* _Result;
        ~_Guard_type() {
            if (_Result) {
                _Deallocate<_New_alignof<_Ty>>(_Result, sizeof(_Ty));
            }
        }
    };

    _Guard_type _Guard{_Allocate<_New_alignof<_Ty>>(sizeof(_Ty))};
    ::new (_Guard._Result) _Ty(_STD forward<_Types>(_Args)...);
    return static_cast<_Ty*>(_STD exchange(_Guard._Result, nullptr));
}

template <class _Ptr, class _Ty>
using _Rebind_pointer_t = typename pointer_traits<_Ptr>::template rebind<_Ty>;

template <class _Pointer, enable_if_t<!::std::is_pointer_v<_Pointer>, int> = 0>
_CONSTEXPR20 _Pointer _Refancy(typename pointer_traits<_Pointer>::element_type* _Ptr) noexcept {
    return pointer_traits<_Pointer>::pointer_to(*_Ptr);
}

template <class _Pointer, enable_if_t<::std::is_pointer_v<_Pointer>, int> = 0>
_CONSTEXPR20 _Pointer _Refancy(_Pointer _Ptr) noexcept {
    return _Ptr;
}

template <class _NoThrowFwdIt, class _NoThrowSentinel>
_CONSTEXPR20 void _Destroy_range(_NoThrowFwdIt _First, _NoThrowSentinel _Last) noexcept;

template <class _Ty>
_CONSTEXPR20 void _Destroy_in_place(_Ty& _Obj) noexcept {
    if constexpr (::std::is_array_v<_Ty>) {
        _Destroy_range(_Obj, _Obj + ::std::extent_v<_Ty>);
    } else {
        _Obj.~_Ty();
    }
}

#if _HAS_CXX17
template <class _Ty>
_CONSTEXPR20 void destroy_at(_Ty* const _Location) noexcept /* strengthened */ {
#if _HAS_CXX20
    if constexpr (::std::is_array_v<_Ty>) {
        _Destroy_range(_STD begin(*_Location), _STD end(*_Location));
    } else
#endif // _HAS_CXX20
    {
        _Location->~_Ty();
    }
}
#endif // _HAS_CXX17

template <class _Ptrty>
auto _Const_cast(_Ptrty _Ptr) noexcept { // remove constness from a fancy pointer
    using _Elem       = typename pointer_traits<_Ptrty>::element_type;
    using _Modifiable = remove_const_t<_Elem>;
    using _Dest       = typename pointer_traits<_Ptrty>::template rebind<_Modifiable>;

    return pointer_traits<_Dest>::pointer_to(const_cast<_Modifiable&>(*_Ptr));
}

template <class _Ty>
auto _Const_cast(_Ty* _Ptr) noexcept {
    return const_cast<remove_const_t<_Ty>*>(_Ptr);
}

template <class _Ty, class = void>
struct _Get_pointer_type {
    using type = typename _Ty::value_type*;
};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Ty>
struct _Get_pointer_type<_Ty, ::std::void_t<typename _Ty::pointer>> {
    using type = typename _Ty::pointer;
};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Ty, class = void>
struct _Get_const_pointer_type {
    using _Ptrty = typename _Get_pointer_type<_Ty>::type;
    using _Valty = typename _Ty::value_type;
    using type   = typename pointer_traits<_Ptrty>::template rebind<const _Valty>;
};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Ty>
struct _Get_const_pointer_type<_Ty, ::std::void_t<typename _Ty::const_pointer>> {
    using type = typename _Ty::const_pointer;
};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Ty, class = void>
struct _Get_void_pointer_type {
    using _Ptrty = typename _Get_pointer_type<_Ty>::type;
    using type   = typename pointer_traits<_Ptrty>::template rebind<void>;
};

template <class _Ty>
struct _Get_void_pointer_type<_Ty, ::std::void_t<typename _Ty::void_pointer>> {
    using type = typename _Ty::void_pointer;
};

template <class _Ty, class = void>
struct _Get_const_void_pointer_type {
    using _Ptrty = typename _Get_pointer_type<_Ty>::type;
    using type   = typename pointer_traits<_Ptrty>::template rebind<const void>;
};

template <class _Ty>
struct _Get_const_void_pointer_type<_Ty, ::std::void_t<typename _Ty::const_void_pointer>> {
    using type = typename _Ty::const_void_pointer;
};

template <class _Ty, class = void>
struct _Get_difference_type {
    using _Ptrty = typename _Get_pointer_type<_Ty>::type;
    using type   = typename pointer_traits<_Ptrty>::difference_type;
};

template <class _Ty>
struct _Get_difference_type<_Ty, ::std::void_t<typename _Ty::difference_type>> {
    using type = typename _Ty::difference_type;
};

template <class _Ty, class = void>
struct _Get_size_type {
    using type = ::std::make_unsigned_t<typename _Get_difference_type<_Ty>::type>;
};

template <class _Ty>
struct _Get_size_type<_Ty, ::std::void_t<typename _Ty::size_type>> {
    using type = typename _Ty::size_type;
};

template <class _Ty, class = void>
struct _Get_propagate_on_container_copy {
    using type = false_type;
};

template <class _Ty>
struct _Get_propagate_on_container_copy<_Ty, ::std::void_t<typename _Ty::propagate_on_container_copy_assignment>> {
    using type = typename _Ty::propagate_on_container_copy_assignment;
};

template <class _Ty, class = void>
struct _Get_propagate_on_container_move {
    using type = false_type;
};

template <class _Ty>
struct _Get_propagate_on_container_move<_Ty, ::std::void_t<typename _Ty::propagate_on_container_move_assignment>> {
    using type = typename _Ty::propagate_on_container_move_assignment;
};

template <class _Ty, class = void>
struct _Get_propagate_on_container_swap {
    using type = false_type;
};

template <class _Ty>
struct _Get_propagate_on_container_swap<_Ty, ::std::void_t<typename _Ty::propagate_on_container_swap>> {
    using type = typename _Ty::propagate_on_container_swap;
};

template <class _Ty, class = void>
struct _Get_is_always_equal {
    using type = bool_constant<::std::is_empty_v<_Ty>>;
};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Ty>
struct _Get_is_always_equal<_Ty, ::std::void_t<typename _Ty::is_always_equal>> {
    using type = typename _Ty::is_always_equal;
};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Ty, class _Other, class = void>
struct _Get_rebind_type {
    using type = typename _Replace_first_parameter<_Other, _Ty>::type;
};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Ty, class _Other>
struct _Get_rebind_type<_Ty, _Other, ::std::void_t<typename _Ty::template rebind<_Other>::other>> {
    using type = typename _Ty::template rebind<_Other>::other;
};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Ty>
class allocator;

template <class _Alloc, class = void>
struct _Is_default_allocator : false_type {};

template <class _Ty>
struct _Is_default_allocator<allocator<_Ty>, ::std::void_t<typename allocator<_Ty>::_From_primary>>
    : is_same<typename allocator<_Ty>::_From_primary, allocator<_Ty>>::type {};

template <class _Void, class... _Types>
struct _Has_no_allocator_construct : true_type {};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Alloc, class _Ptr, class... _Args>
struct _Has_no_allocator_construct<
    ::std::void_t<decltype(_STD declval<_Alloc&>().construct(_STD declval<_Ptr>(), _STD declval<_Args>()...))>, _Alloc, _Ptr,
    _Args...> : false_type {};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Alloc, class _Ptr, class... _Args>
using _Uses_default_construct =
    disjunction<_Is_default_allocator<_Alloc>, _Has_no_allocator_construct<void, _Alloc, _Ptr, _Args...>>;

template <class _Alloc, class _Ptr, class = void>
struct _Has_no_alloc_destroy : true_type {};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Alloc, class _Ptr>
struct _Has_no_alloc_destroy<_Alloc, _Ptr, ::std::void_t<decltype(_STD declval<_Alloc&>().destroy(_STD declval<_Ptr>()))>>
    : false_type {};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Alloc, class _Ptr>
using _Uses_default_destroy = disjunction<_Is_default_allocator<_Alloc>, _Has_no_alloc_destroy<_Alloc, _Ptr>>;

template <class _Alloc, class _Ptr>
using _Uses_default_destroy_t = typename _Uses_default_destroy<_Alloc, _Ptr>::type;

template <class _Alloc, class _Size_type, class _Const_void_pointer, class = void>
struct _Has_allocate_hint : false_type {};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Alloc, class _Size_type, class _Const_void_pointer>
struct _Has_allocate_hint<_Alloc, _Size_type, _Const_void_pointer,
    ::std::void_t<decltype(_STD declval<_Alloc&>().allocate(
        _STD declval<const _Size_type&>(), _STD declval<const _Const_void_pointer&>()))>> : true_type {};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Alloc, class = void>
struct _Has_max_size : false_type {};

_STL_DISABLE_DEPRECATED_WARNING
template <class _Alloc>
struct _Has_max_size<_Alloc, ::std::void_t<decltype(_STD declval<const _Alloc&>().max_size())>> : true_type {};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Alloc, class = void>
struct _Has_select_on_container_copy_construction : false_type {};

template <class _Alloc>
struct _Has_select_on_container_copy_construction<_Alloc,
    ::std::void_t<decltype(_STD declval<const _Alloc&>().select_on_container_copy_construction())>> : true_type {};

template <class _Alloc>
struct allocator_traits;

_STL_DISABLE_DEPRECATED_WARNING
template <class _Alloc>
struct _Normal_allocator_traits { // defines traits for allocators
    using allocator_type = _Alloc;
    using value_type     = typename _Alloc::value_type;

    using pointer            = typename _Get_pointer_type<_Alloc>::type;
    using const_pointer      = typename _Get_const_pointer_type<_Alloc>::type;
    using void_pointer       = typename _Get_void_pointer_type<_Alloc>::type;
    using const_void_pointer = typename _Get_const_void_pointer_type<_Alloc>::type;

    using size_type       = typename _Get_size_type<_Alloc>::type;
    using difference_type = typename _Get_difference_type<_Alloc>::type;

    using propagate_on_container_copy_assignment = typename _Get_propagate_on_container_copy<_Alloc>::type;
    using propagate_on_container_move_assignment = typename _Get_propagate_on_container_move<_Alloc>::type;
    using propagate_on_container_swap            = typename _Get_propagate_on_container_swap<_Alloc>::type;
    using is_always_equal                        = typename _Get_is_always_equal<_Alloc>::type;

    template <class _Other>
    using rebind_alloc = typename _Get_rebind_type<_Alloc, _Other>::type;

    template <class _Other>
    using rebind_traits = allocator_traits<rebind_alloc<_Other>>;

    _NODISCARD static _CONSTEXPR20 pointer
        allocate(_Alloc& _Al, _CRT_GUARDOVERFLOW const size_type _Count) {
        return _Al.allocate(_Count);
    }

    _NODISCARD static _CONSTEXPR20 pointer
        allocate(_Alloc& _Al, _CRT_GUARDOVERFLOW const size_type _Count, const const_void_pointer _Hint) {
        if constexpr (_Has_allocate_hint<_Alloc, size_type, const_void_pointer>::value) {
            return _Al.allocate(_Count, _Hint);
        } else {
            return _Al.allocate(_Count);
        }
    }

    static _CONSTEXPR20 void deallocate(_Alloc& _Al, pointer _Ptr, size_type _Count) {
        _Al.deallocate(_Ptr, _Count);
    }

    template <class _Ty, class... _Types>
    static _CONSTEXPR20 void construct(_Alloc& _Al, _Ty* _Ptr, _Types&&... _Args) {
        if constexpr (_Uses_default_construct<_Alloc, _Ty*, _Types...>::value) {
#if _HAS_CXX20
            _STD construct_at(_Ptr, _STD forward<_Types>(_Args)...);
#else // _HAS_CXX20
            ::new (static_cast<void*>(_Ptr)) _Ty(_STD forward<_Types>(_Args)...);
#endif // _HAS_CXX20
        } else {
            _Al.construct(_Ptr, _STD forward<_Types>(_Args)...);
        }
    }

    template <class _Ty>
    static _CONSTEXPR20 void destroy(_Alloc& _Al, _Ty* _Ptr) {
        if constexpr (_Uses_default_destroy<_Alloc, _Ty*>::value) {
#if _HAS_CXX20
            _STD destroy_at(_Ptr);
#else // _HAS_CXX20
            _Ptr->~_Ty();
#endif // _HAS_CXX20
        } else {
            _Al.destroy(_Ptr);
        }
    }

    _NODISCARD static _CONSTEXPR20 size_type max_size(const _Alloc& _Al) noexcept {
        if constexpr (_Has_max_size<_Alloc>::value) {
            return _Al.max_size();
        } else {
            return (::std::numeric_limits<size_type>::max) () / sizeof(value_type);
        }
    }

    _NODISCARD static _CONSTEXPR20 _Alloc select_on_container_copy_construction(const _Alloc& _Al) {
        if constexpr (_Has_select_on_container_copy_construction<_Alloc>::value) {
            return _Al.select_on_container_copy_construction();
        } else {
            return _Al;
        }
    }
};
_STL_RESTORE_DEPRECATED_WARNING

template <class _Alloc>
struct _Default_allocator_traits { // traits for std::allocator
    using allocator_type = _Alloc;
    using value_type     = typename _Alloc::value_type;

    using pointer            = value_type*;
    using const_pointer      = const value_type*;
    using void_pointer       = void*;
    using const_void_pointer = const void*;

    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    using propagate_on_container_copy_assignment = false_type;
    using propagate_on_container_move_assignment = true_type;
    using propagate_on_container_swap            = false_type;
    using is_always_equal                        = true_type;

    template <class _Other>
    using rebind_alloc = allocator<_Other>;

    template <class _Other>
    using rebind_traits = allocator_traits<allocator<_Other>>;

    _NODISCARD static _CONSTEXPR20 pointer
        allocate(_Alloc& _Al, _CRT_GUARDOVERFLOW const size_type _Count) {
#if _HAS_CXX20 // TRANSITION, GH-1532
        if (_STD is_constant_evaluated()) {
            return _Al.allocate(_Count);
        } else
#endif // _HAS_CXX20
        {
            (void) _Al;
            return static_cast<pointer>(
                _Allocate<_New_alignof<value_type>>(_Get_size_of_n<sizeof(value_type)>(_Count)));
        }
    }

    _NODISCARD static _CONSTEXPR20 pointer
        allocate(_Alloc& _Al, _CRT_GUARDOVERFLOW const size_type _Count, const_void_pointer) {
#if _HAS_CXX20 // TRANSITION, GH-1532
        if (_STD is_constant_evaluated()) {
            return _Al.allocate(_Count);
        } else
#endif // _HAS_CXX20
        {
            (void) _Al;
            return static_cast<pointer>(
                _Allocate<_New_alignof<value_type>>(_Get_size_of_n<sizeof(value_type)>(_Count)));
        }
    }

    static _CONSTEXPR20 void deallocate(_Alloc& _Al, const pointer _Ptr, const size_type _Count) {
        // no overflow check on the following multiply; we assume _Allocate did that check
#if _HAS_CXX20 // TRANSITION, GH-1532
        if (_STD is_constant_evaluated()) {
            _Al.deallocate(_Ptr, _Count);
        } else
#endif // _HAS_CXX20
        {
            (void) _Al;
            _Deallocate<_New_alignof<value_type>>(_Ptr, sizeof(value_type) * _Count);
        }
    }

    template <class _Objty, class... _Types>
    static _CONSTEXPR20 void construct(_Alloc&, _Objty* const _Ptr, _Types&&... _Args) {
#if _HAS_CXX20
        if (_STD is_constant_evaluated()) {
            _STD construct_at(_Ptr, _STD forward<_Types>(_Args)...);
        } else
#endif // _HAS_CXX20
        {
            ::new (_Voidify_iter(_Ptr)) _Objty(_STD forward<_Types>(_Args)...);
        }
    }

    template <class _Uty>
    static _CONSTEXPR20 void destroy(_Alloc&, _Uty* const _Ptr) {
#if _HAS_CXX20
        _STD destroy_at(_Ptr);
#else // _HAS_CXX20
        _Ptr->~_Uty();
#endif // _HAS_CXX20
    }

    _NODISCARD static _CONSTEXPR20 size_type max_size(const _Alloc&) noexcept {
        return static_cast<size_t>(-1) / sizeof(value_type);
    }

    _NODISCARD static _CONSTEXPR20 _Alloc select_on_container_copy_construction(const _Alloc& _Al) {
        return _Al;
    }
};

template <class _Alloc>
struct allocator_traits : conditional_t<_Is_default_allocator<_Alloc>::value, _Default_allocator_traits<_Alloc>,
                              _Normal_allocator_traits<_Alloc>> {};

// _Choose_pocca_v returns whether an attempt to propagate allocators is necessary in copy assignment operations.
// Note that even when false_type, callers should call _Pocca as we want to assign allocators even when equal.
template <class _Alloc>
_INLINE_VAR constexpr bool _Choose_pocca_v = allocator_traits<_Alloc>::propagate_on_container_copy_assignment::value
                                          && !allocator_traits<_Alloc>::is_always_equal::value;

enum class _Pocma_values {
    _Equal_allocators, // usually allows contents to be stolen (e.g. with swap)
    _Propagate_allocators, // usually allows the allocator to be propagated, and then contents stolen
    _No_propagate_allocators, // usually turns moves into copies
};

template <class _Alloc>
_INLINE_VAR constexpr _Pocma_values
    _Choose_pocma_v = allocator_traits<_Alloc>::is_always_equal::value
                        ? _Pocma_values::_Equal_allocators
                        : (allocator_traits<_Alloc>::propagate_on_container_move_assignment::value
                                ? _Pocma_values::_Propagate_allocators
                                : _Pocma_values::_No_propagate_allocators);

template <class _Alloc, class _Value_type>
using _Rebind_alloc_t = typename allocator_traits<_Alloc>::template rebind_alloc<_Value_type>;

// If _Alloc is already rebound appropriately, binds an lvalue reference to it, avoiding a copy. Otherwise, creates a
// rebound copy.
template <class _Alloc, class _Value_type>
using _Maybe_rebind_alloc_t =
    typename _Select<::std::is_same_v<typename _Alloc::value_type, _Value_type>>::template _Apply<_Alloc&,
        _Rebind_alloc_t<_Alloc, _Value_type>>;

template <class _Alloc> // tests if allocator has simple addressing
_INLINE_VAR constexpr bool _Is_simple_alloc_v = ::std::is_same_v<typename allocator_traits<_Alloc>::size_type, size_t>&&
    ::std::is_same_v<typename allocator_traits<_Alloc>::difference_type, ptrdiff_t>&&
        ::std::is_same_v<typename allocator_traits<_Alloc>::pointer, typename _Alloc::value_type*>&&
            ::std::is_same_v<typename allocator_traits<_Alloc>::const_pointer, const typename _Alloc::value_type*>;

template <class _Value_type>
struct _Simple_types { // wraps types from allocators with simple addressing for use in iterators
                       // and other SCARY machinery
    using value_type      = _Value_type;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
};

#if _HAS_CXX23
template <class _Ptr>
struct allocation_result {
    _Ptr ptr;
    size_t count;
};

#ifdef __cpp_lib_concepts
template <class _Alloc>
concept _Has_member_allocate_at_least = requires(_Alloc _Al, size_t _Count) {
    _Al.allocate_at_least(_Count);
};

template <class _Alloc>
_NODISCARD constexpr allocation_result<typename allocator_traits<_Alloc>::pointer> allocate_at_least(
    _Alloc& _Al, const size_t _Count) {
    if constexpr (_Has_member_allocate_at_least<_Alloc>) {
        return _Al.allocate_at_least(_Count);
    } else {
        return {_Al.allocate(_Count), _Count};
    }
}
#endif // __cpp_lib_concepts
#endif // _HAS_CXX23

// The number of user bytes a single byte of ASAN shadow memory can track.
_INLINE_VAR constexpr size_t _Asan_granularity = 8;

template <class _Ty>
class allocator {
public:
    static_assert(!::std::is_const_v<_Ty>, "The C++ Standard forbids containers of const elements "
                                    "because allocator<const T> is ill-formed.");

    using _From_primary = allocator;

    using value_type = _Ty;

#if _HAS_DEPRECATED_ALLOCATOR_MEMBERS
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS typedef _Ty* pointer;
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS typedef const _Ty* const_pointer;

    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS typedef _Ty& reference;
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS typedef const _Ty& const_reference;
#endif // _HAS_DEPRECATED_ALLOCATOR_MEMBERS

    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    using propagate_on_container_move_assignment           = true_type;
    using is_always_equal _CXX20_DEPRECATE_IS_ALWAYS_EQUAL = true_type;

#if _HAS_DEPRECATED_ALLOCATOR_MEMBERS
    template <class _Other>
    struct _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS rebind {
        using other = allocator<_Other>;
    };

    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS _NODISCARD _Ty* address(_Ty& _Val) const noexcept {
        return _STD addressof(_Val);
    }

    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS _NODISCARD const _Ty* address(const _Ty& _Val) const noexcept {
        return _STD addressof(_Val);
    }
#endif // _HAS_DEPRECATED_ALLOCATOR_MEMBERS

    constexpr allocator() noexcept {}

    constexpr allocator(const allocator&) noexcept = default;
    template <class _Other>
    constexpr allocator(const allocator<_Other>&) noexcept {}
    _CONSTEXPR20 ~allocator()       = default;
    _CONSTEXPR20 allocator& operator=(const allocator&) = default;

    _CONSTEXPR20 void deallocate(_Ty* const _Ptr, const size_t _Count) {
        _STL_ASSERT(_Ptr != nullptr || _Count == 0, "null pointer cannot point to a block of non-zero size");
        // no overflow check on the following multiply; we assume _Allocate did that check
        _Deallocate<_New_alignof<_Ty>>(_Ptr, sizeof(_Ty) * _Count);
    }

    _NODISCARD _CONSTEXPR20 _Ty* allocate(_CRT_GUARDOVERFLOW const size_t _Count) {
        return static_cast<_Ty*>(_Allocate<_New_alignof<_Ty>>(_Get_size_of_n<sizeof(_Ty)>(_Count)));
    }

#if _HAS_CXX23
    _NODISCARD constexpr allocation_result<_Ty*> allocate_at_least(_CRT_GUARDOVERFLOW const size_t _Count) {
        return {allocate(_Count), _Count};
    }
#endif // _HAS_CXX23

#if _HAS_DEPRECATED_ALLOCATOR_MEMBERS
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS _NODISCARD _Ty* allocate(
        _CRT_GUARDOVERFLOW const size_t _Count, const void*) {
        return allocate(_Count);
    }

    template <class _Objty, class... _Types>
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS void construct(_Objty* const _Ptr, _Types&&... _Args) {
        ::new (_Voidify_iter(_Ptr)) _Objty(_STD forward<_Types>(_Args)...);
    }

    template <class _Uty>
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS void destroy(_Uty* const _Ptr) {
        _Ptr->~_Uty();
    }

    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS _NODISCARD size_t max_size() const noexcept {
        return static_cast<size_t>(-1) / sizeof(_Ty);
    }
#endif // _HAS_DEPRECATED_ALLOCATOR_MEMBERS

    static constexpr size_t _Minimum_allocation_alignment = _Asan_granularity;
};

template <>
class allocator<void> {
public:
    using value_type = void;
#if _HAS_DEPRECATED_ALLOCATOR_MEMBERS
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS typedef void* pointer;
    _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS typedef const void* const_pointer;
#endif // _HAS_DEPRECATED_ALLOCATOR_MEMBERS

    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    using propagate_on_container_move_assignment           = true_type;
    using is_always_equal _CXX20_DEPRECATE_IS_ALWAYS_EQUAL = true_type;

#if _HAS_DEPRECATED_ALLOCATOR_MEMBERS
    template <class _Other>
    struct _CXX17_DEPRECATE_OLD_ALLOCATOR_MEMBERS rebind {
        using other = allocator<_Other>;
    };
#endif // _HAS_DEPRECATED_ALLOCATOR_MEMBERS
};

template <class _Ty, class _Other>
_NODISCARD _CONSTEXPR20 bool operator==(const allocator<_Ty>&, const allocator<_Other>&) noexcept {
    return true;
}

#if !_HAS_CXX20
template <class _Ty, class _Other>
_NODISCARD bool operator!=(const allocator<_Ty>&, const allocator<_Other>&) noexcept {
    return false;
}
#endif // !_HAS_CXX20

#if _HAS_CXX17
// See N4892 [unord.map.overview]/4
template <class _Alloc>
using _Guide_size_type_t =
    typename allocator_traits<conditional_t<_Is_allocator<_Alloc>::value, _Alloc, allocator<int>>>::size_type;
#endif // _HAS_CXX17

template <class _Alloc>
using _Alloc_ptr_t = typename allocator_traits<_Alloc>::pointer;

template <class _Alloc>
using _Alloc_size_t = typename allocator_traits<_Alloc>::size_type;

template <class _Alloc>
_CONSTEXPR20 void _Pocca(_Alloc& _Left, const _Alloc& _Right) noexcept {
    if constexpr (allocator_traits<_Alloc>::propagate_on_container_copy_assignment::value) {
        _Left = _Right;
    }
}

template <class _Alloc>
_CONSTEXPR20 void _Pocma(_Alloc& _Left, _Alloc& _Right) noexcept { // (maybe) propagate on container move assignment
    if constexpr (allocator_traits<_Alloc>::propagate_on_container_move_assignment::value) {
        _Left = _STD move(_Right);
    }
}

template <class _Alloc>
_CONSTEXPR20 void _Pocs(_Alloc& _Left, _Alloc& _Right) noexcept {
    if constexpr (allocator_traits<_Alloc>::propagate_on_container_swap::value) {
        _Swap_adl(_Left, _Right);
    } else {
        _STL_ASSERT(_Left == _Right, "containers incompatible for swap");
    }
}

template <class _Alloc>
_CONSTEXPR20 void _Destroy_range(_Alloc_ptr_t<_Alloc> _First, const _Alloc_ptr_t<_Alloc> _Last, _Alloc& _Al) noexcept {
    // note that this is an optimization for debug mode codegen; in release mode the BE removes all of this
    using _Ty = typename _Alloc::value_type;
    if constexpr (!::std::conjunction_v<::std::is_trivially_destructible<_Ty>, _Uses_default_destroy<_Alloc, _Ty*>>) {
        for (; _First != _Last; ++_First) {
            allocator_traits<_Alloc>::destroy(_Al, _Unfancy(_First));
        }
    }
}

template <class _NoThrowFwdIt, class _NoThrowSentinel>
_CONSTEXPR20 void _Destroy_range(_NoThrowFwdIt _First, const _NoThrowSentinel _Last) noexcept {
    // note that this is an optimization for debug mode codegen; in release mode the BE removes all of this
    if constexpr (!::std::is_trivially_destructible_v<_Iter_value_t<_NoThrowFwdIt>>) {
        for (; _First != _Last; ++_First) {
            _Destroy_in_place(*_First);
        }
    }
}

template <class _Size_type>
_NODISCARD constexpr _Size_type _Convert_size(const size_t _Len) noexcept(::std::is_same_v<_Size_type, size_t>) {
    // convert size_t to _Size_type, avoiding truncation
    if constexpr (!::std::is_same_v<_Size_type, size_t>) {
        if (_Len > (::std::numeric_limits<_Size_type>::max) ()) {
            throw ::std::length_error("size_t too long for _Size_type");
        }
    }

    return static_cast<_Size_type>(_Len);
}

template <class _Alloc>
_CONSTEXPR20 void _Deallocate_plain(_Alloc& _Al, typename _Alloc::value_type* const _Ptr) noexcept {
    // deallocate a plain pointer using an allocator
    using _Alloc_traits = allocator_traits<_Alloc>;
    if constexpr (::std::is_same_v<_Alloc_ptr_t<_Alloc>, typename _Alloc::value_type*>) {
        _Alloc_traits::deallocate(_Al, _Ptr, 1);
    } else {
        using _Ptr_traits = pointer_traits<_Alloc_ptr_t<_Alloc>>;
        _Alloc_traits::deallocate(_Al, _Ptr_traits::pointer_to(*_Ptr), 1);
    }
}

template <class _Alloc>
_CONSTEXPR20 void _Delete_plain_internal(_Alloc& _Al, typename _Alloc::value_type* const _Ptr) noexcept {
    // destroy *_Ptr in place, then deallocate _Ptr using _Al; used for internal container types the user didn't name
    using _Ty = typename _Alloc::value_type;
    _Ptr->~_Ty();
    _Deallocate_plain(_Al, _Ptr);
}

template <class _Alloc>
struct _Alloc_construct_ptr { // pointer used to help construct 1 _Alloc::value_type without EH
    using pointer = _Alloc_ptr_t<_Alloc>;
    _Alloc& _Al;
    pointer _Ptr;

    _CONSTEXPR20 explicit _Alloc_construct_ptr(_Alloc& _Al_) : _Al(_Al_), _Ptr(nullptr) {}

    _NODISCARD _CONSTEXPR20 pointer _Release() noexcept { // disengage *this and return contained pointer
        return _STD exchange(_Ptr, nullptr);
    }

    _CONSTEXPR20 void _Allocate() { // disengage *this, then allocate a new memory block
        _Ptr = nullptr; // if allocate throws, prevents double-free
        _Ptr = _Al.allocate(1);
    }

    _CONSTEXPR20 ~_Alloc_construct_ptr() { // if this instance is engaged, deallocate storage
        if (_Ptr) {
            _Al.deallocate(_Ptr, 1);
        }
    }

    _Alloc_construct_ptr(const _Alloc_construct_ptr&) = delete;
    _Alloc_construct_ptr& operator=(const _Alloc_construct_ptr&) = delete;
};

struct _Fake_allocator {};

struct _Container_base0 {
    _CONSTEXPR20 void _Orphan_all() noexcept {}
    _CONSTEXPR20 void _Swap_proxy_and_iterators(_Container_base0&) noexcept {}
    _CONSTEXPR20 void _Alloc_proxy(const _Fake_allocator&) noexcept {}
    _CONSTEXPR20 void _Reload_proxy(const _Fake_allocator&, const _Fake_allocator&) noexcept {}
};

struct _Iterator_base0 {
    _CONSTEXPR20 void _Adopt(const void*) noexcept {}
    _CONSTEXPR20 const _Container_base0* _Getcont() const noexcept {
        return nullptr;
    }

    static constexpr bool _Unwrap_when_unverified = true;
};

struct _Container_base12;
struct _Container_proxy { // store head of iterator chain and back pointer
    _CONSTEXPR20 _Container_proxy() noexcept = default;
    _CONSTEXPR20 _Container_proxy(_Container_base12* _Mycont_) noexcept : _Mycont(_Mycont_) {}

    const _Container_base12* _Mycont       = nullptr;
    mutable _Iterator_base12* _Myfirstiter = nullptr;
};

struct _Container_base12 {
public:
    _CONSTEXPR20 _Container_base12() noexcept = default;

    _Container_base12(const _Container_base12&) = delete;
    _Container_base12& operator=(const _Container_base12&) = delete;

    _CONSTEXPR20 void _Orphan_all() noexcept;
    _CONSTEXPR20 void _Swap_proxy_and_iterators(_Container_base12&) noexcept;

    template <class _Alloc>
    _CONSTEXPR20 void _Alloc_proxy(_Alloc&& _Al) {
        _Container_proxy* const _New_proxy = _Unfancy(_Al.allocate(1));
        _Construct_in_place(*_New_proxy, this);
        _Myproxy            = _New_proxy;
        _New_proxy->_Mycont = this;
    }

    template <class _Alloc>
    _CONSTEXPR20 void _Reload_proxy(_Alloc&& _Old_alloc, _Alloc&& _New_alloc) {
        // pre: no iterators refer to the existing proxy
        _Container_proxy* const _New_proxy = _Unfancy(_New_alloc.allocate(1));
        _Construct_in_place(*_New_proxy, this);
        _New_proxy->_Mycont = this;
        _Delete_plain_internal(_Old_alloc, _STD exchange(_Myproxy, _New_proxy));
    }

    _Container_proxy* _Myproxy = nullptr;

private:
    _CONSTEXPR20 void _Orphan_all_unlocked_v3() noexcept;
    _CONSTEXPR20 void _Swap_proxy_and_iterators_unlocked(_Container_base12&) noexcept;

    void _Orphan_all_locked_v3() noexcept {
        _Lockit _Lock(_LOCK_DEBUG);
        _Orphan_all_unlocked_v3();
    }

    void _Swap_proxy_and_iterators_locked(_Container_base12& _Right) noexcept {
        _Lockit _Lock(_LOCK_DEBUG);
        _Swap_proxy_and_iterators_unlocked(_Right);
    }
};

struct _Iterator_base12 { // store links to container proxy, next iterator
public:
    _CONSTEXPR20 _Iterator_base12() noexcept = default; // construct orphaned iterator

    _CONSTEXPR20 _Iterator_base12(const _Iterator_base12& _Right) noexcept {
        *this = _Right;
    }

    _CONSTEXPR20 _Iterator_base12& operator=(const _Iterator_base12& _Right) noexcept {
#if _ITERATOR_DEBUG_LEVEL == 2
#if _HAS_CXX20
        if (_STD is_constant_evaluated()) {
            _Assign_unlocked(_Right);
        } else
#endif // _HAS_CXX20
        {
            _Assign_locked(_Right);
        }
#else // ^^^ _ITERATOR_DEBUG_LEVEL == 2 ^^^ / vvv _ITERATOR_DEBUG_LEVEL != 2 vvv
        _Myproxy = _Right._Myproxy;
#endif // _ITERATOR_DEBUG_LEVEL != 2
        return *this;
    }

#if _ITERATOR_DEBUG_LEVEL == 2
    _CONSTEXPR20 ~_Iterator_base12() noexcept {
#if _HAS_CXX20
        if (_STD is_constant_evaluated()) {
            _Orphan_me_unlocked_v3();
        } else
#endif // _HAS_CXX20
        {
            _Orphan_me_locked_v3();
        }
    }

    _CONSTEXPR20 void _Adopt(const _Container_base12* _Parent) noexcept {
#if _HAS_CXX20
        if (_STD is_constant_evaluated()) {
            _Adopt_unlocked(_Parent);
        } else
#endif // _HAS_CXX20
        {
            _Adopt_locked(_Parent);
        }
    }
#else // ^^^ _ITERATOR_DEBUG_LEVEL == 2 ^^^ / vvv _ITERATOR_DEBUG_LEVEL != 2 vvv
    _CONSTEXPR20 void _Adopt(const _Container_base12* _Parent) noexcept {
        if (_Parent) { // have a parent, do adoption
            _Myproxy = _Parent->_Myproxy;
        } else { // no future parent, just disown current parent
            _Myproxy = nullptr;
        }
    }
#endif // _ITERATOR_DEBUG_LEVEL != 2

    _CONSTEXPR20 const _Container_base12* _Getcont() const noexcept {
        return _Myproxy ? _Myproxy->_Mycont : nullptr;
    }

    static constexpr bool _Unwrap_when_unverified = _ITERATOR_DEBUG_LEVEL == 0;

    mutable _Container_proxy* _Myproxy    = nullptr;
    mutable _Iterator_base12* _Mynextiter = nullptr;

#if _ITERATOR_DEBUG_LEVEL == 2
private:
    _CONSTEXPR20 void _Assign_unlocked(const _Iterator_base12& _Right) noexcept {
        if (_Myproxy == _Right._Myproxy) {
            return;
        }

        if (_Right._Myproxy) {
            _Adopt_unlocked(_Right._Myproxy->_Mycont);
        } else { // becoming invalid, disown current parent
            _Orphan_me_unlocked_v3();
        }
    }

    void _Assign_locked(const _Iterator_base12& _Right) noexcept {
        _Lockit _Lock(_LOCK_DEBUG);
        _Assign_unlocked(_Right);
    }

    _CONSTEXPR20 void _Adopt_unlocked(const _Container_base12* _Parent) noexcept {
        if (!_Parent) {
            _Orphan_me_unlocked_v3();
            return;
        }

        _Container_proxy* _Parent_proxy = _Parent->_Myproxy;
        if (_Myproxy != _Parent_proxy) { // change parentage
            if (_Myproxy) { // adopted, remove self from list
                _Orphan_me_unlocked_v3();
            }
            _Mynextiter                 = _Parent_proxy->_Myfirstiter;
            _Parent_proxy->_Myfirstiter = this;
            _Myproxy                    = _Parent_proxy;
        }
    }

    void _Adopt_locked(const _Container_base12* _Parent) noexcept {
        _Lockit _Lock(_LOCK_DEBUG);
        _Adopt_unlocked(_Parent);
    }

    _CONSTEXPR20 void _Orphan_me_unlocked_v3() noexcept {
        if (!_Myproxy) { // already orphaned
            return;
        }

        // adopted, remove self from list
        _Iterator_base12** _Pnext = &_Myproxy->_Myfirstiter;
        while (*_Pnext && *_Pnext != this) {
            const auto _Temp = *_Pnext; // TRANSITION, VSO-1269037
            _Pnext           = &_Temp->_Mynextiter;
        }

        _STL_VERIFY(*_Pnext, "ITERATOR LIST CORRUPTED!");
        *_Pnext  = _Mynextiter;
        _Myproxy = nullptr;
    }

    void _Orphan_me_locked_v3() noexcept {
        _Lockit _Lock(_LOCK_DEBUG);
        _Orphan_me_unlocked_v3();
    }
#endif // _ITERATOR_DEBUG_LEVEL == 2
};

_CONSTEXPR20 void _Container_base12::_Orphan_all_unlocked_v3() noexcept {
    if (!_Myproxy) { // no proxy, already done
        return;
    }

    // proxy allocated, drain it
    for (auto& _Pnext = _Myproxy->_Myfirstiter; _Pnext; _Pnext = _Pnext->_Mynextiter) { // TRANSITION, VSO-1269037
        _Pnext->_Myproxy = nullptr;
    }
    _Myproxy->_Myfirstiter = nullptr;
}

_CONSTEXPR20 void _Container_base12::_Orphan_all() noexcept {
#if _ITERATOR_DEBUG_LEVEL == 2
#if _HAS_CXX20
    if (_STD is_constant_evaluated()) {
        _Orphan_all_unlocked_v3();
    } else
#endif // _HAS_CXX20
    {
        _Orphan_all_locked_v3();
    }
#endif // _ITERATOR_DEBUG_LEVEL == 2
}

_CONSTEXPR20 void _Container_base12::_Swap_proxy_and_iterators_unlocked(_Container_base12& _Right) noexcept {
    _Container_proxy* _Temp = _Myproxy;
    _Myproxy                = _Right._Myproxy;
    _Right._Myproxy         = _Temp;

    if (_Myproxy) {
        _Myproxy->_Mycont = this;
    }

    if (_Right._Myproxy) {
        _Right._Myproxy->_Mycont = &_Right;
    }
}

_CONSTEXPR20 void _Container_base12::_Swap_proxy_and_iterators(_Container_base12& _Right) noexcept {
#if _ITERATOR_DEBUG_LEVEL == 2
#if _HAS_CXX20
    if (_STD is_constant_evaluated()) {
        _Swap_proxy_and_iterators_unlocked(_Right);
    } else
#endif // _HAS_CXX20
    {
        _Swap_proxy_and_iterators_locked(_Right);
    }
#else // ^^^ _ITERATOR_DEBUG_LEVEL == 2 ^^^ / vvv _ITERATOR_DEBUG_LEVEL != 2 vvv
    _Swap_proxy_and_iterators_unlocked(_Right);
#endif // _ITERATOR_DEBUG_LEVEL != 2
}

#if _ITERATOR_DEBUG_LEVEL == 0
using _Container_base = _Container_base0;
using _Iterator_base  = _Iterator_base0;
#else // _ITERATOR_DEBUG_LEVEL == 0
using _Container_base = _Container_base12;
using _Iterator_base = _Iterator_base12;
#endif // _ITERATOR_DEBUG_LEVEL == 0

struct _Leave_proxy_unbound {
    explicit _Leave_proxy_unbound() = default;
}; // tag to indicate that a proxy is being allocated before it is safe to bind to a _Container_base12

struct _Fake_proxy_ptr_impl { // fake replacement for a container proxy smart pointer when no container proxy is in use
    _Fake_proxy_ptr_impl(const _Fake_proxy_ptr_impl&) = delete;
    _Fake_proxy_ptr_impl& operator=(const _Fake_proxy_ptr_impl&) = delete;
    _CONSTEXPR20 _Fake_proxy_ptr_impl(const _Fake_allocator&, _Leave_proxy_unbound) noexcept {}
    _CONSTEXPR20 _Fake_proxy_ptr_impl(const _Fake_allocator&, const _Container_base0&) noexcept {}

    _CONSTEXPR20 void _Bind(const _Fake_allocator&, _Container_base0*) noexcept {}
    _CONSTEXPR20 void _Release() noexcept {}
};

struct _Basic_container_proxy_ptr12 {
    // smart pointer components for a _Container_proxy * that don't depend on the allocator
    _Container_proxy* _Ptr = nullptr;

    constexpr void _Release() noexcept { // disengage this _Basic_container_proxy_ptr12
        _Ptr = nullptr;
    }

protected:
    _CONSTEXPR20 _Basic_container_proxy_ptr12()                       = default;
    _Basic_container_proxy_ptr12(const _Basic_container_proxy_ptr12&) = delete;
    _Basic_container_proxy_ptr12(_Basic_container_proxy_ptr12&&)      = delete;
};

template <class _Alloc>
struct _Container_proxy_ptr12 : _Basic_container_proxy_ptr12 {
    // smart pointer components for a _Container_proxy * for an allocator family
    _Alloc& _Al;

    _CONSTEXPR20 _Container_proxy_ptr12(_Alloc& _Al_, _Leave_proxy_unbound) : _Al(_Al_) {
        // create a new unbound _Container_proxy
        _Ptr = _Unfancy(_Al_.allocate(1));
        _Construct_in_place(*_Ptr);
    }

    _CONSTEXPR20 _Container_proxy_ptr12(_Alloc& _Al_, _Container_base12& _Mycont) : _Al(_Al_) {
        // create a new _Container_proxy pointing at _Mycont
        _Ptr = _Unfancy(_Al_.allocate(1));
        _Construct_in_place(*_Ptr, _STD addressof(_Mycont));
        _Mycont._Myproxy = _Ptr;
    }

    _CONSTEXPR20 void _Bind(_Alloc& _Old_alloc, _Container_base12* _Mycont) noexcept {
        // Attach the proxy stored in *this to _Mycont, and destroy _Mycont's existing proxy
        // with _Old_alloc. Requires that no iterators are alive referring to _Mycont.
        _Ptr->_Mycont = _Mycont;
        _Delete_plain_internal(_Old_alloc, _STD exchange(_Mycont->_Myproxy, _STD exchange(_Ptr, nullptr)));
    }

    _CONSTEXPR20 ~_Container_proxy_ptr12() {
        if (_Ptr) {
            _Delete_plain_internal(_Al, _Ptr);
        }
    }
};

#if _ITERATOR_DEBUG_LEVEL == 0
_INLINE_VAR constexpr _Fake_allocator _Fake_alloc{};
#define _GET_PROXY_ALLOCATOR(_Alty, _Al) _Fake_alloc // TRANSITION, VSO-1284799, should be _Fake_allocator{}
template <class _Alloc>
using _Container_proxy_ptr = _Fake_proxy_ptr_impl;
#else // _ITERATOR_DEBUG_LEVEL == 0
#define _GET_PROXY_ALLOCATOR(_Alty, _Al) static_cast<_Rebind_alloc_t<_Alty, _Container_proxy>>(_Al)
template <class _Alloc>
using _Container_proxy_ptr = _Container_proxy_ptr12<_Rebind_alloc_t<_Alloc, _Container_proxy>>;
#endif // _ITERATOR_DEBUG_LEVEL == 0

struct _Zero_then_variadic_args_t {
    explicit _Zero_then_variadic_args_t() = default;
}; // tag type for value-initializing first, constructing second from remaining args

struct _One_then_variadic_args_t {
    explicit _One_then_variadic_args_t() = default;
}; // tag type for constructing first from one arg, constructing second from remaining args

template <class _Ty1, class _Ty2, bool = ::std::is_empty_v<_Ty1> && !::std::is_final_v<_Ty1>>
class _Compressed_pair final : private _Ty1 { // store a ::std::pair of values, deriving from empty first
public:
    _Ty2 _Myval2;

    using _Mybase = _Ty1; // for visualization

    template <class... _Other2>
    constexpr explicit _Compressed_pair(_Zero_then_variadic_args_t, _Other2&&... _Val2) noexcept(
        ::std::conjunction_v<::std::is_nothrow_default_constructible<_Ty1>, ::std::is_nothrow_constructible<_Ty2, _Other2...>>)
        : _Ty1(), _Myval2(_STD forward<_Other2>(_Val2)...) {}

    template <class _Other1, class... _Other2>
    constexpr _Compressed_pair(_One_then_variadic_args_t, _Other1&& _Val1, _Other2&&... _Val2) noexcept(
        ::std::conjunction_v<::std::is_nothrow_constructible<_Ty1, _Other1>, ::std::is_nothrow_constructible<_Ty2, _Other2...>>)
        : _Ty1(_STD forward<_Other1>(_Val1)), _Myval2(_STD forward<_Other2>(_Val2)...) {}

    constexpr _Ty1& _Get_first() noexcept {
        return *this;
    }

    constexpr const _Ty1& _Get_first() const noexcept {
        return *this;
    }
};

template <class _Ty1, class _Ty2>
class _Compressed_pair<_Ty1, _Ty2, false> final { // store a ::std::pair of values, not deriving from first
public:
    _Ty1 _Myval1;
    _Ty2 _Myval2;

    template <class... _Other2>
    constexpr explicit _Compressed_pair(_Zero_then_variadic_args_t, _Other2&&... _Val2) noexcept(
        ::std::conjunction_v<::std::is_nothrow_default_constructible<_Ty1>, ::std::is_nothrow_constructible<_Ty2, _Other2...>>)
        : _Myval1(), _Myval2(_STD forward<_Other2>(_Val2)...) {}

    template <class _Other1, class... _Other2>
    constexpr _Compressed_pair(_One_then_variadic_args_t, _Other1&& _Val1, _Other2&&... _Val2) noexcept(
        ::std::conjunction_v<::std::is_nothrow_constructible<_Ty1, _Other1>, ::std::is_nothrow_constructible<_Ty2, _Other2...>>)
        : _Myval1(_STD forward<_Other1>(_Val1)), _Myval2(_STD forward<_Other2>(_Val2)...) {}

    constexpr _Ty1& _Get_first() noexcept {
        return _Myval1;
    }

    constexpr const _Ty1& _Get_first() const noexcept {
        return _Myval1;
    }
};

struct _Move_allocator_tag {
    explicit _Move_allocator_tag() = default;
};

template <class _Ty>
::std::pair<_Ty*, ptrdiff_t> _Get_temporary_buffer(ptrdiff_t _Count) noexcept {
    if (static_cast<size_t>(_Count) <= static_cast<size_t>(-1) / sizeof(_Ty)) {
        for (; 0 < _Count; _Count /= 2) {
            const auto _Size = static_cast<size_t>(_Count) * sizeof(_Ty);
            void* _Pbuf;
#ifdef __cpp_aligned_new
            if constexpr (alignof(_Ty) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                _Pbuf = ::operator new (_Size, ::std::align_val_t{alignof(_Ty)}, ::std::nothrow);
            } else
#endif // __cpp_aligned_new
            {
                _Pbuf = ::operator new(_Size, ::std::nothrow);
            }

            if (_Pbuf) {
                return {static_cast<_Ty*>(_Pbuf), _Count};
            }
        }
    }

    return {nullptr, 0};
}

template <class _Ty>
void _Return_temporary_buffer(_Ty* const _Pbuf) noexcept {
#ifdef __cpp_aligned_new
    if constexpr (alignof(_Ty) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        ::operator delete (_Pbuf, ::std::align_val_t{alignof(_Ty)});
    } else
#endif // __cpp_aligned_new
    {
        ::operator delete(_Pbuf);
    }
}

template <class _NoThrowFwdIt>
struct _NODISCARD _Uninitialized_backout {
    // struct to undo partially constructed ranges in _Uninitialized_xxx algorithms
    _NoThrowFwdIt _First;
    _NoThrowFwdIt _Last;

    constexpr explicit _Uninitialized_backout(_NoThrowFwdIt _Dest) : _First(_Dest), _Last(_Dest) {}

    constexpr _Uninitialized_backout(_NoThrowFwdIt _First_, _NoThrowFwdIt _Last_) : _First(_First_), _Last(_Last_) {}

    _Uninitialized_backout(const _Uninitialized_backout&) = delete;
    _Uninitialized_backout& operator=(const _Uninitialized_backout&) = delete;

    _CONSTEXPR20 ~_Uninitialized_backout() {
        _Destroy_range(_First, _Last);
    }

    template <class... _Types>
    _CONSTEXPR20 void _Emplace_back(_Types&&... _Vals) {
        // construct a new element at *_Last and increment
        _Construct_in_place(*_Last, _STD forward<_Types>(_Vals)...);
        ++_Last;
    }

    constexpr _NoThrowFwdIt _Release() { // suppress any exception handling backout and return _Last
        _First = _Last;
        return _Last;
    }
};

template <class _InIt, class _NoThrowFwdIt>
_CONSTEXPR20 _NoThrowFwdIt _Uninitialized_move_unchecked(_InIt _First, const _InIt _Last, _NoThrowFwdIt _Dest) {
    // move [_First, _Last) to raw [_Dest, ...)
    if constexpr (_Iter_move_cat<_InIt, _NoThrowFwdIt>::_Bitcopy_constructible) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Copy_memmove(_First, _Last, _Dest);
        }
    }
    _Uninitialized_backout<_NoThrowFwdIt> _Backout{_Dest};
    for (; _First != _Last; ++_First) {
        _Backout._Emplace_back(_STD move(*_First));
    }

    return _Backout._Release();
}

#ifdef __cpp_lib_concepts
namespace ranges {
    template <class _It>
    concept _No_throw_input_iterator = input_iterator<_It> //
        && is_lvalue_reference_v<iter_reference_t<_It>> //
        && same_as<remove_cvref_t<iter_reference_t<_It>>, iter_value_t<_It>>;

    template <class _Se, class _It>
    concept _No_throw_sentinel_for = sentinel_for<_Se, _It>;

    template <class _It>
    concept _No_throw_forward_iterator = _No_throw_input_iterator<_It> //
        && forward_iterator<_It> //
        && _No_throw_sentinel_for<_It, _It>;

    template <class _Rng>
    concept _No_throw_input_range = range<_Rng> //
        && _No_throw_input_iterator<iterator_t<_Rng>> //
        && _No_throw_sentinel_for<sentinel_t<_Rng>, iterator_t<_Rng>>;

    template <class _Rng>
    concept _No_throw_forward_range = _No_throw_input_range<_Rng> //
        && _No_throw_forward_iterator<iterator_t<_Rng>>;

    template <class _InIt, class _OutIt>
    in_out_result<_InIt, _OutIt> _Copy_memcpy_count(_InIt _IFirst, _OutIt _OFirst, const size_t _Count) noexcept {
        const auto _IFirstPtr     = _To_address(_IFirst);
        const auto _OFirstPtr     = _To_address(_OFirst);
        const auto _IFirst_ch     = const_cast<char*>(reinterpret_cast<const volatile char*>(_IFirstPtr));
        const auto _OFirst_ch     = const_cast<char*>(reinterpret_cast<const volatile char*>(_OFirstPtr));
        const size_t _Count_bytes = _Count * sizeof(iter_value_t<_InIt>);
        _CSTD memcpy(_OFirst_ch, _IFirst_ch, _Count_bytes);
        if constexpr (::std::is_pointer_v<_InIt>) {
            _IFirst = reinterpret_cast<_InIt>(_IFirst_ch + _Count_bytes);
        } else {
            _IFirst += _Count;
        }

        if constexpr (::std::is_pointer_v<_OutIt>) {
            _OFirst = reinterpret_cast<_OutIt>(_OFirst_ch + _Count_bytes);
        } else {
            _OFirst += _Count;
        }
        return {_STD move(_IFirst), _STD move(_OFirst)};
    }

    template <class _InIt, class _OutIt, class _DistIt>
    in_out_result<_InIt, _OutIt> _Copy_memcpy_distance(
        _InIt _IFirst, _OutIt _OFirst, const _DistIt _DFirst, const _DistIt _DLast) noexcept {
        // equivalent to _Copy_memcpy_count(_IFirst, _OFirst, _DLast - _DFirst) but computes distance more efficiently
        const auto _IFirstPtr   = _To_address(_IFirst);
        const auto _OFirstPtr   = _To_address(_OFirst);
        const auto _DFirstPtr   = _To_address(_DFirst);
        const auto _DLastPtr    = _To_address(_DLast);
        const auto _IFirst_ch   = const_cast<char*>(reinterpret_cast<const volatile char*>(_IFirstPtr));
        const auto _OFirst_ch   = const_cast<char*>(reinterpret_cast<const volatile char*>(_OFirstPtr));
        const auto _DFirst_ch   = const_cast<char*>(reinterpret_cast<const volatile char*>(_DFirstPtr));
        const auto _DLast_ch    = const_cast<char*>(reinterpret_cast<const volatile char*>(_DLastPtr));
        const auto _Count_bytes = static_cast<size_t>(_DLast_ch - _DFirst_ch);
        _CSTD memcpy(_OFirst_ch, _IFirst_ch, _Count_bytes);
        if constexpr (::std::is_pointer_v<_InIt>) {
            _IFirst = reinterpret_cast<_InIt>(_IFirst_ch + _Count_bytes);
        } else {
            _IFirst += _Count_bytes / sizeof(iter_value_t<_InIt>);
        }

        if constexpr (::std::is_pointer_v<_OutIt>) {
            _OFirst = reinterpret_cast<_OutIt>(_OFirst_ch + _Count_bytes);
        } else {
            _OFirst += _Count_bytes / sizeof(iter_value_t<_OutIt>);
        }
        return {_STD move(_IFirst), _STD move(_OFirst)};
    }

    template <class _InIt, class _OutIt>
    in_out_result<_InIt, _OutIt> _Copy_memcpy_common(
        _InIt _IFirst, _InIt _ILast, _OutIt _OFirst, _OutIt _OLast) noexcept {
        const auto _IFirstPtr   = _To_address(_IFirst);
        const auto _ILastPtr    = _To_address(_ILast);
        const auto _OFirstPtr   = _To_address(_OFirst);
        const auto _OLastPtr    = _To_address(_OLast);
        const auto _IFirst_ch   = const_cast<char*>(reinterpret_cast<const volatile char*>(_IFirstPtr));
        const auto _ILast_ch    = const_cast<const char*>(reinterpret_cast<const volatile char*>(_ILastPtr));
        const auto _OFirst_ch   = const_cast<char*>(reinterpret_cast<const volatile char*>(_OFirstPtr));
        const auto _OLast_ch    = const_cast<const char*>(reinterpret_cast<const volatile char*>(_OLastPtr));
        const auto _Count_bytes = static_cast<size_t>((_STD min) (_ILast_ch - _IFirst_ch, _OLast_ch - _OFirst_ch));
        _CSTD memcpy(_OFirst_ch, _IFirst_ch, _Count_bytes);
        if constexpr (::std::is_pointer_v<_InIt>) {
            _IFirst = reinterpret_cast<_InIt>(_IFirst_ch + _Count_bytes);
        } else {
            _IFirst += _Count_bytes / sizeof(iter_value_t<_InIt>);
        }

        if constexpr (::std::is_pointer_v<_OutIt>) {
            _OFirst = reinterpret_cast<_OutIt>(_OFirst_ch + _Count_bytes);
        } else {
            _OFirst += _Count_bytes / sizeof(iter_value_t<_OutIt>);
        }
        return {_STD move(_IFirst), _STD move(_OFirst)};
    }

    template <class _In, class _Out>
    using uninitialized_move_result = in_out_result<_In, _Out>;

    // clang-format off
    template <input_iterator _It, sentinel_for<_It> _Se, _No_throw_forward_iterator _Out,
        _No_throw_sentinel_for<_Out> _OSe>
        requires (constructible_from<iter_value_t<_Out>, iter_rvalue_reference_t<_It>>)
    uninitialized_move_result<_It, _Out> _Uninitialized_move_unchecked(
        _It _IFirst, _Se _ILast, _Out _OFirst, _OSe _OLast) {
        // clang-format on
        constexpr bool _Is_sized1 = sized_sentinel_for<_Se, _It>;
        constexpr bool _Is_sized2 = sized_sentinel_for<_OSe, _Out>;
        if constexpr (_Iter_move_cat<_It, _Out>::_Bitcopy_constructible
                      && _Sized_or_unreachable_sentinel_for<_Se, _It> //
                      && _Sized_or_unreachable_sentinel_for<_OSe, _Out>) {
            if constexpr (_Is_sized1 && _Is_sized2) {
                return _Copy_memcpy_common(_IFirst, _RANGES next(_IFirst, _STD move(_ILast)), _OFirst,
                    _RANGES next(_OFirst, _STD move(_OLast)));
            } else if constexpr (_Is_sized1) {
                return _Copy_memcpy_distance(_IFirst, _OFirst, _IFirst, _RANGES next(_IFirst, _STD move(_ILast)));
            } else if constexpr (_Is_sized2) {
                return _Copy_memcpy_distance(_IFirst, _OFirst, _OFirst, _RANGES next(_OFirst, _STD move(_OLast)));
            } else {
                _STL_ASSERT(false, "Tried to uninitialized_move two ranges with unreachable sentinels");
            }
        } else {
            _Uninitialized_backout _Backout{_STD move(_OFirst)};

            for (; _IFirst != _ILast && _Backout._Last != _OLast; ++_IFirst) {
                _Backout._Emplace_back(_RANGES iter_move(_IFirst));
            }

            return {_STD move(_IFirst), _Backout._Release()};
        }
    }
} // namespace ranges
#endif // __cpp_lib_concepts

template <class _Alloc>
class _NODISCARD _Uninitialized_backout_al {
    // struct to undo partially constructed ranges in _Uninitialized_xxx_al algorithms
    using pointer = _Alloc_ptr_t<_Alloc>;

public:
    _CONSTEXPR20 _Uninitialized_backout_al(pointer _Dest, _Alloc& _Al_) : _First(_Dest), _Last(_Dest), _Al(_Al_) {}

    _Uninitialized_backout_al(const _Uninitialized_backout_al&) = delete;
    _Uninitialized_backout_al& operator=(const _Uninitialized_backout_al&) = delete;

    _CONSTEXPR20 ~_Uninitialized_backout_al() {
        _Destroy_range(_First, _Last, _Al);
    }

    template <class... _Types>
    _CONSTEXPR20 void _Emplace_back(_Types&&... _Vals) { // construct a new element at *_Last and increment
        allocator_traits<_Alloc>::construct(_Al, _Unfancy(_Last), _STD forward<_Types>(_Vals)...);
        ++_Last;
    }

    constexpr pointer _Release() { // suppress any exception handling backout and return _Last
        _First = _Last;
        return _Last;
    }

private:
    pointer _First;
    pointer _Last;
    _Alloc& _Al;
};

template <class _InIt, class _Alloc>
_CONSTEXPR20 _Alloc_ptr_t<_Alloc> _Uninitialized_copy(
    const _InIt _First, const _InIt _Last, _Alloc_ptr_t<_Alloc> _Dest, _Alloc& _Al) {
    // copy [_First, _Last) to raw _Dest, using _Al
    // note: only called internally from elsewhere in the STL
    using _Ptrval = typename _Alloc::value_type*;

    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);

    if constexpr (::std::conjunction_v<bool_constant<_Iter_copy_cat<decltype(_UFirst), _Ptrval>::_Bitcopy_constructible>,
                      _Uses_default_construct<_Alloc, _Ptrval, decltype(*_UFirst)>>) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            _Copy_memmove(_UFirst, _ULast, _Unfancy(_Dest));
            _Dest += _ULast - _UFirst;
            return _Dest;
        }
    }

    _Uninitialized_backout_al<_Alloc> _Backout{_Dest, _Al};
    for (; _UFirst != _ULast; ++_UFirst) {
        _Backout._Emplace_back(*_UFirst);
    }

    return _Backout._Release();
}

template <class _InIt, class _NoThrowFwdIt>
_CONSTEXPR20 _NoThrowFwdIt _Uninitialized_copy_unchecked(_InIt _First, const _InIt _Last, _NoThrowFwdIt _Dest) {
    // copy [_First, _Last) to raw [_Dest, ...)
    if constexpr (_Iter_copy_cat<_InIt, _NoThrowFwdIt>::_Bitcopy_constructible) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            return _Copy_memmove(_First, _Last, _Dest);
        }
    }

    _Uninitialized_backout<_NoThrowFwdIt> _Backout{_Dest};
    for (; _First != _Last; ++_First) {
        _Backout._Emplace_back(*_First);
    }

    return _Backout._Release();
}

template <class _InIt, class _NoThrowFwdIt>
_NoThrowFwdIt uninitialized_copy(const _InIt _First, const _InIt _Last, _NoThrowFwdIt _Dest) {
    // copy [_First, _Last) to raw [_Dest, ...)
    _Adl_verify_range(_First, _Last);
    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);
    auto _UDest       = _Get_unwrapped_n(_Dest, _Idl_distance<_InIt>(_UFirst, _ULast));
    _Seek_wrapped(_Dest, _Uninitialized_copy_unchecked(_UFirst, _ULast, _UDest));
    return _Dest;
}

template <class _InIt, class _Alloc>
_CONSTEXPR20 _Alloc_ptr_t<_Alloc> _Uninitialized_move(
    const _InIt _First, const _InIt _Last, _Alloc_ptr_t<_Alloc> _Dest, _Alloc& _Al) {
    // move [_First, _Last) to raw _Dest, using _Al
    // note: only called internally from elsewhere in the STL
    using _Ptrval     = typename _Alloc::value_type*;
    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);
    if constexpr (::std::conjunction_v<bool_constant<_Iter_move_cat<decltype(_UFirst), _Ptrval>::_Bitcopy_constructible>,
                      _Uses_default_construct<_Alloc, _Ptrval, decltype(_STD move(*_UFirst))>>) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            _Copy_memmove(_UFirst, _ULast, _Unfancy(_Dest));
            return _Dest + (_ULast - _UFirst);
        }
    }

    _Uninitialized_backout_al<_Alloc> _Backout{_Dest, _Al};
    for (; _UFirst != _ULast; ++_UFirst) {
        _Backout._Emplace_back(_STD move(*_UFirst));
    }

    return _Backout._Release();
}

template <class _Alloc>
_CONSTEXPR20 _Alloc_ptr_t<_Alloc> _Uninitialized_fill_n(
    _Alloc_ptr_t<_Alloc> _First, _Alloc_size_t<_Alloc> _Count, const typename _Alloc::value_type& _Val, _Alloc& _Al) {
    // copy _Count copies of _Val to raw _First, using _Al
    using _Ty = typename _Alloc::value_type;
    if constexpr (_Fill_memset_is_safe<_Ty*, _Ty> && _Uses_default_construct<_Alloc, _Ty*, _Ty>::value) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            _Fill_memset(_Unfancy(_First), _Val, static_cast<size_t>(_Count));
            return _First + _Count;
        }
    } else if constexpr (_Fill_zero_memset_is_safe<_Ty*, _Ty> && _Uses_default_construct<_Alloc, _Ty*, _Ty>::value) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            if (_Is_all_bits_zero(_Val)) {
                _Fill_zero_memset(_Unfancy(_First), static_cast<size_t>(_Count));
                return _First + _Count;
            }
        }
    }

    _Uninitialized_backout_al<_Alloc> _Backout{_First, _Al};
    for (; 0 < _Count; --_Count) {
        _Backout._Emplace_back(_Val);
    }

    return _Backout._Release();
}

template <class _NoThrowFwdIt, class _Tval>
void uninitialized_fill(const _NoThrowFwdIt _First, const _NoThrowFwdIt _Last, const _Tval& _Val) {
    // copy _Val throughout raw [_First, _Last)
    _Adl_verify_range(_First, _Last);
    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);
    if constexpr (_Fill_memset_is_safe<_Unwrapped_t<const _NoThrowFwdIt&>, _Tval>) {
        _Fill_memset(_UFirst, _Val, static_cast<size_t>(_ULast - _UFirst));
    } else {
        if constexpr (_Fill_zero_memset_is_safe<_Unwrapped_t<const _NoThrowFwdIt&>, _Tval>) {
            if (_Is_all_bits_zero(_Val)) {
                _Fill_zero_memset(_UFirst, static_cast<size_t>(_ULast - _UFirst));
                return;
            }
        }

        _Uninitialized_backout<_Unwrapped_t<const _NoThrowFwdIt&>> _Backout{_UFirst};
        while (_Backout._Last != _ULast) {
            _Backout._Emplace_back(_Val);
        }

        _Backout._Release();
    }
}

template <class _NoThrowFwdIt>
_INLINE_VAR constexpr bool _Use_memset_value_construct_v =
    ::std::conjunction_v<bool_constant<_Iterator_is_contiguous<_NoThrowFwdIt>>, ::std::is_scalar<_Iter_value_t<_NoThrowFwdIt>>,
        ::std::negation<::std::is_volatile<remove_reference_t<_Iter_ref_t<_NoThrowFwdIt>>>>,
        ::std::negation<::std::is_member_pointer<_Iter_value_t<_NoThrowFwdIt>>>>;

template <class _Ptr>
_Ptr _Zero_range(const _Ptr _First, const _Ptr _Last) { // fill [_First, _Last) with zeroes
    char* const _First_ch = reinterpret_cast<char*>(_To_address(_First));
    char* const _Last_ch  = reinterpret_cast<char*>(_To_address(_Last));
    _CSTD memset(_First_ch, 0, static_cast<size_t>(_Last_ch - _First_ch));
    return _Last;
}

template <class _Alloc>
_CONSTEXPR20 _Alloc_ptr_t<_Alloc> _Uninitialized_value_construct_n(
    _Alloc_ptr_t<_Alloc> _First, _Alloc_size_t<_Alloc> _Count, _Alloc& _Al) {
    // value-initialize _Count objects to raw _First, using _Al
    using _Ptrty = typename _Alloc::value_type*;
    if constexpr (_Use_memset_value_construct_v<_Ptrty> && _Uses_default_construct<_Alloc, _Ptrty>::value) {
#if _HAS_CXX20
        if (!_STD is_constant_evaluated())
#endif // _HAS_CXX20
        {
            auto _PFirst = _Unfancy(_First);
            _Zero_range(_PFirst, _PFirst + _Count);
            return _First + _Count;
        }
    }

    _Uninitialized_backout_al<_Alloc> _Backout{_First, _Al};
    for (; 0 < _Count; --_Count) {
        _Backout._Emplace_back();
    }

    return _Backout._Release();
}

template <class _NoThrowFwdIt, class _Diff>
_NoThrowFwdIt _Uninitialized_value_construct_n_unchecked1(_NoThrowFwdIt _UFirst, _Diff _Count) {
    // value-initialize all elements in [_UFirst, _UFirst + _Count_raw)
    _STL_INTERNAL_CHECK(_Count >= 0);
    if constexpr (_Use_memset_value_construct_v<_NoThrowFwdIt>) {
        return _Zero_range(_UFirst, _UFirst + _Count);
    } else {
        _Uninitialized_backout<_NoThrowFwdIt> _Backout{_UFirst};
        for (; 0 < _Count; --_Count) {
            _Backout._Emplace_back();
        }

        return _Backout._Release();
    }
}

#if _HAS_DEPRECATED_TEMPORARY_BUFFER
template <class _Ty>
_CXX17_DEPRECATE_TEMPORARY_BUFFER _NODISCARD ::std::pair<_Ty*, ptrdiff_t> get_temporary_buffer(ptrdiff_t _Count) noexcept {
    return _Get_temporary_buffer<_Ty>(_Count);
}

template <class _Ty>
_CXX17_DEPRECATE_TEMPORARY_BUFFER void return_temporary_buffer(_Ty* _Pbuf) {
    _Return_temporary_buffer(_Pbuf);
}
#endif // _HAS_DEPRECATED_TEMPORARY_BUFFER

// assumes _Args have already been _Remove_cvref_t'd
template <class _Key, class... _Args>
struct _In_place_key_extract_set {
    // by default we can't extract the key in the emplace family and must construct a node we might not use
    static constexpr bool _Extractable = false;
};

template <class _Key>
struct _In_place_key_extract_set<_Key, _Key> {
    // we can extract the key in emplace if the emplaced type is identical to the key type
    static constexpr bool _Extractable = true;
    static const _Key& _Extract(const _Key& _Val) noexcept {
        return _Val;
    }
};

// assumes _Args have already been _Remove_cvref_t'd
template <class _Key, class... _Args>
struct _In_place_key_extract_map {
    // by default we can't extract the key in the emplace family and must construct a node we might not use
    static constexpr bool _Extractable = false;
};

template <class _Key, class _Second>
struct _In_place_key_extract_map<_Key, _Key, _Second> {
    // if we would call the ::std::pair(key, value) constructor family, we can use the first parameter as the key
    static constexpr bool _Extractable = true;
    static const _Key& _Extract(const _Key& _Val, const _Second&) noexcept {
        return _Val;
    }
};

template <class _Key, class _First, class _Second>
struct _In_place_key_extract_map<_Key, ::std::pair<_First, _Second>> {
    // if we would call the ::std::pair(::std::pair<other, other>) constructor family, we can use the ::std::pair.first member as the key
    static constexpr bool _Extractable = ::std::is_same_v<_Key, _Remove_cvref_t<_First>>;
    static const _Key& _Extract(const ::std::pair<_First, _Second>& _Val) {
        return _Val.first;
    }
};

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma warning(push)
#pragma warning(disable : 4624) // '%s': destructor was implicitly defined as deleted
#endif
template <class _Ty>
struct _Wrap {
    _Ty _Value; // workaround for VSO-586813 "T^ is not allowed in a union"
};
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma warning(pop)
#endif

template <class _Alloc>
struct _Alloc_temporary2 {
    using value_type = typename _Alloc::value_type;
    using _Traits    = allocator_traits<_Alloc>;

    _Alloc& _Al;

#ifdef __cplusplus_winrt
    union {
        _Wrap<value_type> _Storage;
    };

    _NODISCARD _CONSTEXPR20 value_type& _Get_value() noexcept {
        return _Storage._Value;
    }

    _NODISCARD _CONSTEXPR20 const value_type& _Get_value() const noexcept {
        return _Storage._Value;
    }
#else // ^^^ workaround for VSO-586813 "T^ is not allowed in a union" ^^^ / vvv no workaround vvv
    union {
        value_type _Value;
    };

    _NODISCARD _CONSTEXPR20 value_type& _Get_value() noexcept {
        return _Value;
    }

    _NODISCARD _CONSTEXPR20 const value_type& _Get_value() const noexcept {
        return _Value;
    }
#endif // ^^^ no workaround ^^^

    template <class... _Args>
    _CONSTEXPR20 explicit _Alloc_temporary2(_Alloc& _Al_, _Args&&... _Vals) noexcept(
        noexcept(_Traits::construct(_Al_, _STD addressof(_Get_value()), _STD forward<_Args>(_Vals)...)))
        : _Al(_Al_) {
        _Traits::construct(_Al, _STD addressof(_Get_value()), _STD forward<_Args>(_Vals)...);
    }

    _Alloc_temporary2(const _Alloc_temporary2&) = delete;
    _Alloc_temporary2& operator=(const _Alloc_temporary2&) = delete;

    _CONSTEXPR20 ~_Alloc_temporary2() {
        _Traits::destroy(_Al, _STD addressof(_Get_value()));
    }
};

template <class _Alloc>
_NODISCARD constexpr bool _Allocators_equal(const _Alloc& _Lhs, const _Alloc& _Rhs) noexcept {
    if constexpr (allocator_traits<_Alloc>::is_always_equal::value) {
        return true;
    } else {
        return _Lhs == _Rhs;
    }
}

template <class _FwdIt, class _Ty>
_NODISCARD _CONSTEXPR20 _FwdIt remove(_FwdIt _First, const _FwdIt _Last, const _Ty& _Val) {
    // remove each matching _Val
    _Adl_verify_range(_First, _Last);
    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);
    _UFirst           = _Find_unchecked(_UFirst, _ULast, _Val);
    auto _UNext       = _UFirst;
    if (_UFirst != _ULast) {
        while (++_UFirst != _ULast) {
            if (!(*_UFirst == _Val)) {
                *_UNext = _STD move(*_UFirst);
                ++_UNext;
            }
        }
    }

    _Seek_wrapped(_First, _UNext);
    return _First;
}

template <class _FwdIt, class _Pr>
_NODISCARD _CONSTEXPR20 _FwdIt remove_if(_FwdIt _First, const _FwdIt _Last, _Pr _Pred) {
    // remove each satisfying _Pred
    _Adl_verify_range(_First, _Last);
    auto _UFirst      = _Get_unwrapped(_First);
    const auto _ULast = _Get_unwrapped(_Last);
    _UFirst           = _STD find_if(_UFirst, _ULast, _Pass_fn(_Pred));
    auto _UNext       = _UFirst;
    if (_UFirst != _ULast) {
        while (++_UFirst != _ULast) {
            if (!_Pred(*_UFirst)) {
                *_UNext = _STD move(*_UFirst);
                ++_UNext;
            }
        }
    }

    _Seek_wrapped(_First, _UNext);
    return _First;
}

template <class _Container, class _Uty>
_CONSTEXPR20 typename _Container::size_type _Erase_remove(_Container& _Cont, const _Uty& _Val) {
    // erase each element matching _Val
    auto _First          = _Cont.begin();
    const auto _Last     = _Cont.end();
    const auto _Old_size = _Cont.size();
    _Seek_wrapped(_First, _STD remove(_Get_unwrapped(_First), _Get_unwrapped(_Last), _Val));
    _Cont.erase(_First, _Last);
    return _Old_size - _Cont.size();
}

template <class _Container, class _Pr>
_CONSTEXPR20 typename _Container::size_type _Erase_remove_if(_Container& _Cont, _Pr _Pred) {
    // erase each element satisfying _Pred
    auto _First          = _Cont.begin();
    const auto _Last     = _Cont.end();
    const auto _Old_size = _Cont.size();
    _Seek_wrapped(_First, _STD remove_if(_Get_unwrapped(_First), _Get_unwrapped(_Last), _Pred));
    _Cont.erase(_First, _Last);
    return _Old_size - _Cont.size();
}

template <class _Container, class _Pr>
typename _Container::size_type _Erase_nodes_if(_Container& _Cont, _Pr _Pred) {
    // erase each element satisfying _Pred
    auto _First          = _Cont.begin();
    const auto _Last     = _Cont.end();
    const auto _Old_size = _Cont.size();
    while (_First != _Last) {
        if (_Pred(*_First)) {
            _First = _Cont.erase(_First);
        } else {
            ++_First;
        }
    }
    return _Old_size - _Cont.size();
}

#if _HAS_CXX20
template <class _Ty, class _Alloc, class... _Types, enable_if_t<!_Is_specialization_v<_Ty, ::std::pair>, int> = 0>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al, _Types&&... _Args) noexcept {
    if constexpr (!uses_allocator_v<_Ty, _Alloc>) {
        static_assert(::std::is_constructible_v<_Ty, _Types...>,
            "If uses_allocator_v<T, Alloc> does not hold, T must be constructible from Types...");
        (void) _Al;
        return _STD forward_as_tuple(_STD forward<_Types>(_Args)...);
    } else if constexpr (::std::is_constructible_v<_Ty, allocator_arg_t, const _Alloc&, _Types...>) {
        using _ReturnType = tuple<allocator_arg_t, const _Alloc&, _Types&&...>;
        return _ReturnType{allocator_arg, _Al, _STD forward<_Types>(_Args)...};
    } else if constexpr (::std::is_constructible_v<_Ty, _Types..., const _Alloc&>) {
        return _STD forward_as_tuple(_STD forward<_Types>(_Args)..., _Al);
    } else {
        static_assert(_Always_false<_Ty>,
            "T must be constructible from either (allocator_arg_t, const Alloc&, Types...) "
            "or (Types..., const Alloc&) if uses_allocator_v<T, Alloc> is true");
    }
}

template <class _Ty, class _Alloc, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int> = 0>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al) noexcept;

template <class _Ty, class _Alloc, class _Uty1, class _Uty2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int> = 0>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al, _Uty1&& _Val1, _Uty2&& _Val2) noexcept;

template <class _Ty, class _Alloc, class _Uty1, class _Uty2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int> = 0>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al, const ::std::pair<_Uty1, _Uty2>& _Pair) noexcept;

template <class _Ty, class _Alloc, class _Uty1, class _Uty2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int> = 0>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al, ::std::pair<_Uty1, _Uty2>&& _Pair) noexcept;

template <class _Ty, class _Alloc, class _Tuple1, class _Tuple2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int> = 0>
_NODISCARD constexpr auto uses_allocator_construction_args(
    const _Alloc& _Al, piecewise_construct_t, _Tuple1&& _Tup1, _Tuple2&& _Tup2) noexcept {
    return _STD make_tuple(piecewise_construct,
        _STD apply(
            [&_Al](auto&&... _Tuple_args) {
                return _STD uses_allocator_construction_args<typename _Ty::first_type>(
                    _Al, _STD forward<decltype(_Tuple_args)>(_Tuple_args)...);
            },
            _STD forward<_Tuple1>(_Tup1)),
        _STD apply(
            [&_Al](auto&&... _Tuple_args) {
                return _STD uses_allocator_construction_args<typename _Ty::second_type>(
                    _Al, _STD forward<decltype(_Tuple_args)>(_Tuple_args)...);
            },
            _STD forward<_Tuple2>(_Tup2)));
}

template <class _Ty, class _Alloc, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int>>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al) noexcept {
    // equivalent to
    // return _STD uses_allocator_construction_args<_Ty>(_Al, piecewise_construct, tuple<>{}, tuple<>{});
    return _STD make_tuple(piecewise_construct, _STD uses_allocator_construction_args<typename _Ty::first_type>(_Al),
        _STD uses_allocator_construction_args<typename _Ty::second_type>(_Al));
}

template <class _Ty, class _Alloc, class _Uty1, class _Uty2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int>>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al, _Uty1&& _Val1, _Uty2&& _Val2) noexcept {
    // equivalent to
    // return _STD uses_allocator_construction_args<_Ty>(_Al, piecewise_construct,
    //     _STD forward_as_tuple(_STD forward<_Uty1>(_Val1)), _STD forward_as_tuple(_STD forward<_Uty2>(_Val2)));
    return _STD make_tuple(piecewise_construct,
        _STD uses_allocator_construction_args<typename _Ty::first_type>(_Al, _STD forward<_Uty1>(_Val1)),
        _STD uses_allocator_construction_args<typename _Ty::second_type>(_Al, _STD forward<_Uty2>(_Val2)));
}

template <class _Ty, class _Alloc, class _Uty1, class _Uty2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int>>
_NODISCARD constexpr auto uses_allocator_construction_args(
    const _Alloc& _Al, const ::std::pair<_Uty1, _Uty2>& _Pair) noexcept {
    // equivalent to
    // return _STD uses_allocator_construction_args<_Ty>(_Al, piecewise_construct,
    //     _STD forward_as_tuple(_Pair.first), _STD forward_as_tuple(_Pair.second));
    return _STD make_tuple(piecewise_construct,
        _STD uses_allocator_construction_args<typename _Ty::first_type>(_Al, _Pair.first),
        _STD uses_allocator_construction_args<typename _Ty::second_type>(_Al, _Pair.second));
}

template <class _Ty, class _Alloc, class _Uty1, class _Uty2, enable_if_t<_Is_specialization_v<_Ty, ::std::pair>, int>>
_NODISCARD constexpr auto uses_allocator_construction_args(const _Alloc& _Al, ::std::pair<_Uty1, _Uty2>&& _Pair) noexcept {
    // equivalent to
    // return _STD uses_allocator_construction_args<_Ty>(_Al, piecewise_construct,
    //     _STD forward_as_tuple(_STD get<0>(_STD move(_Pair)), _STD forward_as_tuple(_STD get<1>(_STD move(_Pair)));
    return _STD make_tuple(piecewise_construct,
        _STD uses_allocator_construction_args<typename _Ty::first_type>(_Al, _STD get<0>(_STD move(_Pair))),
        _STD uses_allocator_construction_args<typename _Ty::second_type>(_Al, _STD get<1>(_STD move(_Pair))));
}

template <class _Ty, class _Alloc, class... _Types>
_NODISCARD constexpr _Ty make_obj_using_allocator(const _Alloc& _Al, _Types&&... _Args) {
    return _STD make_from_tuple<_Ty>(_STD uses_allocator_construction_args<_Ty>(_Al, _STD forward<_Types>(_Args)...));
}

template <class _Ty, class _Alloc, class... _Types>
constexpr _Ty* uninitialized_construct_using_allocator(_Ty* _Ptr, const _Alloc& _Al, _Types&&... _Args) {
    return _STD apply(
        [&](auto&&... _Construct_args) {
            return _STD construct_at(_Ptr, _STD forward<decltype(_Construct_args)>(_Construct_args)...);
        },
        _STD uses_allocator_construction_args<_Ty>(_Al, _STD forward<_Types>(_Args)...));
}
#endif // _HAS_CXX20
_RPC_END

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#else
#pragma pop_macro("new")
_STL_RESTORE_CLANG_WARNINGS
#pragma warning(pop)
#pragma pack(pop)
#endif
#endif // _STL_COMPILER_PREPROCESSOR
#endif // _XMEMORY_
