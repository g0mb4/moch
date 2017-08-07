CXX = g++ -std=c++11
CXXFLAGS=-I./include -Wall -Wwrite-strings

CXX_SRV_LIBS=-lpthread
CXX_CLNT_LIBS=-lpthread -lncurses

SRV_SRC=$(wildcard srv/*.cpp)
SRV_OBJS=$(patsubst srv/%.cpp, build/srv/%.o, $(SRV_SRC))

CLNT_SRC=$(wildcard clnt/*.cpp)
CLNT_OBJS=$(patsubst clnt/%.cpp, build/clnt/%.o, $(CLNT_SRC))

all: srv clnt

$(SRV_OBJS): build/srv/%.o : srv/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CLNT_OBJS): build/clnt/%.o : clnt/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

srv: $(SRV_OBJS)
	$(CXX) -o build/moch_srv $(SRV_OBJS) $(CXX_SRV_LIBS)

clnt: $(CLNT_OBJS)
	$(CXX) -o build/moch_clnt $(CLNT_OBJS) $(CXX_CLNT_LIBS)
clean:
	rm -f build/srv/*.o
	rm -f build/srv/moch_srv
	rm -f build/clnt/*.o
	rm -f build/clnt/moch_clnt
