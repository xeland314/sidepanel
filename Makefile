CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags x11 cairo pangocairo)
LIBS = $(shell pkg-config --libs x11 cairo pangocairo) -lpthread

TARGET = sidepanel
SRCS = main.c
HEADERS = banner.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
