<h1 align="center">SimpleMemoryPool</h1>

<p align="center">C++20으로 만든 헤더 전용 메모리 풀</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/header--only-library-green" alt="Header-only">
</p>

## Contents

* [프로젝트 소개](#프로젝트-소개)
* [블록 안에 둔 빈 블록 목록](#블록-안에-둔-빈-블록-목록)
* [정렬](#정렬)
* [malloc과의 비교](#malloc과의-비교)
* [빌드 및 사용](#빌드-및-사용)

## 프로젝트 소개

같은 크기의 메모리를 자주 할당하고 해제할 때 쓰는 메모리 풀입니다. 큰 메모리 영역을 한 번에 확보해 블록으로 나누고, 반납된 블록을 다음 요청에 다시 내줍니다. 세 가지 클래스로 구성됩니다.

* [`FixedBlockPool`](FixedBlockPool.h): 같은 크기의 블록을 할당하고 반납합니다.
* [`ObjectPool<T>`](ObjectPool.h): `FixedBlockPool` 위에서 객체를 생성하고 소멸시킵니다. `New()`는 생성자 인자를 그대로 전달하고, `Delete()`는 소멸자를 호출한 뒤 블록을 반납합니다.
* [`MemoryArena`](MemoryArena.h): 크기가 다른 데이터를 순서대로 할당하고, 개별 반납 없이 `Reset()`이나 소멸 시점에 한꺼번에 해제합니다.

```cpp
#include "ObjectPool.h"

struct alignas(void*) Particle
{
    float x;
    float y;

    Particle(float _x, float _y) : x(_x), y(_y) {}
};

jug::ObjectPool<Particle> particles(128);   // 풀 하나에 128개

Particle* particle = particles.New(1.0f, 2.0f);
particles.Delete(particle);
```

풀이 살아 있는 객체를 추적하지 않으므로, `New()`로 만든 객체는 풀이 소멸하기 전에 `Delete()`해야 합니다. 더블 프리, 다른 풀의 메모리 반납, 여러 스레드에서의 동시 접근도 검사하지 않습니다.

## 블록 안에 둔 빈 블록 목록

반납된 블록을 다시 쓰려면 어떤 블록이 비어 있는지 기록해야 합니다. 이 기록을 별도의 배열이나 노드로 두면 기록을 위한 할당이 다시 생깁니다. 그래서 반납된 블록의 앞부분에 다음 빈 블록의 주소를 써서, 빈 블록끼리 연결 리스트를 이루게 했습니다. 빈 블록은 어차피 쓰이지 않는 메모리이므로 추가 공간이 들지 않습니다.

```cpp
void Free(void* _pMem)
{
    if (_pMem)
    {
        LINK* pBlock  = static_cast<LINK*>(_pMem);
        pBlock->pLink = m_pFreeList;     // 빈 블록에 다음 빈 블록 주소를 기록
        m_pFreeList   = pBlock;
    }
}

void* Alloc()
{
    if (m_pFreeList)                 // 가장 최근에 반납한 블록부터 재사용
    {
        LINK* pBlock = m_pFreeList;
        m_pFreeList  = m_pFreeList->pLink;
        return pBlock;
    }
    // 빈 블록이 없으면 현재 풀에서 다음 블록을 잘라 내고,
    // 풀을 다 썼으면 새 풀을 확보해 풀 목록에 연결합니다.
    ...
}
```

할당과 반납은 포인터 두세 개를 바꾸는 것으로 끝납니다. 풀들도 같은 방식으로 각 풀의 첫머리에 다음 풀의 주소를 두어 연결하고, `Reset()`은 이 목록을 따라가며 풀을 해제합니다.

이 구조 때문에 블록은 포인터 하나를 담을 수 있어야 합니다. `ObjectPool<T>`는 `sizeof(T)`와 `alignof(T)`를 그대로 사용하므로, 위 예제의 `Particle`처럼 포인터보다 정렬이 작은 타입에는 `alignas(void*)`를 붙여야 합니다.

## 정렬

```text
| 풀 헤더 + 패딩 | 블록 0 | 블록 1 | ... | 블록 N-1 |
```

블록 주소가 요청한 정렬을 지키도록 세 군데에서 맞춥니다. 풀은 `_aligned_malloc`(POSIX에서는 `posix_memalign`)으로 블록 정렬에 맞춰 확보합니다. 풀 헤더 뒤에는 헤더 크기를 정렬 단위로 올린 만큼 패딩을 두어 첫 블록을 정렬합니다. 블록 크기도 생성자에서 정렬 단위의 배수로 올려 두므로, 커서를 블록 크기만큼 옮긴 다음 블록도 정렬된 주소에 놓입니다. 할당할 때마다 주소를 다시 계산할 필요가 없습니다.

`MemoryArena`는 요청마다 크기와 정렬이 다르므로, 할당할 때 현재 위치를 요청한 정렬로 올린 뒤 그 자리를 반환합니다. 남은 공간이 부족하면 새 풀을 확보하고, 이전 풀의 데이터는 그대로 둡니다.

## malloc과의 비교

할당 패턴 네 가지에서 `malloc`/`free`와 `FixedBlockPool`의 실행 시간을 비교했습니다. MSVC x64 Release로 빌드했고, 블록 크기는 32바이트, 풀당 블록 수는 127개입니다.

<table width="100%">
  <thead>
    <tr>
      <th align="left">할당 패턴</th>
      <th align="right">malloc/free</th>
      <th align="right">FixedBlockPool</th>
      <th align="right">배율</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>200만 회 할당 후 일괄 해제</td>
      <td align="right">144.851 ms</td>
      <td align="right">18.704 ms</td>
      <td align="right">7.74배</td>
    </tr>
    <tr>
      <td>할당 직후 해제를 500만 회 반복</td>
      <td align="right">157.437 ms</td>
      <td align="right">2.950 ms</td>
      <td align="right">53.38배</td>
    </tr>
    <tr>
      <td>무작위 할당·해제, 동시에 1만 개 유지</td>
      <td align="right">145.569 ms</td>
      <td align="right">47.004 ms</td>
      <td align="right">3.10배</td>
    </tr>
    <tr>
      <td>힙을 미리 파편화한 뒤 프레임마다 생성·즉시 소멸</td>
      <td align="right">136.961 ms</td>
      <td align="right">6.558 ms</td>
      <td align="right">20.88배</td>
    </tr>
  </tbody>
</table>

차이가 가장 큰 경우는 할당 직후 해제를 반복할 때입니다. 방금 반납한 블록이 빈 블록 목록의 맨 앞에 있으므로, 매번 같은 블록을 포인터 교환만으로 주고받습니다. 200만 회 할당에서는 블록 127개마다 풀을 새로 확보하므로, 실제 시스템 할당은 약 1만 5,700번입니다. 측정 시나리오와 결과 해석은 [블로그 글](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)에 정리했습니다.

## 빌드 및 사용

헤더 전용이라 별도 빌드가 없습니다. 네 개의 헤더를 프로젝트에 추가하고 `ObjectPool.h`, `FixedBlockPool.h`, `MemoryArena.h` 중 필요한 것을 include합니다. C++20(`std::construct_at`)이 필요하며 외부 의존성은 없습니다.
