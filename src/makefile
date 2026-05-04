CC = gcc
CFLAGS = -Wall -Wextra -O3 -g

SRCS = main.c \
       utils/bitset.c \
       utils/ps_set.c \
       utils/trie.c \
       utils/dp_table.c \
       utils/reverse_maps.c \
       utils/dnnf.c \
       utils/dnnf_transform.c \
       utils/query/count.c \
       utils/query/consistency.c \
       utils/query/validity.c \
       utils/query/find_model.c \
       utils/query/entails.c \
       utils/query/is_implicant.c \
       utils/query/enumerate.c \
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

# Compilation avec AddressSanitizer pour la verification memoire (Phase 1 KC).
# Usage : make asan (puis ./sat_solver benchmark 2>&1 | grep -i sanitizer).
asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="-Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer" all

.PHONY: all clean rebuild asan
