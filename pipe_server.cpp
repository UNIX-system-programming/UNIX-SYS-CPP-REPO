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
    
    auto updateNumber(int cnt) -> void {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < cnt; ++i) {
            data->current_num++;
            safePrint("[ State ] number = " + to_string(data->current_num));
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
        
        safePrint("[ Pipe ] 신호 감지 ( 턴 진행 P" + to_string(playerId) + " )");
        state.updateNumber(cnt);
        
        if (state.getNumber() >= MAX_NUM) {
            state.setGameOver("P" + to_string(playerId));
            safePrint("[ logic ] GAME OVER ( 패배한 클라이언트 프로세스 -> P" + to_string(playerId) + "!! )");
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
        safePrint("[ Broadcast ] number = " + to_string(state.getNumber()) +
                  ", Next turn P" + to_string(state.getTurn()));
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
    shared->current_turn = 1;

    // FIFO 준비
    if (mkfifo(PIPE_PATH, 0666) == -1 && errno != EEXIST) { perror("mkfifo"); }
    int pipeFd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) { perror("open fifo"); return 1; }

    GameState state(shared);
    GameLogic logic(state);
    Broadcaster bc(state);
    PipeReceiver receiver(pipeFd, logic);

    safePrint("============================");
    safePrint("[ Pipe Server ] BR31 Pipe Server Start!!");
    safePrint("============================");

    int number = 0;

    // 메인 스레드 루프: 메시지 수신 및 게임 진행
    while (number < MAX_NUM) {
        int playerId = 0, cnt = 0;
        
        // 클라이언트 메시지 대기 (폴링)
        if (receiver.readMessage(playerId, cnt)) {
            logic.applyMove(playerId, cnt);
            
            if (!state.isGameOver()) {
                // 매번 상태 브로드캐스트 (세마포어 방식과 동일)
                bc.broadcast();
                number = state.getNumber();
            }
        } else {
            usleep(100000);  // 메시지 없으면 대기
        }
    }

    // 게임 종료
    safePrint("[ Broadcast ] 패배한 클라이언트 프로세스 : " + state.getCaller());

    // IPC 정리
    shmdt(shared);
    shmctl(shmId, IPC_RMID, nullptr);
    close(pipeFd);
    unlink(PIPE_PATH);
    safePrint("[ Pipe Server ] 종료 및 IPC 정리 완료");

    return 0;
}
