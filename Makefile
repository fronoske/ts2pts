CC     = g++
CFLAGS = -Wall -O2 -g -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LDLIBS =

TARGET = ts2pts
OBJS   = app.o crc.o ts2pts.o

all: $(TARGET)

app.o: app.cpp
crc.o: crc.cpp
ts2pts.o: ts2pts.cpp

$(TARGET): $(OBJS)
	$(CC) $(LDLIBS) -o $(TARGET) $(OBJS)

clean:
	rm -f *.o *~
	rm -f $(TARGET)
