CC = gcc
CFLAGS = -Wall -Wextra -O2

# Set to 0 to disable PDF support if libhpdf is not available
PDF_SUPPORT ?= 1

ifeq ($(PDF_SUPPORT),1)
    CFLAGS += -DPDF_SUPPORT=1
    LDFLAGS = -lhpdf
else
    CFLAGS += -DPDF_SUPPORT=0
    LDFLAGS =
endif

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