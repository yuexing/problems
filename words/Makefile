CXX=g++
CXXFLAGS=-Wall

OBJ=obj
BIN=$(OBJ)/bin

all: $(BIN)
$(BIN):
	mkdir -p $@

SOURCES=dict.cpp words.cpp
OBJECTS=$(SOURCES:%.cpp=$(OBJ)/%.o)

$(OBJ)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

EXECUTABLE=words

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $(BIN)/$@

.PHONY: clean

clean:
	rm -rf $(OBJ)
