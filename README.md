# lrn-malloc

제한된 힙을 관리하는 메모리 할당 엔진. 핵심 엔진을 실제 입력으로 실행하고 결과와 내부 동작을 확인하는 독립 프로그램이다.

## 실행

macOS/Linux의 C compiler·make·Python 3가 필요하다.

```sh
make setup
make test
make demo
# 다른 유효한 CS:APP trace를 직접 재생
.build/trace-avl malloc-lab/traces/random-bal.rep
.build/trace-list malloc-lab/traces/random-bal.rep
```

대화형 서버는 해당 터미널에서 Ctrl-C로 종료한다. demo/test의 자식 프로세스는 실행기가 보유한 PID 또는 컨테이너 ID로만 종료한다. 다른 서버를 포트 번호로 찾아 일괄 종료하지 않는다. 준비된 Python 환경이 없으면 먼저 `make setup`을 실행한다.

## 입력에서 출력까지

trace의 allocate/free/reallocate → AVL 또는 first-fit free list → block 분할·병합·힙 확장 → 실제 payload pointer

기존 AVL 기반 탐색과 새 explicit first-fit free list를 같은 memlib 힙 위에서 비교한다. `mm_init`, `mm_malloc`, `mm_free`, `mm_realloc`을 제공한다. 최대 힙은 기존 20 MiB, 정렬은 8바이트다. 0 크기는 NULL, free(NULL)은 no-op, realloc(NULL,n)은 malloc, realloc(p,0)은 free다. 표현 범위를 넘는 요청은 NULL로 거절하고 원래 payload를 보존한다.

trace runner는 할당한 실제 메모리에 패턴을 채우고 해제·재할당·종료 시 확인한다. 새 영역은 모든 살아 있는 영역과 겹치지 않는지 검사한다. `--verbose`는 요청과 heap block offset·크기·사용 여부를 출력한다.

`make test`는 13개 기존 balanced trace와 seed 731의 혼합 stress를 두 엔진에 적용한다. `.build/results.json`에 결과를 쓴다. 정확성 실행과 throughput 측정은 분리한다. 최소 20회, 누적 allocator 실행 30ms 이상을 목표로 반복하며 최대 100,000회다. throughput은 trace 해석·검증 비용을 제외한 재생 속도다. 이용률은 peak requested payload / final heap bytes다.

구현을 읽는 순서: `malloc-lab/mm.c`, `malloc-lab/baseline.c`, `scripts/trace_runner.c`.

## 검증과 관찰

기본 13개 trace와 deterministic mixed stress를 두 구현에서 검증한다. NULL·0·SIZE_MAX와 realloc 데이터 보존을 별도 C 계약 테스트로 확인한다.

실행 환경·명령·exit code·원본 백업과 전체 결과는 이번 전환의 별도 작업 폴더에 기록한다. 새 기계에서는 같은 명령으로 직접 재검증한다. 수치가 기록되어 있다는 사실과 현재 실행 성공을 구분한다.

## 지원 범위와 한계

교육용 단일 스레드 할당기이며 libc 대체, LD_PRELOAD, arena·NUMA·범용 오류 탐지기는 제공하지 않는다. 유효한 pointer에 대한 free/realloc만 계약에 포함한다. double free와 임의 pointer는 undefined behavior다.

free list baseline은 선형 탐색·이전 block 탐색, AVL은 복잡한 index 유지 비용이 있다. 구조·최소 block·확장 전략도 달라 결과를 탐색 자료구조 하나의 효과로 단정할 수 없다. 짧은 trace의 처리량은 timer와 환경 잡음에 민감하다. 특정 기계에서의 결과를 항상 AVL이 더 빠르다는 주장으로 사용하지 않는다.

## 원본·학습 문서의 경계

[원본 아카이브와 기여 구분](archive/README.md)을 확인한다. 이 저장소는 실행 코드·테스트·사용법·설계 근거를 소유한다. WIKI는 개념 정본을 소유하며 기존 정본·공통 색인·배포 파일을 이 작업에서 수정하지 않는다. SQL·PintOS와 RepoLM/음성 서비스는 이 프로그램의 실행 의존성이 아니다.
