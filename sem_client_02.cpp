#include "headerSet.hpp"

// [ SRP : 단일 책임 원칙 ] -> server.cpp에 고정된 신호 순차 전송
// [ DIP : 의존 역전 원칙 ] -> server.cpp의 SemaphoreReceiver와만 연결

int main() {
    int semId;
    while (true) {
        semId = semget(SEM_KEY_02, 1, 0666);
        if (semId != -1) break;
        perror("semget 대기 중");
        sleep(1);
    }
    if (semId == -1) { perror("semget"); return 1; }

    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shmId == -1) { perror("shmget ( client )"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat ( client )"); return 1; }

    sembuf sops{};
    sops.sem_num = 0;
    sops.sem_flg = 0;

    int moves[] = {3, 4, 7, 8, 11, 12, 15, 16, 19, 20, 23, 24, 27, 28, 31};
    int moveCount = sizeof(moves) / sizeof(moves[0]);

    cout << "[ SEM_CLIENT_02 ] 시작됨 (세마포어 ID: " << semId << ")" << endl;

    for (int i = 0; i < moveCount; i++) { // P연산
        shared->current_cnt = (moves[i] == MAX_NUM) ? 1 : 2;
        sops.sem_op = -1;
        if (semop(semId, &sops, 1) == -1) {
            if (errno == EINTR) continue;
            perror("semop P");
            break;
        }
        
        cout << "[ SEM_Client_02 ] 숫자 외침 : " << moves[i] 
             << " (이번 턴 외친 개수: " << shared->current_cnt << ")" << endl;

        sops.sem_op = 1; // V연산
        if (semop(semId, &sops, 1) == -1) {
            perror("semop V"); 
            break;
        }
        usleep(200000);
        if (moves[i] >= MAX_NUM) break;
    }

    cout << "[ SEM_CLIENT_02 ] 클라이언트 프로세스 P2 종료" << endl;
    return 0;
}
