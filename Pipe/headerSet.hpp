#pragma once // 다른 헤더 파일 추가 시 거기에도 선언 필수

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <string>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

using namespace std;

#define SHM_KEY 60019
#define SEM_KEY_01 60018
#define SEM_KEY_02 60017
// #define MSG_KEY 60014
#define MAX_NUM 31
#define PIPE_PATH "/tmp/br31_server_fifo"

// struct MsgQueue {
//     long msg_type;
//     char msg_text[100];
// }; // message queue 구조체

struct SharedData {
    int current_num; // 현재 숫자
    int current_turn; // 현재 턴
    int current_cnt; // 클라이언트가 외친 숫자의 개수
    char last_caller[20];
    bool gameover;
}; // 공유 메모리 구조체