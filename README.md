# SimpleMemoryPool

**C++20 헤더 전용 메모리 풀 라이브러리**

고정 크기 블록의 할당·재사용, 타입별 객체 생성·소멸, 스코프 단위 메모리 할당을 제공합니다. 네 개의 헤더로 구성되어 있으며, 외부 라이브러리나 별도 링크 과정 없이 `jug` 네임스페이스에서 사용합니다.

[주요 기능](#주요-기능) · [설계](#설계) · [시작하기](#시작하기) · [벤치마크](#벤치마크) · [빌드](#빌드) · [사용 시 주의사항](#사용-시-주의사항)

## 주요 기능

| 헤더 | 기능 |
| --- | --- |
| [Base.h](Base.h) | 2의 거듭제곱 검사, 주소·크기 정렬, 정렬된 메모리 할당·해제 |
| [FixedBlockPool.h](FixedBlockPool.h) | 고정 크기 블록 할당, free list를 통한 블록 재사용, 풀 확장·일괄 해제 |
| [ObjectPool.h](ObjectPool.h) | `FixedBlockPool`을 감싼 타입별 객체 풀, 생성자 인자 전달, 명시적 생성·소멸 |
| [MemoryArena.h](MemoryArena.h) | 포인터를 전진시키는 순차 할당, 요청별 정렬, 스코프 종료 시 일괄 해제 |

## 설계

### FixedBlockPool — 같은 크기의 블록을 재사용합니다

블록 크기·개수·정렬을 생성 시 지정합니다. 실제 메모리는 첫 `Alloc()` 호출 시 확보하며, 현재 풀이 가득 차면 같은 크기의 풀을 추가합니다. 블록 크기는 지정한 정렬의 배수로 올림합니다.

```text
풀 하나: [다음 풀 포인터 + 정렬 패딩][블록 0][블록 1] ... [블록 N-1]
```

`Free()`는 반납한 블록 안에 다음 빈 블록의 포인터를 기록합니다. 이후 `Alloc()`은 이 free list를 먼저 사용하고, 빈 블록이 없으면 현재 풀의 포인터를 전진시킵니다. 개별 블록을 반납해도 풀 메모리는 유지하며, `Reset()` 또는 소멸 시 전체 풀을 해제합니다.

### ObjectPool — 객체의 수명과 저장 공간을 연결합니다

`ObjectPool<T>`는 `sizeof(T)`·`alignof(T)`를 블록 설정에 사용합니다. `New()`는 생성자 인자를 전달해 `std::construct_at()`으로 객체를 만들고, `Delete()`는 `std::destroy_at()`으로 소멸자를 호출한 뒤 블록을 반납합니다.

풀 자체를 소멸시킬 때 남아 있는 객체의 소멸자를 자동으로 호출하지 않습니다. 생성한 객체는 풀이 살아 있는 동안 `Delete()`로 정리해야 합니다.

### MemoryArena — 함께 버릴 데이터를 순서대로 할당합니다

`Alloc()`은 기본 정렬로, `AlignedAlloc()`은 요청한 정렬로 현재 풀에서 공간을 잘라 반환합니다. 공간이 부족하면 새 풀을 추가하며, 개별 할당의 해제는 제공하지 않습니다.

프레임 임시 데이터처럼 수명이 같은 데이터를 모아 할당하고, `Reset()` 또는 아레나 소멸 시 한 번에 해제하는 용도입니다. 한 번의 요청은 풀 하나 안에 들어가야 하며, 큰 요청에 맞춰 풀 크기를 자동으로 늘리지는 않습니다.

## 시작하기

네 개의 헤더를 같은 디렉터리에 복사하거나 저장소 루트를 프로젝트의 헤더 탐색 경로에 추가합니다. 사용하는 풀의 헤더를 포함하면 필요한 기반 헤더도 함께 포함됩니다.

### 고정 크기 블록 할당

```cpp
#include "FixedBlockPool.h"

int main()
{
    jug::FixedBlockPool pool(32, 127); // 블록당 32바이트, 풀당 127개

    void* block = pool.Alloc();
    // 최대 32바이트의 저장 공간으로 사용합니다.
    pool.Free(block);

    pool.Reset(); // 확보한 모든 풀을 해제합니다.
}
```

`FixedBlockPool`은 저장 공간만 관리합니다. 객체를 배치해 사용한다면 생성·소멸도 호출자가 처리해야 합니다.

### 타입별 객체 생성·소멸

```cpp
#include "ObjectPool.h"

struct alignas(void*) Foo
{
    int value;

    explicit Foo(int v) : value(v) {}
};

int main()
{
    jug::ObjectPool<Foo> pool(64); // 풀당 Foo 64개

    Foo* object = pool.New(42);
    // object->value를 사용합니다.
    pool.Delete(object);
}
```

현재 구현의 free list는 블록 안에 포인터를 저장합니다. 예제의 `alignas(void*)`는 작은 타입도 포인터 크기·정렬을 확보하도록 지정한 것입니다. 자세한 조건은 [사용 시 주의사항](#사용-시-주의사항)을 참고하세요.

### 임시 메모리 일괄 해제

```cpp
#include "MemoryArena.h"

int main()
{
    jug::MemoryArena arena(4096); // 풀 하나의 전체 크기: 4 KiB

    void* data = arena.Alloc(64);
    void* alignedData = arena.AlignedAlloc(128, 16);
    // data와 alignedData를 사용합니다.
    (void)data;
    (void)alignedData;

    arena.Reset(); // 이전에 반환한 포인터는 모두 무효가 됩니다.
}
```

아레나에 객체를 직접 생성했다면 `Reset()` 전에 필요한 소멸자를 호출해야 합니다. 아레나는 객체의 타입과 소멸자를 추적하지 않습니다.

## 벤치마크

`FixedBlockPool`과 `malloc`/`free`를 비교한 기존 측정 결과입니다. **MSVC, x64 Release**에서 블록당 32바이트, 풀당 127개를 사용했습니다. 풀 하나는 헤더·패딩 16바이트와 블록 4,064바이트를 합쳐 4,080바이트입니다.

| 시나리오 | malloc/free | FixedBlockPool | 배율 |
| --- | ---: | ---: | ---: |
| 순수 할당 200만 회 후 일괄 해제 | 144.851 ms | 18.704 ms | **7.74배** |
| 할당 즉시 해제 반복 500만 회 | 157.437 ms | 2.950 ms | **53.38배** |
| 랜덤 할당·해제 혼합, 동시 생존 1만 개 | 145.569 ms | 47.004 ms | **3.10배** |
| 힙 파편화 + 프레임 단위 생성·즉시 소멸 | 136.961 ms | 6.558 ms | **20.88배** |

같은 크기 객체를 짧은 수명으로 반복 생성·파괴하는 패턴에서 이득이 컸습니다. 할당과 해제가 섞이고 생존 기간이 다른 패턴에서는 차이가 줄어듭니다. 수치는 측정 환경과 사용 패턴에 따라 달라지며, 벤치마크 소스는 현재 저장소에 포함되어 있지 않습니다.

## 빌드

### 환경과 의존성

- C++20 이상: `ObjectPool`이 `std::construct_at()`·`std::destroy_at()`을 사용합니다.
- MSVC 또는 `posix_memalign()`을 제공하는 POSIX 환경의 C++ 컴파일러가 필요합니다. `Base.h`가 플랫폼별 정렬 할당·해제를 선택합니다.
- 외부 라이브러리나 별도로 빌드할 라이브러리 파일은 없습니다.

### 받기와 컴파일

```sh
git clone https://github.com/JuggernautSensei/SimpleMemoryPool.git
cd SimpleMemoryPool
```

위 예제 중 하나를 저장소 루트의 `main.cpp`에 저장한 뒤 컴파일합니다. 다른 프로젝트에서 사용한다면 `-I` 또는 `/I`에 이 저장소의 경로를 지정합니다.

**GCC / Linux**

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -I . main.cpp -o example
./example
```

**MSVC / Visual Studio 개발자 PowerShell**

```powershell
cl /std:c++20 /EHsc /W4 /I . main.cpp /Fe:example.exe
.\example.exe
```

현재 저장소에는 별도 빌드 시스템이나 테스트 실행기가 없습니다.

## 사용 시 주의사항

| 항목 | 호출자가 지켜야 할 조건 |
| --- | --- |
| 블록 크기·정렬 | `FixedBlockPool`의 정렬 후 블록 크기는 `sizeof(void*)` 이상, 블록 정렬은 `alignof(void*)` 이상이어야 합니다. `ObjectPool<T>`도 `T`가 이 조건을 만족해야 합니다. |
| 작은 객체 | 현재 구현은 위 조건을 자동으로 보정하지 않습니다. 64비트 환경에서 `int` 하나만 가진 4바이트 타입을 그대로 사용하면 반납 시 인접 객체가 손상될 수 있습니다. |
| 반납할 포인터 | 해당 풀에서 받은 포인터만 한 번 반납합니다. 다른 풀의 포인터나 이중 해제를 검사하지 않습니다. |
| 멀티스레드 | 내부 동기화가 없으므로 같은 풀을 여러 스레드에서 사용하면 외부 동기화가 필요합니다. |
| 아레나 요청 크기 | 풀 크기에 내부 헤더·정렬 패딩·요청 크기가 모두 들어가야 합니다. |
| 포인터 수명 | `Reset()` 또는 풀 소멸 후에는 기존 포인터를 사용할 수 없습니다. |
| 객체 소멸 | `ObjectPool`은 남아 있는 객체를 자동으로 소멸시키지 않으며, `MemoryArena`도 객체 소멸자를 호출하지 않습니다. |

정렬 인자는 2의 거듭제곱이어야 하며, 블록 크기·개수와 아레나 풀 크기는 양수여야 합니다. 일부 전제조건은 `JUG_ASSERT`로 검사합니다. 기본 정의는 표준 `assert`이며, 비활성화된 빌드에서도 호출 조건을 지켜야 합니다. 메모리 할당 실패에 대한 복구 처리도 제공하지 않습니다.

## 참고 자료

- [메모리 풀과 간단한 구현](https://purple-jingle-7d5.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link) — 구현 배경과 메모리 풀의 동작 원리를 정리한 글입니다.

## 라이선스

MIT
