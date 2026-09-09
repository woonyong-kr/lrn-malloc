# lrn-malloc

제한된 힙에서 빈 블록을 찾아 할당하고, 해제된 이웃 블록을 합쳐 다시 쓰는 C 메모리 할당기다. 같은 요청을 AVL 기반 할당기와 first-fit free list에 넣어 데이터 보존, 단편화, 처리량의 차이를 비교한다.

## 실행

macOS/Linux의 C compiler·make·Python 3가 필요하다.

```sh
make setup
make demo
# 다른 유효한 CS:APP trace를 직접 재생
.build/trace-avl malloc-lab/traces/random2-bal.rep
.build/trace-list malloc-lab/traces/random2-bal.rep
```

## 입력에서 출력까지

trace의 allocate/free/reallocate → AVL 또는 first-fit free list → block 분할·병합·힙 확장 → 실제 payload pointer

기존 AVL 기반 탐색과 새 explicit first-fit free list를 같은 memlib 힙 위에서 비교한다. `mm_init`, `mm_malloc`, `mm_free`, `mm_realloc`을 제공한다. 최대 힙은 기존 20 MiB, 정렬은 8바이트다. 0 크기는 NULL, free(NULL)은 no-op, realloc(NULL,n)은 malloc, realloc(p,0)은 free다. 표현 범위를 넘는 요청은 NULL로 거절하고 원래 payload를 보존한다.

trace runner는 할당한 실제 메모리에 패턴을 채우고 해제·재할당·종료 시 확인한다. 새 영역은 모든 살아 있는 영역과 겹치지 않는지 검사한다. `--verbose`는 요청과 heap block offset·크기·사용 여부를 출력한다.

`make test`는 6개 대표 balanced trace와 seed 731의 혼합 stress를 두 엔진에 적용한다. `.build/results.json`에 결과를 쓴다. 정확성 실행과 throughput 측정은 분리한다. 최소 20회, 누적 allocator 실행 30ms 이상을 목표로 반복하며 최대 100,000회다. throughput은 trace 해석·검증 비용을 제외한 재생 속도다. 이용률은 peak requested payload / final heap bytes다.

AVL 할당기는 [`mm.c`](malloc-lab/mm.c), 비교할 free list는 [`baseline.c`](malloc-lab/baseline.c), 실제 메모리에 패턴을 쓰고 확인하는 재생기는 [`trace_runner.c`](scripts/trace_runner.c)에 있다.

## 검증과 관찰

```sh
make test
```

대표 trace와 seed가 고정된 혼합 요청을 두 구현에 적용하고, NULL·0·SIZE_MAX와 realloc의 데이터 보존을 검사한다. `make demo`는 각 요청 뒤의 블록 offset·크기·사용 여부를 출력하므로 분할·병합·재사용을 따라갈 수 있다. 끝의 JSON에서 `verified: true`, 요청한 최대 payload, 최종 힙 크기와 처리량을 확인한다.

## 지원 범위와 한계

교육용 단일 스레드 할당기이며 libc 대체, LD_PRELOAD, arena·NUMA·범용 오류 탐지기는 제공하지 않는다. 유효한 pointer에 대한 free/realloc만 계약에 포함한다. double free와 임의 pointer는 undefined behavior다.

free list baseline은 선형 탐색·이전 block 탐색, AVL은 복잡한 index 유지 비용이 있다. 구조·최소 block·확장 전략도 달라 결과를 탐색 자료구조 하나의 효과로 단정할 수 없다. 짧은 trace의 처리량은 timer와 환경 잡음에 민감하다. 특정 기계에서의 결과를 항상 AVL이 더 빠르다는 주장으로 사용하지 않는다.

## 출처와 기여

[woonyong-kr/SW_AI-W07-malloc-lab](https://github.com/woonyong-kr/SW_AI-W07-malloc-lab)에서 이어 받은 학습용 파생본이다. 기준 원본 revision은 `2a1cbe7fbc771824d55ad4816714610409fcd437`이다. 원본 과제·팀 코드와 이후 개인 확장을 구분하며, 개별 기여는 Git author와 diff로 확인한다. 기존 저작권 표시는 소스에 유지한다.

AVL 구현을 이어받아 비교용 free list와 trace 재생 경로를 추가했다. 과제 자료와 기존 할당 전략의 기록은 [정리 전 이력](https://github.com/woonyong-kr/lrn-malloc/tree/24c2cc836b519985e3163b2acdf6d409543e4410)에 남아 있다.
