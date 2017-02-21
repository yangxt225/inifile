srcdir = .

CC = gcc
CFLAGS=-Wall -O2
#指定目录变量
#HSOURCES=-I$(srcdir)/../
LDFLAGS=-L$(srcdir)/../

OBJECTS = cfgfileopt.o
TARGET=main

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LDFLAGS) -linifile -lzlog

.c.o:
	$(CC) -c $(CFLAGS) $< #$(HSOURCES)

clean:
	rm -f $(OBJECTS) 
	rm -f $(TARGET) 
	rm -f *.ini
	rm -f *.log 

