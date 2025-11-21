#include "headerSet.hpp"
#include <vector>

using namespace std;

#define PLAYER_ID 2

class ShrClient02 {
    int shmId;
    SharedData* data;
    int playerId;
    vector<int> moveSequence;
    size_t sequenceIndex;
    bool running;

public:
    ShrClient02() : shmId(-1), data(nullptr), playerId(PLAYER_ID), sequenceIndex(0), running(true) {
        // 성능 측정용 고정 시퀀스: 2,4,6,8,10,12,14,16,18,20,22,24,26,28,30 (짝수)
        moveSequence = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};

        // 공유메모리 접근
        shmId = shmget(SHM_KEY, sizeof(SharedData), 0666);
        if (shmId == -1) {
            perror("shmget failed");
            exit(1);
        }
        data = (SharedData*)shmat(shmId, nullptr, 0);
        if (data == (void*)-1) {
            perror("shmat failed");
            exit(1);
        }

        cout << "================================================" << endl;
        cout << "[ 클라이언트 02 ] Baskin-Robbins 31 게임 시작됨" << endl;
        cout << "[ 클라이언트 02 ] IPC: 공유메모리" << endl;
        cout << "[ 클라이언트 02 ] 플레이어 ID: " << playerId << endl;
        cout << "[ 클라이언트 02 ] 시퀀스: 2,4,6,8,10,12,14,16,18,20,22,24,26,28,30 (짝수)" << endl;
        cout << "================================================" << endl;
    }

    auto sendMove(int count) -> bool {
        // 공유메모리에 직접 값 업데이트
        data->current_num += count;
        cout << "[ 클라이언트 02 ] " << count << "를 말합니다 -> 현재 숫자: " << data->current_num << endl;
        
        // 게임 종료 판정
        if (data->current_num >= MAX_NUM) {
            data->gameover = true;
            strncpy(data->last_caller, "P2", sizeof(data->last_caller) - 1);
            cout << "[ 클라이언트 02 ] 31을 말했습니다! 게임 종료!" << endl;
            return false;  // 게임 종료 신호
        }
        
        // 턴 변경
        data->current_turn = (data->current_turn == 1 ? 2 : 1);
        return true;
    }

    auto play() -> void {
        // 모든 플레이어가 준비될 때까지 대기
        cout << "[ 클라이언트 02 ] 모든 플레이어 준비 대기 중..." << endl;
        sleep(2);

        int prevTurn = -1;
        
        while (running && !data->gameover) {
            // 현재 상태 확인
            int currentTurn = data->current_turn;
            
            // 턴이 바뀌었으면 출력
            if (prevTurn != currentTurn) {
                cout << "[ 클라이언트 02 ] 현재 턴: P" << currentTurn << ", 숫자: " << data->current_num << endl;
                prevTurn = currentTurn;
            }
            
            // 내 차례 대기
            if (currentTurn != playerId) {
                sleep(1);
                continue;
            }
            
            // 게임 종료 확인
            if (data->gameover) break;

            // 시퀀스에서 다음 값 가져오기
            if (sequenceIndex >= moveSequence.size()) {
                cout << "[ 클라이언트 02 ] 시퀀스 완료" << endl;
                break;
            }

            int count = moveSequence[sequenceIndex];
            sequenceIndex++;

            // 서버에 이동 전송 (공유메모리 직접 수정)
            if (!sendMove(count)) {
                // 게임 종료
                break;
            }

            sleep(1);
        }

        if (data->gameover) {
            cout << "\n[ 클라이언트 02 ] 게임 종료! " << data->last_caller << "이(가) 31을 말했습니다!" << endl;
        }
    }

    auto cleanUp() -> void {
        cout << "[ 클라이언트 02 ] 종료 중..." << endl;
        running = false;
        if (data != nullptr) shmdt(data);
    }

    ~ShrClient02() {
        cleanUp();
    }
};

int main() {
    ShrClient02 client;
    client.play();
    return 0;
}
