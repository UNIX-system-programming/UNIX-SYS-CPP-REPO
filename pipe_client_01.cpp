#include "headerSet.hpp"

int main() {
    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    while (shmId == -1) {
        perror("shmget 대기 중");
        sleep(1);
        shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    }
    if (shmId == -1) { perror("shmget ( client )"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat ( client )"); return 1; }

    int moves[] = {1, 2, 5, 6, 9, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30};
    int moveCnt = sizeof(moves) / sizeof(moves[0]);

    cout << "[ Pipe_Client_01 ] 시작됨" << endl;
    cout.flush();

    for (int i = 0; i < moveCnt; i += 2) {
        if (shared->gameover) break;

        // 턴 대기: current_turn == 1 일 때까지 대기
        while (!shared->gameover && shared->current_turn != 1) {
            usleep(50000);
        }
        if (shared->gameover) break;

        // 두 개의 숫자 외침
        int cnt = 0;
        for (int j = 0; j < 2 && (i + j) < moveCnt; j++) {
            if (shared->gameover) break;
            
            cout << "[ Pipe_Client_01 ] 숫자 외침 : " << moves[i + j] << endl;
            cout.flush();
            
            cnt++;
            usleep(300000);
        }

        // 메시지 전송: "playerId cnt"
        int fd = open(PIPE_PATH, O_WRONLY);
        if (fd != -1) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %d", 1, cnt);
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

    while (!shared->gameover) usleep(100000);
    cout << "[ Pipe_Client_01 ] 클라이언트 프로세스 P1 종료" << endl;
    cout.flush();

    shmdt(shared);
    return 0;
}
