# Makefile for Baskin-Robbins 31 Game (Pipe Version)

CC = g++
CFLAGS = -std=c++17 -Wall -pthread # C++17 표준 및 쓰레드 라이브러리 링크 필수

SERVER_SRC = server_pipe.cpp
CLIENT_SRC = client_pipe.cpp
SERVER_BIN = server_pipe
CLIENT_BIN = client_pipe

PLAYER_COUNT = 3 # 실행할 클라이언트(플레이어) 수 설정

.PHONY: all clean build run start_clients stop

all: build

# --- 빌드 (컴파일) ---
build: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) $< -o $@

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $< -o $@

# --- 실행 ---
run: build start_server start_clients

# 서버 실행 (백그라운드 & 에러 출력을 로그 파일로 리다이렉션)
start_server:
	@echo "============================================"
	@echo "1. 서버를 백그라운드로 실행합니다..."
	@echo "============================================"
	@./$(SERVER_BIN) > server.log 2>&1 & # 서버 실행 & PID를 기록하지 않고 백그라운드 실행
	@sleep 1 # 서버가 FIFO를 생성할 시간을 줌

# 클라이언트 실행 (백그라운드)
start_clients:
	@echo "2. 클라이언트 $(PLAYER_COUNT)개를 백그라운드로 실행합니다..."
	@for i in $$(seq 1 $(PLAYER_COUNT)); do \
		./$(CLIENT_BIN) $$i > client_$$i.log 2>&1 & \
		echo "   -> [P$$i] 클라이언트 실행 완료 (로그: client_$$i.log)"; \
	done
	@echo "============================================"
	@echo "서버와 클라이언트가 백그라운드에서 실행 중입니다. (로그 확인 필요)"
	@echo "상태를 확인하려면 'tail -f server.log' 명령을 사용하세요."
	@echo "게임 종료 시 'make stop' 명령으로 정리해야 합니다."
	@echo "============================================"

# --- 정리 (Cleanup) ---
stop:
	@echo "3. 실행 중인 프로세스와 생성된 IPC 자원을 정리합니다..."
	# 서버 프로세스 (server_pipe) 종료
	@-pkill $(SERVER_BIN)
	# 클라이언트 프로세스 (client_pipe) 종료
	@-pkill $(CLIENT_BIN)
	# 생성된 바이너리 파일 삭제
	@-rm -f $(SERVER_BIN) $(CLIENT_BIN)
	# 로그 파일 및 FIFO 파일 삭제
	@-rm -f server.log client_*.log /tmp/br31_*

clean:
	@rm -f $(SERVER_BIN) $(CLIENT_BIN) *.log
