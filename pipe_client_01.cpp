#include "headerSet.hpp"

int main() {
    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    while (shmId == -1) {
        perror("shmget 대기 중 (client1)");
        sleep(1);
        shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat (client1)"); return 1; }

    int moves[] = {1,2,5,6,9,10,13,14,17,18,21,22,25,26,29,30};
    int moveCnt = sizeof(moves)/sizeof(moves[0]);

    cout << "[ PIPE_Client_01 ] 시작됨" << endl;

    for (int i = 0; i < moveCnt; i += 2) {
        if (shared->gameover) break;

        // wait for our turn
        while (!shared->gameover && shared->current_turn != 1) sleep(1);
        if (shared->gameover) break;

        int remaining = moveCnt - i;
        int cnt = remaining >= 2 ? 2 : 1;

        // send move to server via PIPE: "playerId cnt"
        int written = 0;
        for (int attempt = 0; attempt < 5 && !written; ++attempt) {
            int fd = open(PIPE_PATH, O_WRONLY | O_NONBLOCK);
            if (fd == -1) {
                usleep(200000);
                continue;
            }
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %d", 1, cnt);
            if (write(fd, buf, strlen(buf)+1) == -1) perror("write pipe client1");
            close(fd);
            written = 1;
        }

        // small delay to let server process
        sleep(1);
    }

    // wait until gameover then exit
    while (!shared->gameover) usleep(100000);
    cout << "[ PIPE_Client_01 ] 종료" << endl;
    shmdt(shared);
    return 0;
}
