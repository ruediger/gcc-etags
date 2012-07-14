CXX := g++
CXXFLAGS := -std=c++0x -Wall -Wextra -I`$(CXX) -print-file-name=plugin`/include -fPIC

OBJECTS := gccetags.o
TARGET := gccetags.so

.PHONY: all clean distclean test

all: $(TARGET)

# GNU Make does not provide implicit rules for .c++ suffix
$(OBJECTS): %.o: %.c++
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -shared $< -o $@

test: $(TARGET)
	$(CXX) -S -fplugin=./$(TARGET) test.c++

clean:
	$(RM) $(OBJECTS)

distclean:
	$(RM) $(OBJECTS) $(TARGET)
