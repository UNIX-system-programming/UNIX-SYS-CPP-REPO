CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDLIBS := -pthread

SRCS := server.cpp shr_client_01.cpp shr_client_02.cpp
TARGETS := server client01 client02

.PHONY: all clean run

all: $(TARGETS)

server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server $(LDLIBS)

client01: shr_client_01.cpp
	$(CXX) $(CXXFLAGS) shr_client_01.cpp -o client01 $(LDLIBS)

client02: shr_client_02.cpp
	$(CXX) $(CXXFLAGS) shr_client_02.cpp -o client02 $(LDLIBS)

clean:
	rm -f $(TARGETS) *.o *.log

# 간단 실행: 서버를 백그라운드로 띄우고 클라이언트들을 실행합니다.
run: all
	./server &
	sleep 1
	./client01 &
	sleep 1
	./client02 &
