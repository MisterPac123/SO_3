# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2019-20

SOURCES = main.c fs.c sync.c
SOURCES+= lib/bst.c
OBJS_NOSYNC = $(SOURCES:%.c=%.o)
OBJS_MUTEX  = $(SOURCES:%.c=%-mutex.o)
OBJS_RWLOCK = $(SOURCES:%.c=%-rwlock.o)
OBJS = $(OBJS_NOSYNC) $(OBJS_MUTEX) $(OBJS_RWLOCK)
CC   = gcc
LD   = gcc
CFLAGS =-Wall -std=gnu99 -I../ -g
LDFLAGS=-lm -pthread
TARGETS = tecnicofs-rwlock client-api-test-create client-api-test-delete client-api-test-read client-api-test-success

.PHONY: all clean

all: $(TARGETS)

$(TARGETS):
	$(LD) $(CFLAGS) $^ -o $@ $(LDFLAGS)



lib/bst.o: lib/bst.c lib/bst.h
lib/hash.o: lib/hash.c lib/hash.h
lib/inodes.o: lib/inodes.c lib/inodes.h
fs.o: fs.c fs.h lib/bst.h
sync.o: sync.c sync.h constants.h

### RWLOCK ###
lib/bst-rwlock.o: CFLAGS+=-DRWLOCK
lib/bst-rwlock.o: lib/bst.c lib/bst.h
lib/hash.o: lib/hash.c lib/hash.h

fs-rwlock.o: CFLAGS+=-DRWLOCK
fs-rwlock.o: fs.c fs.h lib/bst.h

sync-rwlock.o: CFLAGS+=-DRWLOCK
sync-rwlock.o: sync.c sync.h constants.h

main-rwlock.o: CFLAGS+=-DRWLOCK
main-rwlock.o: main.c fs.h lib/bst.h constants.h lib/timer.h lib/hash.h lib/inodes.h sync.h
tecnicofs-rwlock: lib/bst-rwlock.o lib/hash.o fs-rwlock.o sync-rwlock.o lib/inodes.o main-rwlock.o

###client###
tecnicofs-api.o: tecnicofs-client-api.c tecnicofs-client-api.h tecnicofs-api-constants.h

###tests###
client-api-test-create: client-api-test-create.c tecnicofs-api.o
client-api-test-delete: client-api-test-delete.c tecnicofs-api.o
client-api-test-read: client-api-test-read.c tecnicofs-api.o
client-api-test-success: client-api-test-success.c tecnicofs-api.o


%.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)
