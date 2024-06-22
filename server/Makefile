CC ?= $(CROSS_COMPILE)gcc
TARGET ?= aesdsocket
HEADERS =
OBJECTS = aesdsocket.o
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -pthread -lrt

.PHONY: default clean

default: $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

clean:
	-rm -f $(TARGET) $(OBJECTS)
