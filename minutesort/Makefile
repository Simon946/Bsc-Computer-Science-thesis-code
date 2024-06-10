.PHONY: all clean

INC_DIR=include
SRC_DIR=source
OBJ_DIR=obj
FILE_TYPE=cpp

CFLAGS=-I$(INC_DIR) -Wno-unused-result -DNDEBUG -Ofast
LDLIBS=-lpthread -lc
CC=g++

SOURCES = $(wildcard $(SRC_DIR)/*.$(FILE_TYPE))
MASTER_SRC = $(filter-out $(SRC_DIR)/worker%,$(SOURCES))
MASTER_OBJ = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(MASTER_SRC:.$(FILE_TYPE)=.o))
WORKER_SRC = $(filter-out $(SRC_DIR)/master%,$(SOURCES))
WORKER_OBJ = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(WORKER_SRC:.$(FILE_TYPE)=.o))

all: master worker

clean:
	rm -rf $(OBJ_DIR)/ master worker

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.$(FILE_TYPE)
	+@[ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

main: $(OBJECTS)
	echo $(SOURCES)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

master: $(MASTER_OBJ)
	echo $(MASTER_SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)


worker: $(WORKER_OBJ)
	echo $(WORKER_SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)
	cp -f ./worker ./das6_worker/worker
	


