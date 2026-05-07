CXX = g++
CXXFLAGS = -std=c++17 -I..
LDFLAGS = -pthread

# 빌드할 타겟 파일 이름
TARGET = joystick_test

# 소스 및 헤더 파일
SRCS = main.cpp ../joystick.cpp
HDRS = ../joystick.h

# 기본 동작: make를 치면 joystick_test를 빌드합니다.
all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@ $(LDFLAGS)

# make clean을 치면 빌드된 파일을 삭제합니다.
clean:
	rm -f $(TARGET)
