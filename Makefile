# C++ 컴파일러(g++)
CXX = g++

# 컴파일 옵션 : -Wall(모든 경고 메세지 표시), -pthread(POSIX 스레드 라이브러리 링크)
CXXFLAGS = -std=c++17 -Wall -pthread

# make all 명령어 사용해 세 개의 C++ 파일 동시에 빌드
TARGETS = pipe_server pipe_client1 pipe_client2

# make 기본 옵션이 make all (전부 실행한다는 말)
all: $(TARGETS)
	@echo "\033[32m[ ALL BUILT ] 모든 타깃 빌드 완료\033[0m"

# 파이프 서버.cpp 및 헤더셋.hpp 파일 둘 다 있어야 실행 가능
pipe_server: pipe_server.cpp headerSet.hpp
	@echo "\033[36m[ BUILD ]\033[0m pipe_server.cpp -> pipe_server"
	$(CXX) $(CXXFLAGS) -o pipe_server pipe_server.cpp

# 첫 번째 파이프 클라이언트 컴파일 명령
pipe_client1: pipe_client_01.cpp headerSet.hpp
	@echo "\033[36m[ BUILD ]\033[0m pipe_client_01.cpp -> pipe_client1"
	$(CXX) $(CXXFLAGS) -o pipe_client1 pipe_client_01.cpp

# 두 번째 파이프 클라이언트 컴파일 명령
pipe_client2: pipe_client_02.cpp headerSet.hpp
	@echo "\033[36m[ BUILD ]\033[0m pipe_client_02.cpp -> pipe_client2"
	$(CXX) $(CXXFLAGS) -o pipe_client2 pipe_client_02.cpp

run: $(TARGETS)
	@echo "[ 서버 | 클라이언트 프로세스 P1 | 클라이언트 프로세스 P2 ] 순차 실행 시작"
	@ipcrm -a 2>/dev/null || true; \
	rm -f /tmp/br31_server_fifo 2>/dev/null || true; \
	./pipe_server & \
	sleep 2; \
	while ! [ -p /tmp/br31_server_fifo ]; do sleep 0.5; done; \
	echo "[ WAIT OK ] 서버 FIFO 초기화 완료"; \
	./pipe_client1 & \
	sleep 1; \
	./pipe_client2 & \
	wait

# make clean 명령 실행 시 생성된 파일 모두 삭제
clean:
	@echo "\033[31m[ CLEAN ] 빌드된 모든 실행 파일 삭제\033[0m"
	rm -f $(TARGETS) server client01 client02 shr_client_01 shr_client_02 *.o *.log
	@echo "\033[31m[ CLEAN DONE ]\033[0m"

.PHONY: all clean run
