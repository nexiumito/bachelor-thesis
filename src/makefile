CC = gcc

# -O3 : optimisation pour la vitesse d'exécution
# -g : permet de garder les symboles de débogage 
CFLAGS = -Wall -Wextra -O3 -g

SRCS = main.c procedure1.c trie.c tree_generation.c
OBJS = $(SRCS:.c=.o)

TARGET = sat_solver

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

# clean et rebuild ne sont pas des fichiers
.PHONY: all clean rebuild