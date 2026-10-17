# SimpleMemoryPool

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)
![Header-only](https://img.shields.io/badge/header--only-library-green)

고정 크기 블록의 할당과 재사용을 다루는 C++20 헤더 전용 구현입니다. 저수준 메모리 관리에 대한 이해를 점검하고, 메모리 정렬을 직접 적용해 보기 위해 만들었습니다.

## 목차

- [소개](#소개)
- [사용 예제](#사용-예제)
- [적용 방법](#적용-방법)
- [성능 비교](#성능-비교)
- [문서](#문서)

## 소개

같은 크기의 메모리를 반복해서 사용한다면, 큰 메모리 영역을 확보한 뒤 블록으로 나누어 쓸 수 있습니다. 사용이 끝난 블록을 모아 두었다가 다음 요청에 다시 내어주는 것이 이 구현의 기본 동작입니다.

- **[`FixedBlockPool`](FixedBlockPool.h)** — 같은 크기의 블록을 할당하고 반납합니다. 첫 요청에서 메모리를 확보하고, 공간이 부족하면 새 풀을 연결합니다.
- **[`ObjectPool<T>`](ObjectPool.h)** — `FixedBlockPool`에 객체의 생성과 소멸을 연결합니다. `New()`로 생성하고 `Delete()`로 소멸·반납합니다.
- **[`MemoryArena`](MemoryArena.h)** — 요청한 크기만큼 순차적으로 할당합니다. 개별 반납 없이 `Reset()` 또는 소멸 시 메모리를 한꺼번에 해제합니다. PBRT에서 본 구조를 참고해 만들었습니다.

`FixedBlockPool::Free()`는 메모리를 기반 할당자에 돌려주지 않습니다. 반납한 블록 안에 다음 빈 블록의 주소를 기록해 free list로 연결하고, 다음 할당에서 가장 최근에 반납한 블록부터 재사용합니다.

정렬은 처음 확보한 주소뿐 아니라 이후 블록의 주소에도 적용합니다. 풀 헤더 뒤에 패딩을 두고 블록 크기도 정렬 단위로 올림해서, 다음 블록으로 이동해도 정렬 조건이 유지되도록 했습니다. 메모리 배치와 계산 과정은 [원문](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)에서 이어서 볼 수 있습니다.

## 사용 예제

`ObjectPool`은 생성자 인자를 그대로 전달합니다. 아래 예제는 객체 하나를 생성해 사용한 뒤 소멸시키고, 그 메모리를 풀에 반납합니다.

```cpp
#include <cstdio>
#include "ObjectPool.h"

struct alignas(void*) Particle
{
    float x;
    float y;

    Particle(float _x, float _y) : x(_x), y(_y) {}
};

int main()
{
    jug::ObjectPool<Particle> particles(128);

    Particle* particle = particles.New(1.0f, 2.0f);
    std::printf("%.1f, %.1f\n", particle->x, particle->y);
    particles.Delete(particle);
}
```

현재 구현은 빈 블록 안에 포인터를 기록하므로 타입의 크기와 정렬이 포인터를 담기에 충분해야 합니다. 예제의 `alignas(void*)`는 이 조건을 맞추기 위한 것입니다. `ObjectPool<T>`가 작은 타입의 크기·정렬을 자동으로 보정하지는 않습니다.

객체 풀은 살아 있는 객체 목록을 추적하지 않습니다. 생성한 객체는 풀 소멸 전에 `Delete()`해야 하며, 풀 자체의 소멸만으로 남은 객체의 소멸자가 호출되지는 않습니다.

## 적용 방법

필요한 헤더를 포함하고 저장소 루트를 헤더 탐색 경로에 추가하면 됩니다. 외부 라이브러리나 별도의 링크 과정은 필요하지 않습니다. 공통 정렬 계산과 플랫폼별 메모리 할당은 [`Base.h`](Base.h)에 있습니다.

요구 사항은 **C++20 이상**과 MSVC 또는 `posix_memalign()`을 지원하는 POSIX 환경입니다. MSVC에서는 `_aligned_malloc/_aligned_free`, 그 외에는 `posix_memalign/free`를 사용합니다.

위 예제를 저장소 루트에 `main.cpp`로 저장했다면, Windows의 **x64 Native Tools Command Prompt for VS**에서 다음과 같이 실행할 수 있습니다.

```bat
cl /std:c++20 /EHsc /W4 /I . main.cpp /Fe:example.exe
example.exe
```

Linux/GCC에서는 다음 명령을 사용합니다.

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -I . main.cpp -o example
./example
```

### 사용 조건

동작 원리를 설명하기 위한 최소 구현입니다. 사용 시 다음 조건을 지켜야 합니다.

- 블록은 포인터를 저장할 수 있는 크기·정렬이어야 합니다. 정렬은 2의 거듭제곱, 풀당 블록 수는 양수여야 하며 크기 계산이 오버플로하지 않아야 합니다.
- 할당받은 블록은 해당 풀에 한 번만 반납해야 하며, 반납한 블록의 데이터는 사용할 수 없습니다. 잘못된 포인터와 중복 반납을 검사하지 않습니다.
- 아레나의 단일 요청은 헤더·정렬 패딩을 포함해 내부 풀 하나에 들어가야 합니다.
- 내부 동기화가 없으므로 같은 인스턴스에 대한 동시 접근은 외부에서 동기화해야 합니다.

`Reset()`이나 풀 소멸 이후에는 기존 포인터를 사용할 수 없습니다. 원시 메모리에 객체를 직접 생성했다면 필요한 소멸자도 메모리 해제 전에 호출해야 합니다. 기반 메모리 할당 실패의 복구와 객체 생성자 예외 시 블록 반환은 구현하지 않았습니다.

## 성능 비교

할당 패턴을 바꾸어 가며 `malloc/free`와 비교했습니다. 아래는 MSVC x64 Release, 32바이트 블록, 풀당 127블록으로 측정해 포스팅에 기록한 결과입니다.

| 할당 패턴 | malloc/free | FixedBlockPool | 배율 |
| --- | ---: | ---: | ---: |
| 200만 회 할당 후 일괄 해제 | 144.851 ms | 18.704 ms | 7.74배 |
| 할당 직후 해제 500만 회 반복 | 157.437 ms | 2.950 ms | 53.38배 |
| 랜덤 할당·해제 혼합, 동시 생존 1만 개 | 145.569 ms | 47.004 ms | 3.10배 |
| 사전 힙 파편화 + 프레임 단위 생성·즉시 소멸 | 136.961 ms | 6.558 ms | 20.88배 |

배율은 `malloc/free`의 측정 시간을 `FixedBlockPool`의 측정 시간으로 나눈 값입니다. 자세한 시나리오와 결과 해석은 [포스팅](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)에 정리했습니다.

벤치마크 소스는 현재 저장소에 포함되어 있지 않습니다. 위 수치는 당시 환경의 결과이며, 할당 패턴과 실행 환경에 따라 달라집니다.

## 문서

[**메모리 풀에 대한 기본적인 개념과 구현**](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)

`FixedBlockPool`의 멤버와 `Alloc()`·`Free()`를 차례로 살펴보고, 헤더와 블록의 정렬을 계산하는 과정을 설명한 글입니다. `ObjectPool`과 `MemoryArena`에 대한 설명도 함께 담았습니다.
