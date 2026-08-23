#pragma once
#include <new>

#include "Base.h"

namespace jug
{

// =========================================================
//  MemoryArena
//   메모리 해제없이 빠른 메모리 할당을 위해 사용되는 메모리 풀
//   영속적으로 사용하는 개념 아님
//   제한된 스코프에서 힙할당을 최소화하기 위해 사용
// =========================================================

class MemoryArena
{
    struct LINK
    {
        LINK* pLink;
    };

public:
    explicit MemoryArena(
        const size_t _poolSize)
        : m_poolSize(AlignUp(_poolSize, alignof(std::max_align_t)))
    {
        JUG_ASSERT(m_poolSize > 0, "_poolSize must be greater than 0");
    }

    ~MemoryArena()
    {
        Reset();
    }

    MemoryArena(const MemoryArena&) = delete;

    MemoryArena(
        MemoryArena&& _other) noexcept
        : m_pPoolList(_other.m_pPoolList)
        , m_pCur(_other.m_pCur)
        , m_pEnd(_other.m_pEnd)
        , m_poolSize(_other.m_poolSize)
    {
        _other.m_pPoolList = nullptr;
        _other.m_pCur      = nullptr;
        _other.m_pEnd      = nullptr;
    }

    MemoryArena& operator=(const MemoryArena& _other) = delete;

    MemoryArena& operator=(
        MemoryArena&& _other) noexcept
    {
        if (this != &_other)
        {
            Reset();
            m_pPoolList        = _other.m_pPoolList;
            m_pCur             = _other.m_pCur;
            m_pEnd             = _other.m_pEnd;
            m_poolSize         = _other.m_poolSize;
            _other.m_pPoolList = nullptr;
            _other.m_pCur      = nullptr;
            _other.m_pEnd      = nullptr;
        }
        return *this;
    }

    void* Alloc(
        const size_t _size)
    {
        return AlignedAlloc(_size, alignof(std::max_align_t));
    }

    void* AlignedAlloc(
        const size_t _size,
        const size_t _alignment)
    {
        JUG_ASSERT(IsPowerOf2(_alignment), "_alignment must be power of 2");
        JUG_ASSERT(sizeof(LINK) + (_alignment - 1) + _size <= m_poolSize, "MemoryArena pool size is too small for the requested allocation size and alignment.\n");

        std::byte* pAlignedCur = m_pCur ? AlignUp(m_pCur, _alignment) : nullptr;
        if (!m_pPoolList || pAlignedCur + _size > m_pEnd)
        {
            LINK* pHeader  = static_cast<LINK*>(jug::Alloc(m_poolSize));
            pHeader->pLink = m_pPoolList;
            m_pPoolList    = pHeader;
            m_pCur         = reinterpret_cast<std::byte*>(pHeader) + sizeof(LINK);
            m_pEnd         = reinterpret_cast<std::byte*>(pHeader) + m_poolSize;
            pAlignedCur    = AlignUp(m_pCur, _alignment);
        }

        m_pCur = pAlignedCur + _size;
        return pAlignedCur;
    }

    void Reset()
    {
        LINK* pPool = m_pPoolList;
        while (pPool)
        {
            LINK* pNext = pPool->pLink;
            jug::Free(pPool);
            pPool = pNext;
        }

        m_pPoolList = nullptr;
        m_pCur      = nullptr;
        m_pEnd      = nullptr;
    }

private:
    LINK*      m_pPoolList = nullptr;   // 풀 리스트
    std::byte* m_pCur      = nullptr;
    std::byte* m_pEnd      = nullptr;
    size_t     m_poolSize  = 0;
};

}   // namespace jug
