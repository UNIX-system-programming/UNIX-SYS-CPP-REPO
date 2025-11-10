#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits>
#include <algorithm>
#include <csignal>
#include <errno.h>
#include <cstdlib> 
#include <ctime>   

using namespace std;

// --- 상수 및 FIFO 경로 정의 ---
#define REQUEST_FIFO "/tmp/br31_request_fifo"
#define RESPONSE_BASE_FIFO "/tmp/br31_resp_"
#define MAX_PLAYERS 5

// --- 클라이언트 요청 구조체 ---
struct ClientRequest {
    int playerId;       
    int count;         
    char client_pipe_path[40]; 
};

class Client {
    int playerId;
    string response_fifo_path;
    pthread_t send_thread, receive_thread;
    bool running;

public:
    Client(int id) : playerId{id}, running{true} {
        // 1. 응답 FIFO 경로 설정 및 생성
        response_fifo_path = RESPONSE_BASE_FIFO + to_string(playerId);
        if (mkfifo(response_fifo_path.c_str(), 0666) == -1 && errno != EEXIST) {
            perror("mkfifo response error");
            exit(1);
        }
        
        cout << "============================================" << endl;
        cout << "[ Client P" << playerId << " ] 베스킨라빈스 클라이언트 가동." << endl;
        cout << "[ Client P" << playerId << " ] 응답 FIFO 경로: " << response_fifo_path << endl;
        cout << "============================================" << endl;
    }

    // --- 3. 초기 등록 요청 함수 (count=0) ---
    void sendInitialRequest() {
        int fd = open(REQUEST_FIFO, O_WRONLY | O_NONBLOCK); 
        if (fd == -1) {
            // 서버 대기 후 재시도
            sleep(2); 
            fd = open(REQUEST_FIFO, O_WRONLY); 
            if (fd == -1) {
                perror("open REQUEST_FIFO failed after retry");
                return;
            }
        }

        ClientRequest req;
        req.playerId = playerId;
        req.count = 0; 
        strcpy(req.client_pipe_path, response_fifo_path.c_str());

        write(fd, &req, sizeof(req));
        close(fd);
        cout << "[ Initial ] 서버에 플레이어 등록 요청 전송 완료." << endl;
    }

    // --- 1. 상태 수신 쓰레드 (FIFO Read) ---
    static auto receiveThreadFunc(void* arg) -> void* {
        Client* client = (Client*)arg;
        char buffer[100];
        
        int fd = open(client->response_fifo_path.c_str(), O_RDONLY); 
        if (fd == -1) {
            perror("open response FIFO error");
            pthread_exit(nullptr);
        }
        
        while (client->running) {
            memset(buffer, 0, sizeof(buffer));
            if (read(fd, buffer, sizeof(buffer)) > 0) {
                string status(buffer);
                
                cout << endl << "--- P" << client->playerId << " 상태 수신 -----------------" << endl;
                cout << status << endl;
                cout << "--------------------------------------------" << endl;
                
                if (status.find("END:") != string::npos) {
                    client->running = false;
                    break;
                }
            } else if (errno == ENXIO) {
                 client->running = false;
                 break;
            }
        }
        close(fd);
        pthread_exit(nullptr);
    }
    
    // --- 2. 요청 전송 쓰레드 (자동 요청/FIFO Write) ---
    static auto sendThreadFunc(void* arg) -> void* {
        Client* client = (Client*)arg;
        ClientRequest req;
        int count;
        
        srand(time(NULL) + client->playerId); 
        
        int fd = open(REQUEST_FIFO, O_WRONLY | O_NONBLOCK); 
        if (fd == -1) {
            perror("open REQUEST_FIFO error");
            pthread_exit(nullptr);
        }
        
        cout << "[ Auto ] 자동 게임 시작. 턴을 기다립니다." << endl;

        while (client->running) {
            
            sleep(1); 
            
            count = (rand() % 3) + 1; 

            req.playerId = client->playerId;
            req.count = count;
            strcpy(req.client_pipe_path, client->response_fifo_path.c_str());
            
            // 요청 전송 (서버의 턴이 아니더라도 경쟁적으로 전송 시도)
            if (write(fd, &req, sizeof(req)) == -1) {
                if (errno != EAGAIN && errno != EINTR) {
                    perror("write to REQUEST_FIFO error");
                    break;
                }
                usleep(500000); 
                continue;
            }
            
            cout << "P" << client->playerId << " -> " << req.count << "개 외침 시도. (랜덤)" << endl;
            // 요청 후 다음 턴을 기다리는 랜덤 딜레이 
            usleep(500000 + (rand() % 1000000)); 
        }
        close(fd);
        pthread_exit(nullptr);
    }
    
    // --- 쓰레드 관리 및 자원 해제 ---
    auto start() -> void {
        sendInitialRequest(); 
        pthread_create(&receive_thread, nullptr, receiveThreadFunc, this);
        pthread_create(&send_thread, nullptr, sendThreadFunc, this);
    }

    auto wait() -> void {
        pthread_join(receive_thread, nullptr);
        pthread_join(send_thread, nullptr);
    }

    auto cleanUp() -> void {
        if (unlink(response_fifo_path.c_str()) == 0) {
             cout << "[ Client P" << playerId << " ] 응답 FIFO 제거 완료." << endl;
        }
    }

    ~Client() {
        if (running) cleanUp();
    }
};

void sigPipeHandler(int signum) {
    cerr << "[ SIGPIPE ] 서버가 종료되어 write 실패! 시그널 무시 처리됨." << endl;
}


int main(int argc, char* argv[]) {
    signal(SIGPIPE, sigPipeHandler); 

    if (argc != 2) {
        cerr << "사용법: " << argv[0] << " <PlayerID (1, 2, 3...)>" << endl;
        return 1;
    }
    
    int playerId = stoi(argv[1]);
    if (playerId <= 0 || playerId > MAX_PLAYERS) {
        cerr << "Player ID는 1부터 " << MAX_PLAYERS << " 사이여야 합니다." << endl;
        return 1;
    }

    Client client(playerId);
    client.start();
    client.wait();
    
    return 0;
}
