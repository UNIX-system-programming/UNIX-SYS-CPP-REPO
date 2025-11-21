#include "headerSet.hpp"
#include <signal.h>

volatile sig_atomic_t stop_requested = 0;

void handle_sigint(int) {
    stop_requested = 1;
}

int main() {
    // 시그널 핸들러 등록
    struct sigaction sa{};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    while (shmId == -1) {
        perror("shmget 대기 중");
        sleep(1);
        shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    }
    if (shmId == -1) { perror("shmget ( client )"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat ( client )"); return 1; }

    int moves[] = {3, 4, 7, 8, 11, 12, 15, 16, 19, 20, 23, 24, 27, 28, 31};
    int moveCnt = sizeof(moves) / sizeof(moves[0]);

    cout.flush();

    for (int i = 0; i < moveCnt; i += 2) {
        if (shared->gameover) break;

        // 턴 대기: current_turn == 2 일 때까지 대기
        while (!shared->gameover && shared->current_turn != 2) {
            usleep(50000);
        }
        if (shared->gameover) break;

        // 두 개의 숫자 외침
        int cnt = 0;
        for (int j = 0; j < 2 && (i + j) < moveCnt; j++) {
            if (shared->gameover) break;
            
            // 클라이언트가 외친 숫자 및 외친 개수 기록 (출력 형식은 세마포어 클라이언트와 동일)
            shared->current_cnt = 2;
            cnt++;
            cout.flush();
            usleep(300000);
        }

        // 메시지 전송: "playerId cnt"
        // FIFO가 준비될 때까지 대기
        while (!stop_requested && access(PIPE_PATH, F_OK) == -1) {
            usleep(100000);
        }
        if (stop_requested) break;
        int fd = open(PIPE_PATH, O_WRONLY);
        if (fd != -1) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %d", 2, cnt);
            write(fd, buf, strlen(buf)+1);
            close(fd);
        }

        // 중요: 턴이 바뀔 때까지 대기 (동기화)
        int prevTurn = shared->current_turn;
        while (!shared->gameover && shared->current_turn == prevTurn) {
            usleep(50000);
        }

        usleep(200000);
    }

    while (!shared->gameover && !stop_requested) usleep(100000);
    cout.flush();

    shmdt(shared);
    return 0;
}
