# ircdk
# by stx3plus1

SRC=src
CC=cc
SOURCE=$(wildcard src/*.c)
COMMIT="\"$(shell git rev-parse HEAD | head -c6)\""
CFLAGS=-O3 -std=c2x -Dcommit=$(COMMIT) -lssl -lcrypto

.PHONY: ircdk

ircdk: ${SOURCE}
	@echo "[$(CC)] $^ -> $@"
	@$(CC) -o $@ $^ $(CFLAGS)

unfuck: src/unfuck/unfuck.c
	@echo "[$(CC)] $^ -> $@"
	@$(CC) -o $@ $^ $(CFLAGS)

# run install with sudo/doas.
install: ircdk
ifeq ($(UNAME), $(filter $(UNAME) Darwin,FreeBSD))
	@echo "[in] $< -> /usr/local/bin/$<"
	@mkdir -p /usr/local/bin
	@cp $< /usr/local/bin/
else
	@echo "[in] $< -> /usr/bin/$<"
	@cp $< /usr/bin
endif

clean: 
	@rm -f ircdk
