CC = gcc

CFLAGS = -Wall -Wextra -g # -O3, à décommenter pour les tests de performance finaux

TARGET = sat_solver

OBJS = main.o procedure1.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c formula.h procedure1.h
	$(CC) $(CFLAGS) -c main.c

procedure1.o: procedure1.c formula.h procedure1.h
	$(CC) $(CFLAGS) -c procedure1.c

clean:
	rm -f $(OBJS) $(TARGET)