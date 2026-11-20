<h1 align="center">SimpleMemoryPool</h1>

<p align="center">C++20으로 만든 헤더 전용 메모리 풀</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/header--only-library-green" alt="Header-only">
</p>

메모리 풀의 할당·반납·재사용을 살펴보기 위한 C++20 헤더 전용 구현입니다. 저수준 메모리 관리에 대한 이해를 점검하고, 메모리를 확보할 때부터 다음 블록의 위치를 계산할 때까지 정렬을 직접 적용해 보려고 작성했습니다.

## Contents

- [소개](#소개)
- [사용 예제](#사용-예제)
- [성능 비교](#성능-비교)
- [문서](#문서)

## 소개

같은 크기의 메모리를 반복해서 사용한다면, 큰 메모리 영역을 확보한 뒤 블록으로 나누어 쓸 수 있습니다. 사용이 끝난 블록은 풀에 반납하고 다음 요청에서 다시 사용합니다. 이 과정을 직접 따라가 볼 수 있도록 고정 크기 블록 풀과 이를 활용한 객체 풀, 순차 할당용 아레나를 담았습니다.

- [`FixedBlockPool`](FixedBlockPool.h): 같은 크기의 블록을 할당하고 반납합니다. 첫 할당 요청에서 풀 메모리를 확보하며, 반납된 블록이 없고 현재 풀도 다 썼다면 새 풀을 연결합니다.
- [`ObjectPool<T>`](ObjectPool.h): `FixedBlockPool`에 객체 생성·소멸을 연결한 클래스입니다. `New()`는 생성자 인자를 전달해 객체를 만들고, `Delete()`는 소멸자를 호출한 뒤 블록을 반납합니다.
- [`MemoryArena`](MemoryArena.h): 요청한 크기만큼 메모리를 순차적으로 할당합니다. 개별 반납 없이 `Reset()` 또는 소멸 시 확보한 풀을 모두 해제합니다. PBRT에서 본 구조를 참고해 만들었습니다.

`FixedBlockPool::Free()`는 반납된 블록의 앞부분에 다음 빈 블록의 주소를 기록합니다. 이 연결을 빈 블록 목록인 free list로 관리하고, `Alloc()`에서 가장 최근에 반납한 블록부터 꺼내 씁니다. 블록 자체를 목록의 노드로 사용하므로 연결 정보를 위한 별도 할당이 필요하지 않습니다. 풀의 메모리는 `Reset()`이나 소멸 시 해제합니다.

정렬은 풀을 확보할 때와 블록을 나눌 때 모두 고려했습니다. 헤더 뒤에는 첫 블록의 정렬을 맞추는 패딩을 두고, 블록 간 간격도 정렬 단위의 배수로 맞춥니다. 따라서 커서를 다음 블록으로 옮겨도 정렬 조건이 유지됩니다. 구체적인 메모리 배치와 계산 과정은 [포스팅](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)에서 설명합니다.

## 사용 예제

헤더 전용이라 저장소의 헤더를 프로젝트에 추가하고 `ObjectPool.h`를 include하면 됩니다. `ObjectPool`을 사용하면 메모리 확보와 함께 객체를 생성할 수 있습니다. 아래 예제는 `Particle`을 하나 생성해 사용한 뒤, 소멸시키고 그 블록을 풀에 반납합니다.

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

빈 블록에는 연결 포인터를 기록하므로 포인터가 들어갈 크기와 정렬이 필요합니다. 예제의 `alignas(void*)`는 이 조건을 맞추기 위한 것입니다. `ObjectPool<T>`는 `sizeof(T)`와 `alignof(T)`를 그대로 기반 풀에 전달하며, 작은 타입의 크기나 정렬을 자동으로 보정하지 않습니다.

`New()`로 만든 객체는 풀 소멸 전에 `Delete()`해야 합니다. 객체 풀은 살아 있는 객체 목록을 추적하지 않으므로, 풀을 소멸시켜도 남은 객체의 소멸자가 자동으로 호출되지는 않습니다.

## 성능 비교

할당 패턴을 바꾸어 가며 `malloc/free`와 비교했습니다. 아래는 포스팅에 기록한 기존 측정 결과입니다.

- 컴파일 환경: MSVC, x64 Release
- 블록 크기: 32바이트
- 풀당 블록 수: 127개

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
      <td align="left">200만 회 할당 후 일괄 해제</td>
      <td align="right">144.851 ms</td>
      <td align="right">18.704 ms</td>
      <td align="right">7.74배</td>
    </tr>
    <tr>
      <td align="left">할당 직후 해제 500만 회 반복</td>
      <td align="right">157.437 ms</td>
      <td align="right">2.950 ms</td>
      <td align="right">53.38배</td>
    </tr>
    <tr>
      <td align="left">랜덤 할당·해제 혼합, 동시 생존 1만 개</td>
      <td align="right">145.569 ms</td>
      <td align="right">47.004 ms</td>
      <td align="right">3.10배</td>
    </tr>
    <tr>
      <td align="left">사전 힙 파편화 + 프레임 단위 생성·즉시 소멸</td>
      <td align="right">136.961 ms</td>
      <td align="right">6.558 ms</td>
      <td align="right">20.88배</td>
    </tr>
  </tbody>
</table>

배율은 `malloc/free`의 측정 시간을 `FixedBlockPool`의 측정 시간으로 나눈 값입니다. 상세 시나리오와 결과에 대한 해석은 [포스팅](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)에 정리했습니다.

## 문서

메모리 풀의 기본 개념부터 `FixedBlockPool`의 멤버와 할당·반납 코드, 정렬 계산을 차례로 살펴본 글입니다. `ObjectPool`과 `MemoryArena`에 대한 설명도 함께 담았습니다.

[메모리 풀에 대한 기본적인 개념과 구현](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)
