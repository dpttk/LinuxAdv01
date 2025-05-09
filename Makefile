CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lhpdf

TARGET = bldd
SRC = bldd.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install 