CC = gcc
BIN = tinytile

SRC = TinyTile.c \
      src/backend.c \
      src/client.c \
      src/layer.c \
      src/render.c \
      src/tags.c \
      src/actions.c \
      src/bar.c \
      src/text.c \
      src/spawn.c \
      src/layout/dwindle.c \
      deps/toml.c

OBJ = $(SRC:.c=.o)
INCLUDES = -I. -Ideps -Isrc -Isrc/layout

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

WLR_PKG ?= wlroots-0.20
PKGS = $(WLR_PKG) wayland-server xkbcommon
PKG_CFLAGS = $(shell pkg-config --cflags $(PKGS))
PKG_LIBS = $(shell pkg-config --libs $(PKGS))

CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L $(INCLUDES) $(PKG_CFLAGS)
LDLIBS = $(PKG_LIBS) -lm

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c TinyTile.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BIN) $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

.PHONY: all clean install uninstall