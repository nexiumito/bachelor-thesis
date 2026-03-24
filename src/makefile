CC = gcc
CFLAGS = -Wall -Wextra -O3 -g

SRCS = main.c \
       utils/bitset.c \
       utils/ps_set.c \
       utils/trie.c \
       utils/dp_table.c \
       utils/reverse_maps.c \
       core/parser.c \
       core/decomposition_tree.c \
       algo/procedure1.c \
       algo/procedure2.c \
       algo/procedure3.c

OBJS = $(SRCS:.c=.o)

EXEC = sat_solver

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)

rebuild: clean all

.PHONY: all clean rebuild