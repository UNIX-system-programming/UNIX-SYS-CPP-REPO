#include <iostream>
#include <cstring>
#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
using namespace std;

#define SHM_KEY 60011
#define MSG_KEY 60012
#define PLAYERS 2
#define MAX_NUM 31
#define PLAYER_ID 1

struct MsgQueue {
    long msg_type;
    char msg_text[100];
};

struct SharedData {
    int current_num;
    int current_turn;
    char last_caller[20];
    bool gameover;
};

class ShrClient01 {
    int shmId, msgId;
    SharedData* data;
    int playerId;
    pthread_t monitorThread;
    bool running;

public:
    ShrClient01() : playerId(PLAYER_ID), running(true) {
        shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
        if (shmId == -1) {
            perror("shmget failed");
            exit(1);
        }
        data = (SharedData*)shmat(shmId, nullptr, 0);
        if (data == (void*)-1) {
            perror("shmat failed");
            exit(1);
        }

        msgId = msgget(MSG_KEY, 0666);
        if (msgId == -1) {
            perror("msgget failed");
            exit(1);
        }

        cout << "================================================" << endl;
        cout << "[ Client 01 ] 베스킨라빈스 31 서버 가동 시작" << endl;
        cout << "[ Client 01 ] Shared Memory" << endl;
        cout << "[ Client 01 ] Player ID: " << playerId << endl;
        cout << "================================================" << endl;
    }

    static auto monitorThreadFunc(void* arg) -> void* {
        ShrClient01* client = static_cast<ShrClient01*>(arg);
        
        while (client->running) {
            if (client->data->gameover) {
                client->running = false;
                break;
            }
            sleep(1);
        }
        return nullptr;
    }

    auto sendMove(int count) -> bool {
        MsgQueue msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_type = 1;
        snprintf(msg.msg_text, sizeof(msg.msg_text), "%d %d", playerId, count);

        if (msgsnd(msgId, &msg, 100, 0) == -1) {
            perror("msgsnd failed");
            return false;
        }
        return true;
    }

    auto start() -> void {
        pthread_create(&monitorThread, nullptr, monitorThreadFunc, this);
    }

    auto play() -> void {
        srand(time(nullptr) + playerId);

        int prev_turn = -1;
        while (running && !data->gameover) {
            // 대기 중일 때는 턴 변화가 있을 때만 간단히 출력합니다.
            while (data->current_turn != playerId && running && !data->gameover) {
                if (prev_turn != data->current_turn) {
                    cout << "[ Client 01 ] 상대 턴... (현재 숫자: " << data->current_num << ", 다음 턴: " << data->current_turn << ")" << endl;
                    prev_turn = data->current_turn;
                }
                sleep(1);
            }

            if (!running || data->gameover) break;

            int count = (rand() % 3) + 1;

            // 요청을 서버에 전송한 후 공유 메모리의 변경을 관찰합니다
            int prev = data->current_num;
            sendMove(count);

            // 서버가 증가시킨 숫자들을 화면에 순차적으로 출력 (중복 최소화)
            while (!data->gameover) {
                if (data->current_num > prev) {
                    for (int n = prev + 1; n <= data->current_num; ++n) {
                        cout << "[ Client 01 ] " << n << endl;
                        usleep(150000);
                    }
                    prev = data->current_num;
                }
                if (data->current_turn != playerId) break; // 턴이 바뀌면 중단
                usleep(100000);
            }

            // 다음 루프에서 출력이 중복되지 않도록 prev_turn을 갱신
            prev_turn = data->current_turn;
            sleep(1);
        }
    }

    auto wait() -> void {
        pthread_join(monitorThread, nullptr);
    }

    auto cleanUp() -> void {
        cout << "[ Client 01 ] 종료 중..." << endl;
        running = false;
        shmdt(data);
    }

    ~ShrClient01() {
        cleanUp();
    }
};

int main() {
    ShrClient01 client;
    client.start();
    client.play();
    client.wait();

    return 0;
}
