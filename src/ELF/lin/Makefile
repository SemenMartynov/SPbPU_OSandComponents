CXX = g++
CXXFLAGS = -O2 -g -Wall -Werror -Wextra -pedantic -std=c++14 -Weffc++ -Wold-style-cast -Wconversion -Wsign-conversion -Wunreachable-code
LDFLAGS =

TARGET = netmonitor

OBJS = main.o parse.o

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

install:
	strip $(TARGET)
	cp $(TARGET) $(DESTDIR)/sbin/

.PHONY: all clean install
