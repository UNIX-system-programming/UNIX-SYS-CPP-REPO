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
        // 공유 메모리 접근
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

        // 메시지 큐 접근
        msgId = msgget(MSG_KEY, 0666);
        if (msgId == -1) {
            perror("msgget failed");
            exit(1);
        }

        cout << "================================================" << endl;
        cout << "[ Client 02 ] 베스킨라빈스 31 게임 클라이언트 시작" << endl;
        cout << "[ Client 02 ] 공유 메모리를 통한 통신" << endl;
        cout << "[ Client 02 ] Player ID: " << playerId << endl;
        cout << "================================================" << endl;
    }

    static auto monitorThreadFunc(void* arg) -> void* {
        ShrClient02* client = static_cast<ShrClient02*>(arg);
        
        while (client->running) {
            if (client->data->gameover) {
                cout << "\n[ Client 02 ] GAME OVER! " << client->data->last_caller << "가 31을 외쳤습니다!" << endl;
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
        sprintf(msg.msg_text, "%d %d", playerId, count);

        if (msgsnd(msgId, &msg, sizeof(msg.msg_text), 0) == -1) {
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

        // Player 2가 먼저 시작하지 않도록 대기
        cout << "[ Client 02 ] Player 1을 기다리는 중..." << endl;
        sleep(2);

        while (running && !data->gameover) {
            // 현재 턴이 아니면 대기
            while (data->current_turn != playerId && running && !data->gameover) {
                cout << "[ Client 02 ] 상대 차례입니다. 대기 중... (현재 숫자: " 
                     << data->current_num << ", 다음 턴: " << data->current_turn << ")" << endl;
                sleep(2);
            }

            if (!running || data->gameover) break;

            // 1~3개의 숫자를 외칠 수 있음
            int count = (rand() % 3) + 1;

            cout << "\n[ Client 02 ] 게임 시작 (현재 숫자: " << data->current_num << ")" << endl;
            cout << "[ Client 02 ] " << count << "개의 숫자를 외치겠습니다" << endl;

            for (int i = 0; i < count; i++) {
                data->current_num++;
                cout << "[ Client 02 ] 숫자 발성: " << data->current_num << endl;

                if (data->current_num >= MAX_NUM) {
                    data->gameover = true;
                    strcpy(data->last_caller, "Client 02");
                    cout << "\n[ Client 02 ] 31을 외쳤습니다... 졌습니다!" << endl;
                    running = false;
                    break;
                }
                sleep(1);
            }

            // 턴 넘기기
            if (!data->gameover) {
                data->current_turn = (playerId == 1) ? 2 : 1;
                cout << "[ Client 02 ] 턴을 넘겼습니다. 상대 차례입니다." << endl;
            }

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
