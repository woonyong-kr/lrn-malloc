# 🧩 lrn-malloc

제한된 힙의 빈 블록을 찾아 할당하고, 해제한 이웃 블록을 합쳐 재사용하는 C 메모리 할당기입니다. 같은 trace를 AVL과 first-fit free list에 넣어 정확성·이용률·처리량을 비교합니다.

[메모리 관리 Wiki](https://docs.woonyong.com/wiki/computer-systems-network-topic-d160fea60072/) · [AVL 구현](malloc-lab/mm.c) · [free list](malloc-lab/baseline.c)

## 실행

macOS/Linux의 C compiler, Make, Python 3가 필요합니다.

```sh
make setup
make demo
make test
# 다른 trace를 직접 재생
.build/trace-avl malloc-lab/traces/random2-bal.rep
.build/trace-list malloc-lab/traces/random2-bal.rep
```

데모는 요청마다 힙 블록의 offset·크기·사용 여부를 출력합니다. 검사와 비교 결과는 `.build/results.json`에 저장됩니다.

## 구현과 설계

allocate/free/reallocate → 빈 블록 탐색 → 분할·병합·힙 확장 → 실제 payload pointer로 이어집니다.

- `mm_init`, `mm_malloc`, `mm_free`, `mm_realloc`을 최대 20 MiB의 memlib 힙 위에서 구현합니다. 반환 영역은 8바이트 정렬을 지킵니다.
- [trace_runner.c](scripts/trace_runner.c)는 실제 메모리에 패턴을 쓰고 비중첩과 데이터 보존을 확인합니다. `--verbose`로 각 요청의 블록 변화를 볼 수 있습니다.
- 0 크기는 NULL, `free(NULL)`은 no-op, `realloc(NULL,n)`은 malloc, `realloc(p,0)`은 free입니다. 표현 범위를 넘는 요청은 NULL로 거절하고 원래 payload를 보존합니다.

`make test`는 대표 trace와 seed가 고정된 혼합 요청, NULL·0·SIZE_MAX·realloc 경계를 두 구현에 적용합니다. 정확성 검사와 반복 throughput 측정은 분리되어 있으며 처리량은 trace 해석·검증 시간을 제외합니다. 이용률은 최대 요청 payload / 최종 힙 크기입니다.

## 현재 범위

단일 스레드 교육용 할당기이며 libc 대체나 `LD_PRELOAD`를 제공하지 않습니다. 유효한 pointer의 free/realloc만 계약에 포함하고 double free·임의 pointer는 undefined behavior입니다.

두 구현은 탐색 방식뿐 아니라 최소 블록·확장 전략과 관리 비용도 다릅니다. 짧은 trace는 측정 잡음에 민감하며, 결과를 자료구조 하나의 효과나 AVL의 일관된 우위로 해석하지 않습니다.

## 출처와 기여

원본 `woonyong-kr/SW_AI-W07-malloc-lab`의 `2a1cbe7fbc771824d55ad4816714610409fcd437`에서 이어 받은 학습용 파생본이다. 원본 과제·팀 코드와 이후 개인 확장은 Git author와 diff로 구분하며, 기존 저작권 표시는 소스에 유지한다. 원본 주소의 공개 접근이 제한돼 있어 자료는 아래 이력 링크로 확인할 수 있다.

AVL 구현을 이어받아 비교용 free list와 trace 재생 경로를 추가했다. 과제 자료와 기존 할당 전략의 기록은 [정리 전 이력](https://github.com/woonyong-kr/lrn-malloc/tree/24c2cc836b519985e3163b2acdf6d409543e4410)에 남아 있다.
