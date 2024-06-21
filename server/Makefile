TARGET = aesdsocket
HEADERS =
OBJECTS = aesdsocket.o
CFLAGS += -Wall
LDFLAGS += -pthread

.PHONY: default clean

default: $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

clean:
	-rm -f $(TARGET) $(OBJECTS)
