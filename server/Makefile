CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = aesdsocket
OBJFILES = aesdsocket.o

ifdef CROSS_COMPILE
    CC = $(CROSS_COMPILE)gcc
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJFILES)