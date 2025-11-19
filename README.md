# UNIX-SYS-CPP-REPO (server.cpp 설명)
**2025 4-2 유닉스 시스템 프밍 텀프 서버 로직 설명입니다.**

---

## 텀프 주제 (IPC 기법을 사용한 프로세스 간 베라31 게임)

- 목표 : 상대 프로세스가 마지막 숫자 31을 외치도록 유도
- 참가 인원 : 2개 이상의 프로세스가 순서를 정해 번갈아가며 진행
- 각 프로세스는 본인의 순서에 숫자를 1~3개 연속으로 외칠 수 있음
- 패배 조건 : 마지막 숫자인 31을 외치는 프로세스가 패배
---
## 핵심 공유 자원 정의

본 게임에서 가장 중요한 `공유 자원`은 `현재 외쳐진 숫자(currnet_num)` 및 `현재 턴(current_turn)`
정보이며 이 변수들은 `여러 스레드(recieve, recieve_pipe, borad_cast)`가 동시에 접근하기 때문에
임계 구역 설정을 통한 보호가 필요하며 mutex 기법을 사용해 다음과 같은 경쟁 조건 방지
- 여러 스레드가 동시에 숫자를 증가시키려 시도하는 경우
- 게임 종료 후에도 다른 스레드가 공유 메모리 수정을 시도하는 경우
- 턴 교체 과정에서 잘못된 순서의 프로세스 접근이 감지될 경우
```cpp
pthread_mutex_lock(&lock); // 임계 구역 진입 후 락 설정
if (!data->gameover) {
  data->current_num++;
  if (data-<current_num >= MAX_NUM) {
    data->gameover = true;
    strcpy(data->last_caller, ("P" + to_string(playerId)).c_str());
  }
}
pthread_mutex_unlock(&lock); // 임계 구역 락 해제
```
---
## 서버 구조 및 사용된 IPC 기법
현재 `server.cpp` 로직에선 세 가지 IPC 기법을 지원한다.
- **공유 메모리 (Advanced IPC)** -> 현재 게임 상태 공유
- **메세지 큐 (Advanced IPC)** -> 클라이언트의 요청 데이터 수신
- **파이프 (Named FIFO)** -> pipe 클라이언트와 단방향 통신
---
## 스레드 구성
서버는 총 세 개의 스레드로 병렬 처리된다. (각 스레드에는 동일한 공유 자원에 접근)
- **recieve** -> 메세지 큐 기반 요청 수신 (클라이언트의 메세지를 읽고 게임 로직에 반영)
- **recieve_pipe** -> 파이프 기반 요청 수신 (FIFO로부터 숫자 외치기 요청을 수신)
- **boradcast** -> 상태 전파 (현재 숫자의 턴을 주기적으로 출력)
---
## 공유 자원 구조체 
서버의 핵심 공유 데이터 구조 (모든 스레드들이 참조하며 임계구역 내부에서만 읽기/쓰기 가능)
```cpp
struct SharedData {
  int current_num;      // 현재 외쳐진 숫자
  int current_trun;     // 현재 턴 (1 이나 2)
  char last_caller[20]; // 마지막으로 숫자를 외친 process no.
  bool gameover;        // 게임 종료 여부
};
```
---
## 종료 처리
사용자의 SIGINT(2) ctrl + c 단축키 입력 감지 시 안전하게 종료
- `Server::cleanUp()` 함수 호출
- 공유 메모리, 메세지 큐, 파이프의 자원을 해제
- 데드락 방지를 위해 뮤텍스 락 해제
