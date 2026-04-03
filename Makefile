CXX      := g++
CXXFLAGS := -std=c++17 -I include -I /ucrt64/include -I /ucrt64/include/SDL2
LDFLAGS  := -L /ucrt64/lib -lmingw32 -lSDL2main -lSDL2 -lopengl32 \
            -lssl -lcrypto -lws2_32 -lcrypt32

IMGUI_SRC := $(wildcard src/imgui/*.cpp)

.PHONY: all gui terminal clean

all: gui terminal

gui: src/gui_test.cpp src/bayesian.cpp $(IMGUI_SRC)
	$(CXX) $(CXXFLAGS) $^ -o src/gui_test.exe $(LDFLAGS) -mconsole

terminal: src/main.cpp src/bayesian.cpp
	$(CXX) $(CXXFLAGS) $^ -o src/main.exe -lssl -lcrypto -lws2_32 -lcrypt32

clean:
	rm -f src/gui_test.exe src/main.exe
