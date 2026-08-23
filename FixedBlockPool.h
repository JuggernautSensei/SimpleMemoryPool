#pragma once
#include <new>

#include "Base.h"

namespace jug
{

// =========================================================
//  Fixed Block Pool
//   고정 사이즈 블록 메모리 풀
//   최소한의 구현이기에 대부분의 메모리 관련 문제에 대응하지 못함
//   1. 풀에 속하지 않은 메모리 반납
//   2. 더블 프리
//   3. 멀티스레드 환경에서의 동기화 문제
//   -> 전부 사용자가 책임져야함
// =========================================================

class FixedBlockPool
{
    struct LINK
    {
        LINK* pLink;
    };

public:
    FixedBlockPool(
        const size_t _blockSize,
        const size_t _numBlocksPerPool,
        const size_t _blockAlign = alignof(std::max_align_t))
        : m_alignedBlockSize(AlignUp(_blockSize, _blockAlign))
        , m_blockAlign(_blockAlign)
        , m_numBlocksPerPool(_numBlocksPerPool)
    {
        JUG_ASSERT(IsPowerOf2(_blockAlign), "_blockAlign must be power of 2");
        JUG_ASSERT(_numBlocksPerPool > 0, "_numBlocksPerPool must be greater than 0");
        JUG_ASSERT(m_alignedBlockSize > 0, "_blockSize must be greater than 0");
    }

    ~FixedBlockPool()
    {
        Reset();
    }

    FixedBlockPool(const FixedBlockPool&) = delete;

    FixedBlockPool(FixedBlockPool&& _other) noexcept
        : m_pPoolList(_other.m_pPoolList)
        , m_pFreeList(_other.m_pFreeList)
        , m_pEnd(_other.m_pEnd)
        , m_pCur(_other.m_pCur)
        , m_alignedBlockSize(_other.m_alignedBlockSize)
        , m_blockAlign(_other.m_blockAlign)
        , m_numBlocksPerPool(_other.m_numBlocksPerPool)
    {
        _other.m_pPoolList = nullptr;
        _other.m_pFreeList = nullptr;
        _other.m_pEnd      = nullptr;
        _other.m_pCur      = nullptr;
    }

    FixedBlockPool& operator=(const FixedBlockPool&) = delete;

    FixedBlockPool& operator=(FixedBlockPool&& _other) noexcept
    {
        if (this != &_other)
        {
            Reset();
            m_pPoolList        = _other.m_pPoolList;
            m_pFreeList        = _other.m_pFreeList;
            m_pEnd             = _other.m_pEnd;
            m_pCur             = _other.m_pCur;
            m_alignedBlockSize = _other.m_alignedBlockSize;
            m_blockAlign       = _other.m_blockAlign;
            m_numBlocksPerPool = _other.m_numBlocksPerPool;
            _other.m_pPoolList = nullptr;
            _other.m_pFreeList = nullptr;
            _other.m_pEnd      = nullptr;
            _other.m_pCur      = nullptr;
        }
        return *this;
    }

    void* Alloc()
    {
        // free list 재사용
        if (m_pFreeList)
        {
            LINK* pBlock = m_pFreeList;
            m_pFreeList  = m_pFreeList->pLink;
            return pBlock;
        }

        if (!m_pPoolList || m_pCur == m_pEnd)
        {
            // | Pool Header + Padding | Block 0 | Block 1 | ... | Block N |
            const size_t poolHeaderSize = AlignUp(sizeof(LINK), m_blockAlign);
            const size_t poolSize       = poolHeaderSize + m_alignedBlockSize * m_numBlocksPerPool;
            JUG_ASSERT(IsAligned(poolSize, m_blockAlign), "poolSize must be aligned to m_blockAlign");

            void* pPool    = AlignedAlloc(poolSize, m_blockAlign);
            LINK* pHeader  = static_cast<LINK*>(pPool);
            pHeader->pLink = m_pPoolList;
            m_pPoolList    = pHeader;

            m_pCur = static_cast<std::byte*>(pPool) + poolHeaderSize;
            m_pEnd = static_cast<std::byte*>(pPool) + poolSize;
            JUG_ASSERT(IsAligned(m_pCur, m_blockAlign), "m_pCur must be aligned to m_blockAlign");
        }

        void* pMem = m_pCur;
        m_pCur += m_alignedBlockSize;
        return pMem;
    }

    void Free(
        void* _pMem)
    {
        if (_pMem)
        {
            LINK* pBlock  = static_cast<LINK*>(_pMem);
            pBlock->pLink = m_pFreeList;
            m_pFreeList   = pBlock;
        }
    }

    void Reset()
    {
        LINK* pPool = m_pPoolList;
        while (pPool)
        {
            LINK* pNext = pPool->pLink;
            AlignedFree(pPool, m_blockAlign);
            pPool = pNext;
        }

        m_pPoolList = nullptr;
        m_pFreeList = nullptr;
        m_pEnd      = nullptr;
        m_pCur      = nullptr;
    }

private:
    LINK*      m_pPoolList        = nullptr;
    LINK*      m_pFreeList        = nullptr;
    std::byte* m_pEnd             = nullptr;
    std::byte* m_pCur             = nullptr;
    size_t     m_alignedBlockSize = 0;
    size_t     m_blockAlign       = 0;
    size_t     m_numBlocksPerPool = 0;
};

}   // namespace jug
