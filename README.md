# jug-memory

C++로 짠 간단한 고정길이 블록 메모리 풀 구현. 헤더 파일만 있으면 바로 쓸 수 있다.

- `Base.h` — 정렬(align) 유틸, 정렬된 할당/해제 래퍼. 다른 파일들이 공통으로 참조하는 최소 기반.
- `FixedBlockPool.h` — 고정 크기 블록을 미리 잘라놓고 재사용하는 메모리 풀 본체.
- `ObjectPool.h` — `FixedBlockPool`을 감싼 타입 안전 래퍼. 생성자 인자를 그대로 전달받아 `New`/`Delete`로 객체를 생성·소멸시킨다.
- `MemoryArena.h` — 해제를 염두에 두지 않고 계속 앞으로만 할당하는 메모리 아레나. 제한된 스코프에서 힙 할당을 최소화할 때 쓴다.

## 요구 사항

- C++20 이상 (`std::construct_at`, `std::destroy_at` 사용)
- 표준 라이브러리 외 의존성 없음

## 사용법

파일을 그대로 복사해서 include 하면 된다.

```cpp
#include "FixedBlockPool.h"

jug::FixedBlockPool pool(32, 127);   // 블록 32byte, 풀당 127블록

void* p = pool.Alloc();
// ... p 사용 ...
pool.Free(p);
```

```cpp
#include "ObjectPool.h"

struct Foo { int x; Foo(int x) : x(x) {} };

jug::ObjectPool<Foo> pool(64);       // Foo 64개 단위로 풀 생성

Foo* pFoo = pool.New(42);
// ... pFoo 사용 ...
pool.Delete(pFoo);
```

```cpp
#include "MemoryArena.h"

jug::MemoryArena arena(4096);        // 4KB 단위 아레나

void* p1 = arena.Alloc(64);
void* p2 = arena.AlignedAlloc(128, 16);
// 개별 해제 없음, arena 소멸 시 전체 해제
```

## 왜 만들었나

힙 할당(`malloc`/`new`)은 스택 할당에 비해 오버헤드가 크다. 스택은 포인터만 옮기면 끝나지만, 힙은 파편화된 메모리에서 적절한 블록을 찾는 과정이 추가로 들어간다.

이 문제를 완화하려고 만든 게 메모리 풀이다. 미리 큰 메모리를 할당해두고 잘라서 쓰면, 할당은 포인터 전진 한 번, 해제는 free list에 다시 연결하는 것으로 끝난다. 자세한 구현 설명은 블로그 글([메모리 풀과 간단한 구현](https://app.notion.com/p/3c5c3fb6c02f8041b2d5e177d24cab3d))에 정리했다.

## 벤치마크

`FixedBlockPool` vs `malloc`/`free`, 블록 32byte / 풀 4080byte(헤더 16byte + 블록 127개) 기준.

| 시나리오 | malloc/free | FixedBlockPool | 배율 |
|---|---:|---:|---:|
| 순수 할당 200만 회 후 일괄 해제 | 144.851 ms | 18.704 ms | **7.74배** |
| 할당 즉시 해제 반복 500만 회 | 157.437 ms | 2.950 ms | **53.38배** |
| 랜덤 할당/해제 뒤섞기 (동시 생존 1만개) | 145.569 ms | 47.004 ms | **3.10배** |
| 힙 파편화 + 프레임 단위 생성/즉시소멸 | 136.961 ms | 6.558 ms | **20.88배** |

같은 크기 객체를 짧은 생명주기로 반복 생성/파괴하는 패턴에서 이득이 가장 크다. 크기가 뒤섞이고 생존 기간이 제각각인 랜덤 워크로드에서는 격차가 줄어든다.

측정 환경: MSVC, x64 Release.

## 한계

최소한의 구현이라 아래 상황은 대응하지 못한다. 전부 사용자가 책임져야 한다.

1. 풀에 속하지 않은 메모리 반납
2. 더블 프리
3. 멀티스레드 환경에서의 동기화 문제

## 라이선스

MIT
"# Simple_Implemented_Memory_Pool" 
