
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L

NAME = ocsh
SRC = src/main.c

all: $(NAME)

$(NAME): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(NAME)

clean:
	rm -f $(NAME)

re: clean all

.PHONY: all clean re
