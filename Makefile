CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Iinclude
LDLIBS   := $(shell pkg-config --libs libarchive)
CXXFLAGS += $(shell pkg-config --cflags libarchive)

PREFIX  := /usr
BINDIR  := $(PREFIX)/bin
DESTDIR ?=
TARGET   := faro

SOURCES  := $(wildcard src/*.cpp)
OBJECTS  := $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f src/*.o $(TARGET)

.PHONY: install

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
