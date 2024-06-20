TARGET = aesdsocket
HEADERS =
OBJECTS = aesdsocket.o
CFLAGS += -Wall
LDFLAGS += -pthread

.PHONY: default clean

default: $(TARGET)

%.o: %.c $(HEADERS)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CROSS_COMPILE)gcc $(OBJECTS) $(LDFLAGS) -o $(TARGET)

clean:
	-rm -f $(TARGET) $(OBJECTS)
