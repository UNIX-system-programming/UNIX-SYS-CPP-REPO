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
#include <stdexcept> 

using namespace std;

// --- 상수 및 FIFO 경로 정의 ---
#define REQUEST_FIFO "/tmp/br31_request_fifo"
#define RESPONSE_BASE_FIFO "/tmp/br31_resp_"
#define MAX_PLAYERS 5
#define MAX_NUM 31

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
    
    // [전략] 서버 상태 저장을 위한 volatile 변수 (쓰레드 간 공유)
    volatile int current_server_num = 0; 
    volatile int current_turn_id = 0;    

public:
    Client(int id) : playerId{id}, running{true} {
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
        // ... (이전 코드와 동일) ...
        int fd = open(REQUEST_FIFO, O_WRONLY | O_NONBLOCK); 
        if (fd == -1) {
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

    // --- 1. 상태 수신 쓰레드 (FIFO Read & 상태 파싱) ---
    static auto receiveThreadFunc(void* arg) -> void* {
        // ... (이전 코드와 동일) ...
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
                
                try {
                    size_t current_pos = status.find("CURRENT:");
                    size_t turn_pos = status.find("TURN:");
                    
                    if (current_pos != string::npos) {
                        size_t end_current = status.find(" ", current_pos + 8);
                        client->current_server_num = stoi(status.substr(current_pos + 8, end_current - (current_pos + 8)));
                    }

                    if (turn_pos != string::npos) {
                        size_t end_turn = status.find(" ", turn_pos + 5);
                        if (end_turn == string::npos) end_turn = status.length();
                        client->current_turn_id = stoi(status.substr(turn_pos + 5, end_turn - (turn_pos + 5)));
                    }
                } catch (const invalid_argument& e) {
                    cerr << "[ RCV Error ] Status parsing failed." << endl;
                }

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
            
            // 1. 자신의 턴이 아니면 요청을 보내지 않고 대기
            if (client->playerId != client->current_turn_id) {
                usleep(500000); 
                continue;
            }
            
            // 2. [전략] 31을 외치지 않을 안전한 외칠 개수 계산
            int current = client->current_server_num;
            int remaining_to_31 = MAX_NUM - current; 
            
            int max_safe_count;

            if (remaining_to_31 <= 3) {
                // 남은 숫자가 3개 이하일 때
                max_safe_count = remaining_to_31 - 1; 
                
                if (max_safe_count < 1) {
                    // [수정된 로직] 현재 30일 때: 1개 외치고 패배하도록 강제
                    count = 1; 
                    cout << "P" << client->playerId << " (전략): 패배 확정. 1개 외치고 게임 종료 유도." << endl;
                } else {
                    // 28, 29일 때: 안전한 범위 (1~max_safe_count) 내에서 랜덤
                    count = (rand() % max_safe_count) + 1; 
                }
            } else {
                // 안전한 범위 (1~3)
                max_safe_count = 3; 
                count = (rand() % max_safe_count) + 1; 
            }
            
            // 요청 데이터 구성 및 전송
            req.playerId = client->playerId;
            req.count = count;
            strcpy(req.client_pipe_path, client->response_fifo_path.c_str());

            if (write(fd, &req, sizeof(req)) == -1) {
                if (errno != EAGAIN && errno != EINTR) {
                    perror("write to REQUEST_FIFO error");
                    break;
                }
                usleep(500000); 
                continue;
            }
            
            cout << "P" << client->playerId << " -> " << count << "개 외침 시도. (전략적)" << endl;
            usleep(500000 + (rand() % 1000000)); 
        }
        close(fd);
        pthread_exit(nullptr);
    }
    
    // --- 쓰레드 관리 ---
    auto start() -> void {
        sendInitialRequest(); 
        pthread_create(&receive_thread, nullptr, receiveThreadFunc, this);
        pthread_create(&send_thread, nullptr, sendThreadFunc, this);
    }

    auto wait() -> void {
        pthread_join(receive_thread, nullptr);
        pthread_join(send_thread, nullptr);
    }

    // --- 자원 해제 ---
    auto cleanUp() -> void {
        if (unlink(response_fifo_path.c_str()) == 0) {
             cout << "[ Client P" << playerId << " ] 응답 FIFO 제거 완료." << endl;
        }
    }

    ~Client() {
        if (running) cleanUp();
    }
};

// --- SIGPIPE 핸들러 ---
void sigPipeHandler(int signum) {
    cerr << "[ SIGPIPE ] 서버가 종료되어 write 실패! 시그널 무시 처리됨." << endl;
}


int main(int argc, char* argv[]) {
    // SIGPIPE 시그널 핸들러 등록
    signal(SIGPIPE, sigPipeHandler); 

    if (argc != 2) {
        cerr << "사용법: " << argv[0] << " <PlayerID (1, 2, 3...)>" << endl;
        return 1;
    }
    
    // Player ID 유효성 검사
    int playerId = stoi(argv[1]);
    if (playerId <= 0 || playerId > MAX_PLAYERS) {
        cerr << "Player ID는 1부터 " << MAX_PLAYERS << " 사이여야 합니다." << endl;
        return 1;
    }

    Client client(playerId);
    client.start();
    client.wait(); // 쓰레드 종료 대기
    
    return 0;
}
