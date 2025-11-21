#include "headerSet.hpp"

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

void safePrint(const string& msg) {
    pthread_mutex_lock(&print_lock);
    cout << msg << endl;
    cout.flush();
    pthread_mutex_unlock(&print_lock);
}

// [ SRP ] 게임 상태 관리
class GameState {
    SharedData* data;
    pthread_mutex_t lock;
public:
    explicit GameState(SharedData* ptr) : data{ptr} {
        pthread_mutex_init(&lock, nullptr);
    }
    
    auto getCnt() -> int {
        pthread_mutex_lock(&lock);
        int c = data->current_cnt;
        pthread_mutex_unlock(&lock);
        return c;
    }
    
    auto isGameOver() -> bool {
        pthread_mutex_lock(&lock);
        bool over = data->gameover;
        pthread_mutex_unlock(&lock);
        return over;
    }
    
    auto getNumber() -> int {
        pthread_mutex_lock(&lock);
        int n = data->current_num;
        pthread_mutex_unlock(&lock);
        return n;
    }
    
    auto getTurn() -> int {
        pthread_mutex_lock(&lock);
        int t = data->current_turn;
        pthread_mutex_unlock(&lock);
        return t;
    }
    
    auto getCaller() -> string {
        pthread_mutex_lock(&lock);
        string s = data->last_caller;
        pthread_mutex_unlock(&lock);
        return s;
    }
    
    auto updateNumber(int cnt, int callerId) -> void {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < cnt; ++i) {
            data->current_num++;
            safePrint("[ Client P" + to_string(callerId) + " ] 외친 숫자 = " + to_string(data->current_num));
            usleep(250000);
        }
        pthread_mutex_unlock(&lock);
    }
    
    auto switchTurn() -> void {
        pthread_mutex_lock(&lock);
        data->current_turn = (data->current_turn == 1 ? 2 : 1);
        pthread_mutex_unlock(&lock);
    }
    
    auto setGameOver(const string& caller) -> void {
        pthread_mutex_lock(&lock);
        data->gameover = true;
        strncpy(data->last_caller, caller.c_str(), sizeof(data->last_caller));
        pthread_mutex_unlock(&lock);
    }
    
    ~GameState() {
        pthread_mutex_destroy(&lock);
    }
};

// [ SRP ] 게임 규칙 적용
class GameLogic {
    GameState& state;
public:
    explicit GameLogic(GameState& s) : state{s} {}
    
    GameState& getState() { return state; }
    
    void applyMove(int playerId, int cnt) {
        if (state.isGameOver()) return;
        if (playerId != state.getTurn()) {
            safePrint("[ logic ] 잘못된 턴 접근 P" + to_string(playerId));
            return;
        }
        
        state.updateNumber(cnt, playerId);
        
        if (state.getNumber() >= MAX_NUM) {
            state.setGameOver("P" + to_string(playerId));
            safePrint("[ Result ] GAME OVER ( 패배한 클라이언트 -> P" + to_string(playerId) + "!! )");
            return;
        }
        
        state.switchTurn();
    }
};

// [ SRP ] 브로드캐스터 (매번 상태 출력)
class Broadcaster {
    GameState& state;
public:
    explicit Broadcaster(GameState& s) : state{s} {}
    
    void broadcast() {
        safePrint("[ Broadcast ] ( 턴 교체 -> 다음 턴 P" + to_string(state.getTurn()) + " )");
    }
};

// 클라이언트 신호 수신 (메인 스레드에서 처리)
class PipeReceiver {
    int pipeFd;
    GameLogic& logic;
public:
    PipeReceiver(int fd, GameLogic& gl) : pipeFd{fd}, logic{gl} {}
    
    bool readMessage(int& playerId, int& cnt) {
        char buf[128];
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(pipeFd, buf, sizeof(buf));
        
        if (n > 0) {
            sscanf(buf, "%d %d", &playerId, &cnt);
            return true;
        }
        return false;
    }
};

volatile sig_atomic_t stop_requested = 0;

void handle_sigint(int) {
    stop_requested = 1;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.tie(nullptr);
    setvbuf(stdout, NULL, _IONBF, 0);

    // IPC 초기화
    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmId == -1) { perror("shmget"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat"); return 1; }
    memset(shared, 0, sizeof(SharedData));
    shared->current_turn = 0;

    // FIFO 준비
    if (mkfifo(PIPE_PATH, 0666) == -1 && errno != EEXIST) { perror("mkfifo"); }
    int pipeFd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) { perror("open fifo"); return 1; }

    GameState state(shared);
    GameLogic logic(state);
    Broadcaster bc(state);
    PipeReceiver receiver(pipeFd, logic);

    safePrint("============================");
    safePrint("[ Server ] BR31 Server Start!!");
    safePrint("============================");

    int number = 0;

    // 시그널 핸들러 등록 (Ctrl+C 등)
    struct sigaction sa{};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // 메인 스레드 루프: 서버가 먼저 턴을 알리고 클라이언트가 응답하도록 턴 단위 처리
    int turn = 1;
    while (number < MAX_NUM && !stop_requested) {
        // 서버가 턴 알림(프롬프트)을 먼저 출력
        safePrint("[    Pipe   ] ( 신호 감지 -> 턴 진행 P" + to_string(turn) + " )");
        /* assign shared turn directly so clients observe server prompt then act */
        shared->current_turn = turn;

        // 해당 턴의 클라이언트 메시지를 기다림
        int playerId = 0, cnt = 0;
        while (!stop_requested) {
            if (receiver.readMessage(playerId, cnt)) {
                if (playerId != turn) {
                    safePrint("[ logic ] 잘못된 턴 접근 P" + to_string(playerId));
                    continue;
                }
                logic.applyMove(playerId, cnt);
                break;
            }
            usleep(100000);
        }

        if (state.isGameOver() || stop_requested) break;

        // 턴을 클리어하여 클라이언트가 다음 프롬프트 전에 외치지 않도록 함
        shared->current_turn = 0;

        // 브로드캐스트 및 상태 갱신
        int nextTurn = (turn == 1 ? 2 : 1);
        safePrint("[ Broadcast ] ( 턴 교체 -> 다음 턴 P" + to_string(nextTurn) + " )");
        number = state.getNumber();

        // 다음 턴
        turn = nextTurn;
        usleep(300000);
    }

    // 게임 종료 or 외부 요청
    if (stop_requested) {
        // 외부 시그널로 종료 요청이 들어오면 게임오버 플래그 설정
        // main 스레드에서 안전하게 설정
        safePrint("[ Server ] SIGINT/SIGTERM 수신 - 종료 처리 시작");
        // last caller may not be meaningful here
        shared->gameover = true;
    }

    safePrint("[ Broadcast ] 패배한 클라이언트 프로세스 : " + state.getCaller());

    // IPC 정리
    shmdt(shared);
    shmctl(shmId, IPC_RMID, nullptr);
    close(pipeFd);
    unlink(PIPE_PATH);
    safePrint("[ Server ] IPC 리소스 정리 완료");

    // 모든 관련 프로세스(클라이언트 포함)에 SIGINT를 보내 종료를 유도
    kill(0, SIGINT);

    return 0;
}
