CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = resource_monitor

all: $(TARGET)

$(TARGET): resource_monitor.c
	$(CC) $(CFLAGS) resource_monitor.c -o $(TARGET)

clean:
	rm -f $(TARGET)

install:
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
