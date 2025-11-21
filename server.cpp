#include "headerSet.hpp"

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

// 출력 순서 꼬임 문제 해결 용도로 사용하는 동기화
void safePrint(const string& msg) {
    pthread_mutex_lock(&print_lock);
    cout << msg << endl;
    pthread_mutex_unlock(&print_lock);
}

// system call -> C style
// struct of logic -> Modern C++
// server.cpp 코드는 Advanced IPC(공유 메모리, 세마포어), 파이프 클라이언트 모두 사용 가능

// [ SRP : 단일 책임 원칙 ] 게임 상태(숫자/턴/종료)를 관리하는 역할만 담당
// [ 캡슐화 ] 상태값(private) 접근(getter/setter), 경쟁 조건 방지(pthread_mutex)
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
        for (int i = 0; i < cnt; i++) {
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

// [ SRP : 단일 책임 원칙 ] 게임 규칙만 처리(턴 검증, 숫자 갱신, 종료 판단)
// [ DIP : 의존 역전 원칙 ] GameState 추상화에 의존(상태 관리 방법이 바뀌어도 영향 없음)
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
        state.updateNumber(cnt);
        if (state.getNumber() >= MAX_NUM) {
            state.setGameOver("P" + to_string(playerId));
            safePrint("[ logic ] GAME OVER ( 패배한 클라이언트 프로세스 -> P" + to_string(playerId) + "!! )");
            return;
        }
        state.switchTurn();
    }
};

// IReceiver (추상 클래스 / 인터페이스)
class IReceiver {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual ~IReceiver() = default;
};

// [ OCP : 개방 폐쇄 원칙 ] -> 세마포어 IPC 기반 입력 수신 채널
// [ SRP : 단일 책임 원칙 ] -> 세마포어를 통한 동기적 입력 수신만 담당
class SemaphoreReceiver : public IReceiver {
    int semId;
    int playerId;
    GameLogic& logic;
    bool running = true;
public:
    SemaphoreReceiver(int id, int pid, GameLogic& gl) : semId{id}, playerId{pid}, logic{gl} {}
    void start() override {
        sembuf sops{};
        sops.sem_num = 0;
        sops.sem_flg = 0;

        while (running) {
            if (logic.getState().isGameOver()) break;

            struct semid_ds buf;
            if (semctl(semId, 0, IPC_STAT, &buf) == -1) {
                safePrint("[ SemaphoreReceiver ] sem removed, exiting thread");
                break;
            }

            // 세마포어 P 연산
            sops.sem_op = -1;
            if (semop(semId, &sops, 1) == -1) {
                if (errno == EINTR) continue;
                if (errno == EINVAL || errno == EIDRM) break;
                perror("semop P");
                break;
            }

            safePrint("[ Semaphore ] ( 신호 감지 -> 턴 진행 P" + to_string(playerId) + " )");

            int cnt = logic.getState().getCnt();
            logic.applyMove(playerId, cnt > 0 ? cnt : 1);

            // 턴 교대 (다음 플레이어 세마포어 V 연산)
            int nextKey = (playerId == 1 ? SEM_KEY_02 : SEM_KEY_01);
            int nextSem = semget(nextKey, 1, 0666);
            if (nextSem != -1) {
                sembuf vop{};
                vop.sem_num = 0;
                vop.sem_op = 1;
                vop.sem_flg = 0;
                semop(nextSem, &vop, 1);
            }

            usleep(300000);
        }
    }
    void stop() override {
        running = false;
        sembuf sop{};
        sop.sem_num = 0;
        sop.sem_op = 1;
        sop.sem_flg = 0;
        semop(semId, &sop, 1);
    }
};

// [ SRP : 단일 책임 원칙 ] -> 출력 역할만 담당
class Broadcaster {
    GameState& state;
    bool running = true;
public:
    explicit Broadcaster(GameState& s) : state{s} {}
    void stop() { running = false; }
    void start() {
        while (running && !state.isGameOver()) {
            safePrint("[ Broadcast ] 다음 턴 P" + to_string(state.getTurn()));
            usleep(350000);
        }
        if (state.isGameOver()) {
            safePrint("[ Broadcast ] 패배한 클라이언트 프로세스 : " + state.getCaller());
        }
    }
};

// [ SRP : 단일 책임 원칙 ] -> 서버 실행/스레드 관리 전담
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
        safePrint("============================");
        safePrint("[ Server ] BR31 Server Start!!");
        safePrint("============================");

        for (auto& r : receivers) {
            pthread_t t{};
            pthread_create(&t, nullptr, receiverThread, r);
            pthread_detach(t);
        }

        sleep(2);
        pthread_t bt{};
        pthread_create(&bt, nullptr, bcThread, &bc);
        pthread_detach(bt);
    }

    void stopAll() {
        for (auto r : receivers) r->stop();
        bc.stop();
        safePrint("[ Server ] 모든 스레드 종료 요청 완료");
    }
};

ServerApp* g_server = nullptr;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.tie(nullptr);
    setvbuf(stdout, NULL, _IONBF, 0);

    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmId == -1) { perror("shmget"); return 1; }
    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) { perror("shmat"); return 1; }
    memset(shared, 0, sizeof(SharedData));
    shared->current_turn = 1;

    int semId1 = semget(SEM_KEY_01, 1, 0666 | IPC_CREAT);
    int semId2 = semget(SEM_KEY_02, 1, 0666 | IPC_CREAT);
    semctl(semId1, 0, SETVAL, 0);
    semctl(semId2, 0, SETVAL, 0);

    cout << "============================\n";
    cout << "[ Server ] BR31 Server Start!!\n";
    cout << "============================\n";

    sembuf sops{};
    sops.sem_num = 0;
    sops.sem_flg = 0;

    int number = 0;
    int turn = 1;

    while (number < MAX_NUM) {
        cout << "[ Semaphore ] ( 신호 감지 -> 턴 진행 P" << turn << " )" << endl;
        cout.flush();

        for (int i = 0; i < 2; i++) {
            int temp = (turn == 1) ? 1 : 2;
            number++;
            shared->current_num = number;
            cout << "[ Client P" << temp << " ] 외친 숫자 = " << number << endl;
            cout.flush();
            usleep(250000);
            if (number >= MAX_NUM) break;
        }

        if (number >= MAX_NUM) break;

        cout << "[ Broadcast ] ( 턴 교체 -> 다음 턴 P" << (turn == 1 ? 2 : 1) << " )" << endl;
        cout.flush();

        turn = (turn == 1 ? 2 : 1);
        shared->current_turn = turn;

        int nextSem = (turn == 1) ? semId1 : semId2;
        sops.sem_op = 1;
        semop(nextSem, &sops, 1);

        usleep(300000); 
    }

    cout << "[ Result ] GAME OVER ( 패배한 클라이언트 -> P" << (turn == 1 ? 1 : 2) << "!! )" << endl;
    cout.flush();

    shmdt(shared);
    shmctl(shmId, IPC_RMID, nullptr);
    semctl(semId1, 0, IPC_RMID);
    semctl(semId2, 0, IPC_RMID);
    cout << "[ Server ] IPC 리소스 정리 완료" << endl;
    cout.flush();

    kill(0, SIGINT);

    return 0;
}