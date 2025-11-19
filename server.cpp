#include "headerSet.hpp"

// system call -> C style
// struct of logic -> Modern C++

// [ SRP : 단일 책임 원칙 ] -> 상태 접근/갱신 기능만 담당
// [ 캡슐화 ] -> 데이터 보호 및 뮤텍스로 동기화
class GameState {
    SharedData *data;
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
        for (int i = 0; i < cnt; i++) {
            pthread_mutex_lock(&lock);
            data->current_num++;
            cout << "[ State ] number = " << data->current_num << endl;
            pthread_mutex_unlock(&lock);
            usleep(200000);
        }
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

// 비즈니스 로직 담당
// [ SRP : 단일 책임 원칙 ] -> 게임 규칙만 처리
// [ DIP : 의존 역전 원칙 ] -> GameState 클래스를 인터페이스처럼 사용
class GameLogic {
    GameState& state;
public:
    explicit GameLogic(GameState& s) : state(s) {}
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

// [ ISP : 인터페이스 분리 원칙 ] -> 입력 수신 기능만 제공 (추상 클래스)
// [ DIP : 의존 역전 원칙 ] -> 상위 모듈(ServerApp)이 추상화에 의존
class IReceiver {
public:
    // 순수 가상 함수는 상속 받는 자식 클래스에서 반드시 구현(오버라이딩)되어야 한다.
    // C++에선 자바와는 달리 virtual 키워드를 선언해야 실질적인 overriding이 된다. (안 쓰면 본인 객체의 결과가 나옴)
    virtual void start() = 0; // 순수 가상 함수를 의미함 (이게 있다면 추상 클래스 == 인터페이스)
    virtual ~IReceiver() = default;
};

// [ OCP : 개방 폐쇄 원칙 ] -> 새로운 수신 방식 추가 시 기존 코드 수정 없음
// [ LSP : 리스코프 치환 법칙 ] -> IReceiver 기능을 대체 가능
class MessageQueueReceiver : public IReceiver {
    int msgId;
    GameLogic& logic;
public:
    MessageQueueReceiver(int id, GameLogic& gl) : msgId{id}, logic{gl} {}
    void start() override {
        MsgQueue msg{};
        while (true) {
            if (msgrcv(msgId, &msg, sizeof(msg.msg_text), 1, 0) != -1) {
                int pid{}, cnt{};
                sscanf(msg.msg_text, "%d %d", &pid, &cnt);
                cout << "[ MessageQueue ] P" << pid << " -> " << cnt << endl;
                logic.applyMove(pid, cnt);
            }
        }
    }
};

// [ OCP : 개방 폐쇄 원칙 ] -> 새로운 수신 채널로 확장이 가능
// [ LSP : 리스코프 치환 법칙 ] -> IReceiver 기능 대체 가능
class PipeReceiver : public IReceiver {
    int pipeFd;
    GameLogic& logic;
public:
    PipeReceiver(int fd, GameLogic& gl) : pipeFd{fd}, logic{gl} {}
    void start() override {
        char buf[100];
        while (true) {
            memset(buf, 0, sizeof(buf));
            ssize_t n = read(pipeFd, buf, sizeof(buf));
            if (n > 0) {
                int pid{}, cnt{};
                sscanf(buf, "%d %d", &pid, &cnt);
                cout << "[ PIPE ] P" << pid << " -> " << cnt << endl;
                logic.applyMove(pid, cnt);
            }
            sleep(1);
        }
    }
};

// [ SRP : 단일 책임 원칙 ] -> 출력 역할만 담당
class Broadcaster {
    GameState& state;
public:
    explicit Broadcaster(GameState& s) : state{s} {}
    void start() {
        while (!state.isGameOver()) {
            cout << "[ Broadcast ] number = " << state.getNumber();
            cout << ", Next = P" << state.getTurn() << endl;
            sleep(3);
        }
        cout << "[ Broadcast ] 종료 : " << state.getCaller() << endl;
    }
};

// [ SRP : 단일 책임 원칙 ] -> 서버 실행/스레드 관리 전담
// [ DIP : 의존 역전 원칙 ] -> IReceiver 추상 클래스에만 의존
// [ OCP : 개방 폐쇄 원칙 ] -> Receiver 추가 시 기존 코드 변경 없음
class ServerApp {
    GameState& state;
    GameLogic& logic;
    Broadcaster& bc;
    vector<IReceiver*> receivers;
public:
    ServerApp(GameState& s, GameLogic& l, Broadcaster& b) : state{s}, logic{l}, bc{b} {}
    void addReceiver(IReceiver* r) {
        receivers.emplace_back(r); 
    }
    static void* receiverThread(void* arg) {
        static_cast<IReceiver*>(arg)->start();
        return nullptr;
    }
    static void* bcThread(void* arg) {
        static_cast<Broadcaster*>(arg)->start();
        return nullptr;
    }
    void run() {
        cout << "==========================" << endl;
        cout << "[Server] BR31 Server Start" << endl;
        cout << "==========================" << endl;

        for (auto &r : receivers) {
            pthread_t t{};
            pthread_create(&t, nullptr, receiverThread, r);
            pthread_detach(t);
        }

        pthread_t bt{};
        pthread_create(&bt, nullptr, bcThread, &bc);
        pthread_detach(bt);
    }
};

int main() {

    signal(SIGINT, [](int) {
        cout << "[ Server ] SIGINT(2) 입력 감지 (서버 종료)" << endl;
        exit(0);
    });

    int shmId = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmId == -1) { perror("shmget"); return 1; }

    SharedData* shared = (SharedData*)shmat(shmId, nullptr, 0);
    if (shared == (void*)-1) {
        perror("shmat");
        return 1;
    }
    memset(shared, 0, sizeof(SharedData));
    shared->current_turn = 1;

    int msgId = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msgId == -1) { perror("msgget"); return 1; }

    if (mkfifo(PIPE_PATH, 0666) == -1 && errno != EEXIST) {
        perror("PIPE(mkfifo)");
        return 1;
    }
    int pipeFd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);

    GameState state(shared);
    GameLogic logic(state); 
    Broadcaster bc(state);

    MessageQueueReceiver mq(msgId, logic);
    PipeReceiver pr(pipeFd, logic);

    ServerApp server(state, logic, bc);
    server.addReceiver(&mq);
    server.addReceiver(&pr);

    server.run();

    pause();

    return 0;
}