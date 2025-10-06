CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
# Re-adicionando -lstdc++fs por segurança e compatibilidade
LDLIBS = -lncursesw -lstdc++fs
TARGET = device_viewer
SRCS = main.cpp sys_info.cpp
OBJS = $(SRCS:.cpp=.o)
.PHONY: all clean
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)
main.o: main.cpp sys_info.hpp
sys_info.o: sys_info.cpp sys_info.hpp
clean:
	rm -f $(OBJS) $(TARGET)