CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude

TARGET = bank_system

SOURCES = main.cpp src/admin.cpp src/user.cpp src/sha256.cpp src/result.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)