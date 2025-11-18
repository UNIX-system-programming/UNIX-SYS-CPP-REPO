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
#define PLAYER_ID 2

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

class ShrClient02 {
    int shmId, msgId;
    SharedData* data;
    int playerId;
    pthread_t monitorThread;
    bool running;

public:
    ShrClient02() : playerId(PLAYER_ID), running(true) {
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
        cout << "[ Client 02 ] 베스킨라빈스 31 서버 가동 시작" << endl;
        cout << "[ Client 02 ] Shared Memory" << endl;
        cout << "[ Client 02 ] Player ID: " << playerId << endl;
        cout << "================================================" << endl;
    }

    static auto monitorThreadFunc(void* arg) -> void* {
        ShrClient02* client = static_cast<ShrClient02*>(arg);
        
        while (client->running) {
            if (client->data->gameover) {
                cout << "\n[ Client 02 ] 게임 종료! " << client->data->last_caller << " 이 31을 외쳤습니다!" << endl;
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

        cout << "[ Client 02 ] Client 1을 기다리는 중..." << endl;
        sleep(3);

        while (running && !data->gameover) {
            while (data->current_turn != playerId && running && !data->gameover) {
                cout << "[ Client 02 ] 턴을 기다리는 중... (현재 숫자: " 
                     << data->current_num << ", 다음 순서: " << data->current_turn << ")" << endl;
                sleep(2);
            }

            if (!running || data->gameover) break;

            int count = (rand() % 3) + 1;

            // 요청을 서버에 전송한 후 공유 메모리의 변경을 관찰합니다
            int prev = data->current_num;
            sendMove(count);
            // 서버가 요청을 처리하는 동안 대기합니다.
            // 서버는 current_num을 요청한 개수만큼 증가시키고 이후 current_turn을 변경합니다.
            while (!data->gameover) {
                if (data->current_num > prev) {
                    for (int n = prev + 1; n <= data->current_num; ++n) {
                        cout << "[ Client 02 ] " << n << endl;
                        usleep(200000);
                    }
                    prev = data->current_num;
                }
                // 서버가 턴을 다른 플레이어로 넘기면 반복을 종료합니다
                if (data->current_turn != playerId) break;
                usleep(100000);
            }
            cout << "[ Client 02 ] 현재 숫자: " << data->current_num << endl;

            sleep(1);
        }
    }

    auto wait() -> void {
        pthread_join(monitorThread, nullptr);
    }

    auto cleanUp() -> void {
        cout << "[ Client 02 ] 종료 중..." << endl;
        running = false;
        shmdt(data);
    }

    ~ShrClient02() {
        cleanUp();
    }
};

int main() {
    ShrClient02 client;
    client.start();
    client.play();
    client.wait();

    return 0;
}
