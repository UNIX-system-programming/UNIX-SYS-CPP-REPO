#include <iostream>
#include <cstring>
#include <string>
#include <csignal>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <map>
#include <algorithm>
#include <vector>
#include <limits>
#include <errno.h>

using namespace std;

// --- 상수 및 FIFO 경로 정의 ---
#define MAX_NUM 31
#define MAX_PLAYERS 5
#define REQUEST_FIFO "/tmp/br31_request_fifo"
#define RESPONSE_BASE_FIFO "/tmp/br31_resp_"

// --- 클라이언트 요청 구조체 ---
struct ClientRequest {
    int playerId;
    int count;          // 0: 등록 요청, 1~3: 숫자 외치기
    char client_pipe_path[40];
};

// --- 서버 내부 공유 상태 (Mutex 보호 대상) ---
struct GameState {
    int current_num;
    int current_turn;
    char last_caller[20];
    bool gameover;
    map<int, string> player_pipes;
    vector<int> active_players;
};

class Server {
    GameState state;
    pthread_mutex_t lock; 
    pthread_t receive_thread, broadcast_thread;
    bool running;
    int request_fd;

public:
    Server() : running{true} {
        pthread_mutex_init(&lock, nullptr);
        
        if (mkfifo(REQUEST_FIFO, 0666) == -1 && errno != EEXIST) {
            perror("mkfifo request error");
            exit(1);
        }

        request_fd = open(REQUEST_FIFO, O_RDWR);
        if (request_fd == -1) {
            perror("open REQUEST_FIFO O_RDWR error");
            unlink(REQUEST_FIFO); 
            exit(1);
        }
        
        signal(SIGPIPE, SIG_IGN); 

        state.current_num = 0;
        state.current_turn = 0; 
        state.gameover = false;
        strcpy(state.last_caller, "[ Not Started ]");

        cout << "============================================" << endl;
        cout << "[ Server ] -> 베스킨라빈스 31 파이프 서버 가동 시작" << endl;
        cout << "[ Server ] -> 요청 FIFO 준비: " << REQUEST_FIFO << endl;
        cout << "============================================" << endl;
    }

    // --- 1. 요청 수신 쓰레드 (FIFO Read) ---
    static auto receiveThreadFunc(void* arg) -> void* {
        // ... (이전 코드와 동일) ...
        Server* server = (Server*)arg;
        ClientRequest req;
        
        cout << "[ RCV Thread ] 요청 수신 대기 시작." << endl;

        while (server->running) {
            memset(&req, 0, sizeof(req));
            ssize_t bytes_read = read(server->request_fd, &req, sizeof(req));
            
            if (bytes_read == sizeof(req)) {
                server->processRequest(req);
            } else if (bytes_read == 0) {
                 cout << "[ RCV Thread ] 모든 클라이언트 연결 종료 감지." << endl;
                 server->running = false;
                 break;
            } else if (bytes_read == -1 && errno != EINTR) {
                perror("[ RCV Error ] read from FIFO failed");
                server->running = false;
                break;
            }
        }
        pthread_exit(nullptr);
    }

