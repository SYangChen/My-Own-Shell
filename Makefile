CC = gcc
TARGET1 = shell

FLAG = -std=c11

all: $(TARGET1).c
	$(CC) $(FLAG) -c $(TARGET1).c
	$(CC) $(FLAG) -o $(TARGET1) $(TARGET1).o 
	
clean:
	rm -f $(TARGET1)
	rm -f $(TARGET1).o
	
