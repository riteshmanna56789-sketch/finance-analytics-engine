CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic

APP = finance.exe
TEST_APP = hardening.exe

SOURCES = $(wildcard src/*.c)
TEST_SOURCES = \
	src/expense.c \
	src/category.c \
	src/storage.c \
	src/analytics.c \
	src/query.c \
	src/sorting.c \
	src/expense_management.c

.PHONY: all test clean

all: $(APP)

$(APP): $(SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCES) -o $@

$(TEST_APP): tests/hardening.c $(TEST_SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@

test: $(TEST_APP)
	./$(TEST_APP)

clean:
	$(RM) $(APP) $(TEST_APP) finance_asan.exe finance_analyzer.exe
	$(RM) *.o src/*.o tests/*.o
