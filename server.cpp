#include "headerSet.hpp"

// system call -> C style
// struct of logic -> Modern C++

//   [ SRP : 단일 책임 원칙 ] 게임 상태(숫자/턴/종료)를 관리하는 역할만 담당
//   [ 캡슐화 ] 상태값(private) 접근(getter/setter), 경쟁 조건 방지(pthread_mutex)
class GameState {
    SharedData* data;
    pthread_mutex_t lock;
public:
    explicit GameState(SharedData* ptr) : data{ptr} {
        pthread_mutex_init(&lock, nullptr);
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
        for (int i = 0; i < cnt; i++) {
            data->current_num++;
            cout << "[ State ] number = " << data->current_num << endl;
            usleep(200000);
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

//   [ SRP : 단일 책임 원칙 ] 게임 규칙만 처리(턴 검증, 숫자 갱신, 종료 판단)
//   [ DIP : 의존 역전 원칙 ] GameState 추상화에 의존(상태 관리 방법이 바뀌어도 영향 없음)
class GameLogic {
    GameState& state;
public:
    explicit GameLogic(GameState& s) : state{s} {}
    void applyMove(int playerId, int cnt) {
        if (state.isGameOver()) return;
        if (playerId != state.getTurn()) {
            cout << "[ logic ] 잘못된 턴 접근 P" << playerId << endl;
            return;
        }
        state.updateNumber(cnt);
        if (state.getNumber() >= MAX_NUM) {
            state.setGameOver("P" + to_string(playerId));
            cout << "[ logic ] Game Over ( by P" << playerId << " )" << endl;
            return;
        }
        state.switchTurn();
    }
};

//   IReceiver (추상 클래스 / 인터페이스)
//   [ ISP : 인터페이스 분리 원칙 ] 입력 수신 기능만 정의
//   [ DIP : 의존 역전 원칙 ] ServerApp은 구체 클래스(Pipe, Semaphore)에 의존하지 않음
class IReceiver {
public:
    virtual void start() = 0; // 수신 루프 실행
    virtual void stop() = 0;  // 안전 종료
    virtual ~IReceiver() = default;
};

//   [ OCP : 개방 폐쇄 원칙 ] -> 새로운 수신 채널로 확장이 가능
//   [ LSP : 리스코프 치환 법칙 ] -> IReceiver 기능 대체 가능
class PipeReceiver : public IReceiver {
    int pipeFd;
    GameLogic& logic;
    bool running = true;
public:
    PipeReceiver(int fd, GameLogic& gl) : pipeFd{fd}, logic{gl} {}
    void start() override {
        char buf[100];
        while (running) {
            memset(buf, 0, sizeof(buf));
            ssize_t n = read(pipeFd, buf, sizeof(buf));
            if (n > 0) {
                int pid{}, cnt{};
                sscanf(buf, "%d %d", &pid, &cnt);
                cout << "[ PIPE ] P" << pid << " -> " << cnt << endl;
                logic.applyMove(pid, cnt);
            }
            usleep(200000);
        }
    }
    void stop() override { running = false; }
};

//   [ OCP : 개방 폐쇄 원칙 ] -> 세마포어 IPC 기반 입력 수신 채널
//   [ SRP : 단일 책임 원칙 ] -> 세마포어를 통한 동기적 입력 수신만 담당
class SemaphoreReceiver : public IReceiver {
    int semId;
    GameLogic& logic;
    bool running = true;
public:
    SemaphoreReceiver(int id, GameLogic& gl) : semId{id}, logic{gl} {}
    void start() override {
        sembuf sops{};
        sops.sem_num = 0;
        sops.sem_flg = 0;

        while (running) {
            sops.sem_op = -1;

            if (semop(semId, &sops, 1) == -1) {
                if (errno == EINTR) continue;
                perror("semop wait");
                break;
            }

            cout << "[ Semaphore ] 신호 감지 (턴 진행)" << endl;

            int playerId = 1; 
            int cnt = 1;
            logic.applyMove(playerId, cnt);
            sops.sem_op = 1;

            if (semop(semId, &sops, 1) == -1) {
                perror("semop release");
                break;
            }
            usleep(300000);
        }
    }
    void stop() override { running = false; }
};

//   [ SRP : 단일 책임 원칙 ] -> 출력 역할만 담당
class Broadcaster {
    GameState& state;
    bool running = true;
public:
    explicit Broadcaster(GameState& s) : state{s} {}
    void stop() { running = false; }
    void start() {
        while (running && !state.isGameOver()) {
            cout << "[ Broadcast ] number = " << state.getNumber()
                 << ", Next = P" << state.getTurn() << endl;
            usleep(300000);
        }
        if (state.isGameOver()) {
            cout << "[ Broadcast ] 종료 : " << state.getCaller() << endl;
        }
    }
};

//   [ SRP : 단일 책임 원칙 ] -> 서버 실행/스레드 관리 전담
//   [ DIP : 의존 역전 원칙 ] -> IReceiver 추상 클래스에만 의존
//   [ OCP : 개방 폐쇄 원칙 ] -> Receiver 추가 시 기존 코드 변경 없음
class ServerApp {
    GameState& state;
    GameLogic& logic;
    Broadcaster& bc;
    vector<IReceiver*> receivers;
public:
    ServerApp(GameState& s, GameLogic& l, Broadcaster& b) : state{s}, logic{l}, bc{b} {}
    void addReceiver(IReceiver* r) { receivers.emplace_back(r); }
    static void* receiverThread(void* arg) {
        reinterpret_cast<IReceiver*>(arg)->start();
        return nullptr;
    }
    static void* bcThread(void* arg) {
        reinterpret_cast<Broadcaster*>(arg)->start();
        return nullptr;
    }
    void run() {
        cout << "============================" << endl;
        cout << "[ Server ] BR31 Server Start" << endl;
        cout << "============================" << endl;
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
        cout << "[ Server ] 모든 스레드 종료 요청 완료" << endl;
    }
};

ServerApp* g_server = nullptr;

int main() {
    signal(SIGINT, [](int) {
        cout << "[ Server ] SIGINT(CTRL+C) 감지 -> 서버 종료" << endl;
        if (g_server) g_server->stopAll();
        exit(0);
    });

    // Advanced IPC (공유 메모리)
    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmId == -1) { perror("shmget"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat"); return 1; }
    memset(shared, 0, sizeof(SharedData));
    shared->current_turn = 1;

    // Advanced IPC (세마포어)
    int semId = semget(SEM_KEY, 1, 0666 | IPC_CREAT);
    if (semId == -1) { perror("semget"); return 1; }
    semctl(semId, 0, SETVAL, 1); // 이진 세마포어 (초기값 1)

    // 단방향 IPC (PIPE -> fifo)
    if (mkfifo(PIPE_PATH, 0666) == -1 && errno != EEXIST) {
        perror("PIPE(mkfifo)");
        return 1;
    }
    int pipeFd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);

    GameState state(shared);
    GameLogic logic(state);
    Broadcaster bc(state);

    SemaphoreReceiver sr(semId, logic);
    PipeReceiver pr(pipeFd, logic);

    ServerApp server(state, logic, bc);
    server.addReceiver(&sr);
    server.addReceiver(&pr);

    g_server = &server;
    server.run();

    pause();

    return 0;
}