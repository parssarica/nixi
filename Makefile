CC = gcc
CFLAGS = -O0 -Wall -Wextra -pedantic -Wstrict-prototypes
LDFLAGS = -lsqlite3 -lcurl -larchive -lgpgme
TARGET = nixi
SOURCES = nixi.c gpg.c untar.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET)
install:
	cp nixi /usr/bin

# Phony targets
.PHONY: all clean
