#pragma once

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <type_traits>

#ifndef JUG_ASSERT
#define JUG_ASSERT(cond, msg) assert((cond) && (msg))
#endif

namespace jug
{

inline constexpr bool IsPowerOf2(
    const size_t _v)
{
    return _v != 0 && (_v & (_v - 1)) == 0;
}

inline constexpr size_t AlignUp(
    const size_t _size,
    const size_t _align)
{
    return (_size + (_align - 1)) & ~(_align - 1);
}

inline bool IsAligned(
    const size_t _v,
    const size_t _align)
{
    return (_v & (_align - 1)) == 0;
}

inline bool IsAligned(
    const void* _p,
    const size_t _align)
{
    return IsAligned(reinterpret_cast<size_t>(_p), _align);
}

inline std::byte* AlignUp(
    std::byte* _p,
    const size_t _align)
{
    return reinterpret_cast<std::byte*>(
        AlignUp(reinterpret_cast<size_t>(_p), _align));
}

inline void* AlignedAlloc(
    size_t _size,
    size_t _align)
{
#if defined(_MSC_VER)
    return _aligned_malloc(_size, _align);
#else
    if (_align < sizeof(void*))
    {
        _align = sizeof(void*);
    }
    void* p = nullptr;
    if (posix_memalign(&p, _align, _size) != 0)
    {
        return nullptr;
    }
    return p;
#endif
}

inline void AlignedFree(
    void* _p,
    size_t /*_align*/)
{
#if defined(_MSC_VER)
    _aligned_free(_p);
#else
    free(_p);
#endif
}

inline void* Alloc(
    const size_t _size)
{
    return AlignedAlloc(_size, alignof(std::max_align_t));
}

inline void Free(
    void* _p)
{
    AlignedFree(_p, alignof(std::max_align_t));
}

}   // namespace jug
