# C++ 컴파일러(g++)
CXX = g++

# 컴파일 옵션 : -Wall(모든 경고 메세지 표시), -pthread(POSIX 스레드 라이브러리 링크)
CXXFLAGS = -Wall -pthread   

# make all 명령어 사용해 세 개의 C++ 파일 동시에 빌드
TARGETS = sem_server sem_client_01 sem_client_02

# make 기본 옵션이 make all (전부 실행한다는 말)
all: $(TARGETS)
	@echo "\033[32m[ ALL BUILT ] 모든 타깃 빌드 완료\033[0m"

# 서버.cpp 및 헤더셋.hpp 파일 둘 다 있어야 실행 가능
sem_server: sem_server.cpp headerSet.hpp 
	@echo "\033[36m[ BUILD ]\033[0m sem_server.cpp -> sem_server"
	$(CXX) $(CXXFLAGS) -o sem_server sem_server.cpp
# 첫 번째 세마포어 클라이언트 컴파일 명령
sem_client_01: sem_client_01.cpp headerSet.hpp
	@echo "\033[36m[ BUILD ]\033[0m sem_client_01.cpp -> sem_client_01"
	$(CXX) $(CXXFLAGS) -o sem_client_01 sem_client_01.cpp

# 두 번째 세마포어 클라이언트 컴파일 명령
sem_client_02: sem_client_02.cpp headerSet.hpp
	@echo "\033[36m[ BUILD ]\033[0m sem_client_02.cpp -> sem_client_02"
	$(CXX) $(CXXFLAGS) -o sem_client_02 sem_client_02.cpp

run:
	@echo "[ 서버 | 클라이언트 프로세스 P1 | 클라이언트 프로세스 P2 ] 순차 실행 시작"
	@./sem_server & \
	sem_server_pid=$$!; \
	sleep 3; \
	( while ! ipcs -s | grep -q "60012"; do sleep 0.5; done; \
	  echo "[ WAIT OK ] 서버 IPC 완전 초기화 완료" ); \
	./sem_client_01 & \
	sleep 2; \
	./sem_client_02 & \
	wait $$sem_server_pid

# make clean 명령 실행 시 생성된 파일 모두 삭제
clean:
	@echo "\033[31m[ CLEAN ] 빌드된 모든 실행 파일 삭제\033[0m"
	rm -f $(TARGETS)
	@echo "\033[31m[ CLEAN DONE ]\033[0m"