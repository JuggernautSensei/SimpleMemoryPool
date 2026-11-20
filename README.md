<h1 align="center">SimpleMemoryPool</h1>

<p align="center">C++20으로 만든 헤더 전용 메모리 풀</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/header--only-library-green" alt="Header-only">
</p>

## Contents

- [프로젝트 소개](#프로젝트-소개)
- [고정 크기 블록 재사용](#고정-크기-블록-재사용)
- [순차 할당과 일괄 해제](#순차-할당과-일괄-해제)
- [성능 비교](#성능-비교)
- [빌드 및 실행](#빌드-및-실행)

## 프로젝트 소개

크기가 같은 객체를 만들고 지우는 작업에는 고정 크기 블록 풀과 객체 풀을 쓸 수 있습니다. 객체 풀은 생성자에 인자를 전달해 객체를 만들고, 사용이 끝나면 소멸자를 호출해 그 자리를 다음 객체에 돌려줍니다. 크기가 다른 임시 데이터를 한꺼번에 정리할 때는 메모리 아레나를 사용합니다.

## 고정 크기 블록 재사용

`FixedBlockPool::Free()`는 반납된 블록의 앞부분에 다음 빈 블록의 주소를 기록합니다. 이 연결을 빈 블록 목록인 free list로 관리하고, `Alloc()`에서 가장 최근에 반납한 블록부터 꺼내 씁니다. 블록 자체를 목록의 노드로 사용하므로 연결 정보를 위한 별도 할당이 필요하지 않습니다. 풀의 메모리는 `Reset()`이나 소멸 시 해제합니다.

정렬은 풀을 확보할 때와 블록을 나눌 때 모두 고려했습니다. 헤더 뒤에는 첫 블록의 정렬을 맞추는 패딩을 두고, 블록 간 간격도 정렬 단위의 배수로 맞춥니다. 따라서 커서를 다음 블록으로 옮겨도 정렬 조건이 유지됩니다. 구체적인 메모리 배치와 계산 과정은 [포스팅](https://juggernautsensei.notion.site/3c5c3fb6c02f8041b2d5e177d24cab3d?source=copy_link)에서 설명합니다.

## 순차 할당과 일괄 해제

한 작업에서 크기가 다른 임시 데이터를 여러 번 할당하고 마지막에 함께 해제한다면, 각 데이터가 차지한 자리를 따로 반납할 필요가 없습니다. 메모리 아레나는 첫 요청에서 풀 하나를 확보합니다. 이후에는 요청한 정렬에 맞춰 시작 주소를 잡고, 요청한 크기만큼 다음 할당 위치를 옮깁니다.

현재 풀에 공간이 부족하면 새 풀을 확보해 기존 풀과 연결합니다. 이전 풀에 할당한 데이터는 그 자리에 남습니다. `Reset()`을 호출하거나 아레나가 소멸하면 연결된 풀을 차례로 해제합니다.

## 성능 비교

할당 패턴을 바꾸어 가며 `malloc/free`와 비교했습니다. 아래는 포스팅에 기록한 기존 측정 결과입니다.

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
      <td align="right">53.37배</td>
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

## 빌드 및 실행

C++20 컴파일러가 필요합니다. 다음은 Windows의 Visual Studio 개발자 명령 프롬프트에서 실행하는 예입니다.

1. 저장소의 헤더가 있는 폴더에 다음 예제를 `main.cpp`로 저장합니다.

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

2. 같은 폴더에서 예제를 빌드하고 실행합니다.

   ```cmd
   cl /nologo /std:c++20 /EHsc main.cpp
   main.exe
   ```

빈 블록에는 연결 포인터를 기록하므로 포인터가 들어갈 크기와 정렬이 필요합니다. 예제의 `alignas(void*)`는 이 조건을 맞추기 위한 것입니다. `ObjectPool<T>`는 `sizeof(T)`와 `alignof(T)`를 그대로 기반 풀에 전달하며, 작은 타입의 크기나 정렬을 자동으로 보정하지 않습니다.

`New()`로 만든 객체는 풀 소멸 전에 `Delete()`해야 합니다. 객체 풀은 살아 있는 객체 목록을 추적하지 않으므로, 풀을 소멸시켜도 남은 객체의 소멸자가 자동으로 호출되지는 않습니다.
