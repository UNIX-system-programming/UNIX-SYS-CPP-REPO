#include <iostream>
#include <cstring>
#include <string>
#include <csignal>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
using namespace std;
// string은 객체라 사용 시 에러 발생하니 cstring 사용
// 정 쓰고 싶으면 string_view() 쓰면 되는데 복잡해짐

#define SHM_KEY 60011
#define MSG_KEY 60012
#define PLAYERS 2
#define MAX_NUM 31

struct MsgQueue {
    long msg_type;
    char msg_text[100];
}; // message queue 구조체

struct SharedData {
    int current_num; // 현재 숫자
    int current_turn; // 현재 턴
    char last_caller[20];
    bool gameover;
};// 공유 메모리 구조체

class Server {
    int shmId, msgId;
    SharedData* data;
    pthread_mutex_t lock;
    // 클라이언트 요청 수신용 & 게임 상태 브로드캐스트 용 threadId
    pthread_t recieve, broadcast;
    bool running;
public:
    Server() : running{true} {
        pthread_mutex_init(&lock, nullptr);

        // 아래는 공유 메모리 생성 로직
        shmId = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
        if (shmId == -1) {
            perror("result of shmget sys call");
            exit(1);
        }
        data = (SharedData*)shmat(shmId, nullptr, 0);
        memset(data, 0, sizeof(SharedData));

        // 메세지 큐 생성 로직
        msgId = msgget(MSG_KEY, 0666 | IPC_CREAT);
        if (msgId == -1) {
            perror("result of msgget sys call");
            exit(1);
        }

        // 초기 값 설정
        data->current_num = 0;
        data->current_turn = 1;
        data->gameover = false;
        strcpy(data->last_caller, "[ nullptr ]");

        cout << "============================================" << endl;
        cout << "[ Server ] -> 베스킨라빈스 31 서버 가동 시작" << endl;
        cout << "[ Server ] -> 현재 숫자 = 0 | 시작 턴 = P1" << endl;
        cout << "============================================" << endl;
    }
    static auto recieveThreadFunc(void* arg) -> void* {
        // void* 타입은 모든 포인터 타입을 형 변환 없이 받을 수 있음
        // 다만 void* 타입을 다른 값에 대입 시 반드시 형 변환 필요 (안 하면 에러남)
        Server* server = (Server*)arg; 
        MsgQueue msg;

        while (server->running) {
            memset(&msg, 0, sizeof(msg));
            if (msgrcv(server->msgId, &msg, sizeof(msg.msg_text), 1, 0) != -1) {
                pthread_mutex_lock(&server->lock);
                if (server->data->gameover) {
                    pthread_mutex_unlock(&server->lock);
                    continue;
                }
                int playerId, cnt; // playerId 추후 PID로 수정 예정
                sscanf(msg.msg_text, "%d %d", &playerId, &cnt);
                if (playerId != server->data->current_turn) {
                    cout << "[ Server ] Process ID " << playerId << "접근 거절 (본인 순서가 아님)" << endl;
                    pthread_mutex_unlock(&server->lock);
                    continue;
                }
                cout << "[ Server ] Process ID " << playerId << " 요청 (정수 " << cnt << "개)" << endl;
                for (int i = 0; i < cnt; i++) {
                    server->data->current_num++;
                    cout << "[ Server ] 현재 숫자 : " << server->data->current_num << endl;
                    if (server->data->current_num >= MAX_NUM) {
                        server->data->gameover = true;
                        // to_string() 통해 문자열 객체로 변환 후 다시 char * 타입으로 변환 (반드시 해줘야 함)
                        strcpy(server->data->last_caller, ("Process ID " + to_string(playerId)).c_str());
                        cout << endl << "[ Server ] GAME OVER - 프로세스 " << server->data->last_caller << "가 숫자 31을 외침" << endl;
                        pthread_mutex_unlock(&server->lock);
                        server->running = false;
                        pthread_exit(nullptr);
                    }
                    sleep(1);
                }
                server->data->current_turn = (playerId == 1) ? 2 : 1; // 턴 교체
                pthread_mutex_unlock(&server->lock);
            }
        }
        return nullptr;
    }
    static auto broadcastThreadFunc(void* arg) -> void* {
        Server* server = (Server*)arg;
        while (server->running) {
            pthread_mutex_lock(&server->lock);
            if (server->data->gameover) {
                pthread_mutex_unlock(&server->lock);
                break;
            }
            cout << "[ Server ] 상태 전파 (현재 숫자 = " << server->data->current_num << ", 다음 턴 Proceess ID " << server->data->current_turn << endl;
            pthread_mutex_unlock(&server->lock);
            sleep(3);
        }
        return nullptr;
    }
    auto start() -> void {
        pthread_create(&recieve, nullptr, recieveThreadFunc, this);
        pthread_create(&broadcast, nullptr, broadcastThreadFunc, this);
    }
    auto wait() -> void {
        pthread_join(recieve, nullptr);
        pthread_join(broadcast, nullptr);
    }
    auto cleanUp() -> void {
        cout << "[ Server ] 종료 시그널 감지 (자원 해제 중,,,)" << endl;
        shmdt(data);
        shmctl(shmId, IPC_RMID, nullptr);
        msgctl(msgId, IPC_RMID, nullptr);
        pthread_mutex_destroy(&lock);
    }
    ~Server() {
        cleanUp(); // 소멸자 객체 통해 모든 자원 반환
    }
};

Server* server_sig = nullptr;
auto sigHandler(int signNo) { // SIGINT(2)를 매개변수로 받음
    if (server_sig) server_sig->cleanUp();
    exit(0);
}

int main() {

    signal(SIGINT, sigHandler);
    server_sig = new Server();
    server_sig->start();
    server_sig->wait();
    delete server_sig;

    return 0;
}