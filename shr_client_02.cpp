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
    int connected_players;
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

    // 서버에 등록 메시지를 보냅니다 (접속 알림)
    auto sendRegister() -> bool {
        MsgQueue msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_type = 2; // 등록
        snprintf(msg.msg_text, sizeof(msg.msg_text), "%d", playerId);
        if (msgsnd(msgId, &msg, 100, 0) == -1) {
            perror("register msgsnd failed");
            return false;
        }
        return true;
    }

    auto play() -> void {
        srand(time(nullptr) + playerId);

        // 서버에 접속 등록
        sendRegister();

        // 최소 PLAYERS명이 접속할 때까지 대기
        while (!data->gameover && data->connected_players < PLAYERS) {
            cout << "[ Client 02 ] 플레이어를 기다리는 중... (현재 접속=" << data->connected_players << ")" << endl;
            sleep(1);
        }

        int prev_turn = -1;
        while (running && !data->gameover) {
            // 대기 중일 때는 턴 변화가 있을 때만 간단히 출력합니다.
            while (data->current_turn != playerId && running && !data->gameover) {
                if (prev_turn != data->current_turn) {
                    cout << "[ Client 02 ] 상대 턴... (현재 숫자: " << data->current_num << ", 다음 턴: " << data->current_turn << ")" << endl;
                    prev_turn = data->current_turn;
                }
                sleep(1);
            }

            if (!running || data->gameover) break;

            int count = (rand() % 3) + 1;

            // 요청을 서버에 전송한 후 공유 메모리의 변경을 관찰합니다
            int prev = data->current_num;
            sendMove(count);

            while (!data->gameover) {
                if (data->current_num > prev) {
                    for (int n = prev + 1; n <= data->current_num; ++n) {
                        cout << "[ Client 02 ] " << n << endl;
                        usleep(150000);
                    }
                    prev = data->current_num;
                }
                if (data->current_turn != playerId) break;
                usleep(100000);
            }

            prev_turn = data->current_turn;
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