    // --- 2. 게임 로직 처리 함수 (뮤텍스로 보호되는 임계 구역) ---
    void processRequest(const ClientRequest& req) {
        // ... (이전 코드와 동일) ...
        pthread_mutex_lock(&lock); 

        if (state.gameover) {
            pthread_mutex_unlock(&lock);
            return;
        }

        // 1. 플레이어 등록 및 초기 턴 설정 
        if (state.player_pipes.find(req.playerId) == state.player_pipes.end()) {
            state.player_pipes[req.playerId] = req.client_pipe_path;
            state.active_players.push_back(req.playerId);
            sort(state.active_players.begin(), state.active_players.end());
            
            if (state.current_turn == 0) {
                state.current_turn = state.active_players[0]; 
                cout << "[ Logic ] P" << req.playerId << " 등록 완료. P" << state.current_turn << " 턴 부여." << endl;
            } else {
                cout << "[ Logic ] P" << req.playerId << " 등록. 현재 참여자 수: " << state.active_players.size() << endl;
            }
            pthread_mutex_unlock(&lock); 
            return;
        }
        
        // 2. 턴 검사 
        if (req.playerId != state.current_turn) {
            cout << "[ Logic ] P" << req.playerId << " 접근 거절 (턴이 아님)." << endl;
            pthread_mutex_unlock(&lock);
            return;
        }

        // 3. 숫자 업데이트 및 유효성 검사
        if (req.count < 1 || req.count > 3 || (state.current_num + req.count) > MAX_NUM) {
             cout << "[ Logic ] P" << req.playerId << " - 비정상 개수 (" << req.count << ") 요청 거부. 턴 유지." << endl;
             pthread_mutex_unlock(&lock);
             return;
        }

        // 숫자 증가 로직
        ostringstream shouted_sequence; 
        
        for (int i = 0; i < req.count; ++i) {
            state.current_num++;
            shouted_sequence << state.current_num;
            if (i < req.count - 1) {
                shouted_sequence << ", "; 
            }
        }
        cout << "[ Logic ] P" << req.playerId << " 외침 (" << req.count << "개): [" 
             << shouted_sequence.str() << "]!" << endl;


        // 4. 게임 오버 체크 및 턴 교체
        if (state.current_num >= MAX_NUM) {
            state.gameover = true;
            ostringstream oss;
            oss << "P" << req.playerId;
            strcpy(state.last_caller, oss.str().c_str());

            cout << "============================================" << endl;
            cout << "[ GAME OVER ] " << state.last_caller << "가 숫자 " << state.current_num << "를 외쳐 패배!" << endl;
            cout << "============================================" << endl;
            running = false; 
        } else {
            // 턴 교체 로직
            auto it = find(state.active_players.begin(), state.active_players.end(), state.current_turn);
            if (it != state.active_players.end()) {
                it++; 
                if (it == state.active_players.end()) {
                    it = state.active_players.begin();
                }
                state.current_turn = *it;
            }
        }
        
        pthread_mutex_unlock(&lock); 
    }

    // --- 3. 상태 브로드캐스트 쓰레드 (FIFO Write) ---
    static auto broadcastThreadFunc(void* arg) -> void* {
        Server* server = (Server*)arg;
        
        while (server->running) {
            string status;
            
            pthread_mutex_lock(&server->lock);
            
            // 상태 메시지 구성
            if (server->state.gameover) {
                status = "END: " + string(server->state.last_caller) + " is the loser. Final number: " + to_string(server->state.current_num);
            } else {
                status = "CURRENT:" + to_string(server->state.current_num) + 
                         " TURN:" + to_string(server->state.current_turn) + 
                         " PARTICIPANTS:" + to_string(server->state.active_players.size());
            }

            // 모든 접속된 클라이언트에게 상태 전파 (연결 끊김 감지 및 정리 로직 포함)
            vector<int> disconnected_players; 

            for (const auto& pair : server->state.player_pipes) {
                int resp_fd = open(pair.second.c_str(), O_WRONLY | O_NONBLOCK);
                if (resp_fd != -1) {
                    write(resp_fd, status.c_str(), status.length() + 1);
                    close(resp_fd);
                } else if (errno == ENXIO) {
                    disconnected_players.push_back(pair.first);
                }
            }
            
            // 연결 끊긴 플레이어 정리
            for (int id : disconnected_players) {
                server->state.player_pipes.erase(id);
                server->state.active_players.erase(
                    remove(server->state.active_players.begin(), server->state.active_players.end(), id),
                    server->state.active_players.end()
                );
            }

            // [수정된 로그] 참여자 수 제거
            cout << "[ BCAST Thread ] 현재 숫자: " << server->state.current_num << ", 다음 턴: P" << server->state.current_turn << endl; 
            
            pthread_mutex_unlock(&server->lock);
            
            sleep(1); 
        }
        pthread_exit(nullptr);
    }
    
    // --- 쓰레드 관리 및 자원 해제 ---
    auto start() -> void {
        pthread_create(&receive_thread, nullptr, receiveThreadFunc, this);
        pthread_create(&broadcast_thread, nullptr, broadcastThreadFunc, this);
    }

    auto wait() -> void {
        pthread_join(receive_thread, nullptr);
        pthread_join(broadcast_thread, nullptr);
    }

    auto cleanUp() -> void {
        cout << "\n[ Server ] 종료 시그널 감지 (자원 해제 중...)" << endl;
        if (close(request_fd) == 0) { 
            cout << "[ Server ] 요청 FIFO 파일 디스크립터 닫기 완료." << endl;
        }

        if (unlink(REQUEST_FIFO) == 0) {
            cout << "[ Server ] 요청 FIFO (" << REQUEST_FIFO << ") 제거 완료." << endl;
        }
        for (const auto& pair : state.player_pipes) {
            unlink(pair.second.c_str());
        }
        pthread_mutex_destroy(&lock);
    }

    ~Server() {
         if (running) cleanUp();
    }
};

Server* server_sig = nullptr;
auto sigHandler(int signNo) { 
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
