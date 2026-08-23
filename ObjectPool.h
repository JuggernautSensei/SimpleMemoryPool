#pragma once
#include <memory>
#include <utility>

#include "Base.h"
#include "FixedBlockPool.h"

namespace jug
{

// =========================================================
//  Object Pool
//   FixedBlockPool 에 대한 얇은 wrapper.
//   생성자 인자를 그대로 전달해 객체를 생성/소멸시켜준다.
//   실사용에서는 FixedBlockPool 보다 ObjectPool 쪽 활용성이 좋다.
// =========================================================

template<typename T>
class ObjectPool
{
public:
    explicit ObjectPool(
        const size_t _numObjectPerPool = 1024)
        : m_pool(sizeof(T), _numObjectPerPool, alignof(T))
    {
    }

    template<typename... TArgs>
    T* New(
        TArgs&&... _args)
    {
        T* pObj = static_cast<T*>(m_pool.Alloc());
        return std::construct_at(pObj, std::forward<TArgs>(_args)...);
    }

    void Delete(
        T* const _pObj)
    {
        if (_pObj)
        {
            std::destroy_at(_pObj);
            m_pool.Free(_pObj);
        }
    }

private:
    FixedBlockPool m_pool;
};

}   // namespace jug
