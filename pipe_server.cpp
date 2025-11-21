#include "headerSet.hpp"

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

// 출력 순서 꼬임 문제 해결 용도로 사용하는 동기화
void safePrint(const string& msg) {
    pthread_mutex_lock(&print_lock);
    cout << msg << endl;
    pthread_mutex_unlock(&print_lock);
}

// GameState: 공유 메모리 기반 상태 관리
class GameState {
    SharedData* data;
    pthread_mutex_t lock;
public:
    explicit GameState(SharedData* ptr) : data{ptr} { pthread_mutex_init(&lock, nullptr); }
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
    ~GameState() { pthread_mutex_destroy(&lock); }
};

// GameLogic: 게임 규칙 적용
class GameLogic {
    GameState& state;
public:
    explicit GameLogic(GameState& s) : state{s} {}
    void applyMove(int playerId, int cnt) {
        if (state.isGameOver()) return;
        if (playerId != state.getTurn()) {
            safePrint("[ logic ] 잘못된 턴 접근 P" + to_string(playerId));
            return;
        }
        state.updateNumber(cnt);
        if (state.getNumber() >= MAX_NUM) {
            state.setGameOver("P" + to_string(playerId));
            safePrint("[ logic ] GAME OVER ( 패배한 클라이언트 프로세스 -> P" + to_string(playerId) + " )");
            return;
        }
        state.switchTurn();
    }
    GameState& getState() { return state; }
};

// IReceiver 인터페이스
class IReceiver {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual ~IReceiver() = default;
};

// PipeReceiver: FIFO에서 클라이언트 메시지를 읽어 GameLogic에 전달
class PipeReceiver : public IReceiver {
    int pipeFd;
    GameLogic& logic;
    bool running = true;
public:
    PipeReceiver(int fd, GameLogic& gl) : pipeFd{fd}, logic{gl} {}
    void start() override {
        char buf[128];
        while (running && !logic.getState().isGameOver()) {
            memset(buf, 0, sizeof(buf));
            ssize_t n = read(pipeFd, buf, sizeof(buf));
            if (n > 0) {
                int pid = 0, cnt = 0;
                sscanf(buf, "%d %d", &pid, &cnt);
                safePrint("[ PIPE ] P" + to_string(pid) + " -> " + to_string(cnt));
                logic.applyMove(pid, cnt);
            } else {
                usleep(200000);
            }
        }
    }
    void stop() override { running = false; }
};

// Broadcaster: 상태 주기 출력
class Broadcaster {
    GameState& state;
    bool running = true;
public:
    explicit Broadcaster(GameState& s) : state{s} {}
    void stop() { running = false; }
    void start() {
        while (running && !state.isGameOver()) {
            safePrint("[ Broadcast ] number = " + to_string(state.getNumber()) + ", Next turn P" + to_string(state.getTurn()));
            usleep(350000);
        }
        if (state.isGameOver()) safePrint("[ Broadcast ] 패배한 클라이언트 프로세스 : " + state.getCaller());
    }
};

// ServerApp: 수신기와 방송기를 관리하는 OOP 서버 래퍼
class ServerApp {
    GameState& state;
    GameLogic& logic;
    Broadcaster& bc;
    vector<IReceiver*> receivers;
public:
    ServerApp(GameState& s, GameLogic& l, Broadcaster& b) : state{s}, logic{l}, bc{b} {}
    void addReceiver(IReceiver* r) { receivers.emplace_back(r); }
    static void* receiverThread(void* arg) { reinterpret_cast<IReceiver*>(arg)->start(); return nullptr; }
    static void* bcThread(void* arg) { reinterpret_cast<Broadcaster*>(arg)->start(); return nullptr; }
    void run() {
        safePrint("============================");
        safePrint("[ Pipe Server ] BR31 Pipe Server Start!!");
        safePrint("============================");

        for (auto& r : receivers) {
            pthread_t t{};
            pthread_create(&t, nullptr, receiverThread, r);
            pthread_detach(t);
        }

        pthread_t bt{};
        pthread_create(&bt, nullptr, bcThread, &bc);
        pthread_detach(bt);
    }
    void stopAll() {
        for (auto r : receivers) r->stop();
        bc.stop();
        safePrint("[ Pipe Server ] 모든 수신기 종료 요청 완료");
    }
};

ServerApp* g_server = nullptr;

int main() {
    signal(SIGINT, [](int){
        safePrint("[ Pipe Server ] SIGINT 감지 -> 종료 요청");
        if (g_server) g_server->stopAll();
        exit(0);
    });

    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmId == -1) { perror("shmget"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat"); return 1; }
    memset(shared, 0, sizeof(SharedData));
    shared->current_turn = 1;

    // FIFO 준비
    if (mkfifo(PIPE_PATH, 0666) == -1 && errno != EEXIST) { perror("mkfifo"); }
    int pipeFd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) { perror("open fifo"); }

    GameState state(shared);
    GameLogic logic(state);
    Broadcaster bc(state);

    PipeReceiver pr(pipeFd, logic);

    ServerApp server(state, logic, bc);
    server.addReceiver(&pr);
    g_server = &server;
    server.run();

    // 메인 스레드는 게임 종료까지 대기
    while (!state.isGameOver()) sleep(1);

    // 정리
    shmdt(shared);
    shmctl(shmId, IPC_RMID, nullptr);
    close(pipeFd);
    unlink(PIPE_PATH);
    safePrint("[ Pipe Server ] 종료 및 IPC 정리 완료");
    return 0;
}
