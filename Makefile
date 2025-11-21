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
	rm -f $(TARGETS) pipe_server pipe_client1 pipe_client2 *.o *.log

# 간단 실행: 서버를 백그라운드로 띄우고 클라이언트들을 실행합니다.
run: pipe_server pipe_client1 pipe_client2
	@ipcrm -a 2>/dev/null || true
	@rm -f /tmp/br31_server_fifo 2>/dev/null || true
	@(./pipe_server &) 
	@sleep 3
	@(./pipe_client1 &)
	@sleep 2
	@(./pipe_client2 &)
	@sleep 20
	@pkill -f "pipe_server|pipe_client" 2>/dev/null || true

# pipe build targets (allow running pipe server/clients from repo root)
pipe_server: pipe_server.cpp
	$(CXX) $(CXXFLAGS) pipe_server.cpp -o pipe_server $(LDLIBS)

pipe_client1: pipe_client_01.cpp
	$(CXX) $(CXXFLAGS) pipe_client_01.cpp -o pipe_client1 $(LDLIBS)

pipe_client2: pipe_client_02.cpp
	$(CXX) $(CXXFLAGS) pipe_client_02.cpp -o pipe_client2 $(LDLIBS)
