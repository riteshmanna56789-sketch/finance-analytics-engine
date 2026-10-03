CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic

APP = finance.exe
TEST_APP = finance_tests.exe

SOURCES = $(wildcard src/*.c)
TEST_SOURCES = \
	tests/test_runner.c \
	tests/test_category.c \
	tests/test_expense.c \
	tests/test_query.c \
	tests/test_sorting.c \
	tests/test_analytics.c \
	tests/test_storage.c \
	tests/test_workflows.c
TEST_HEADERS = tests/test_support.h
TEST_MODULES = \
	src/expense.c \
	src/category.c \
	src/storage.c \
	src/analytics.c \
	src/query.c \
	src/sorting.c \
	src/expense_management.c
BENCHMARK_APP = finance_benchmarks.exe
BENCHMARK_SOURCES = \
	benchmarks/benchmark_runner.c \
	benchmarks/benchmark_data.c
BENCHMARK_MODULES = \
	src/expense.c \
	src/category.c \
	src/analytics.c \
	src/query.c \
	src/sorting.c

.PHONY: all test benchmark clean

all: $(APP)

$(APP): $(SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCES) -o $@

$(TEST_APP): $(TEST_SOURCES) $(TEST_HEADERS) $(TEST_MODULES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TEST_SOURCES) $(TEST_MODULES) -o $@

test: $(TEST_APP)
	./$(TEST_APP)

$(BENCHMARK_APP): $(BENCHMARK_SOURCES) benchmarks/benchmark_data.h $(BENCHMARK_MODULES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(BENCHMARK_SOURCES) $(BENCHMARK_MODULES) -o $@

benchmark: $(BENCHMARK_APP)
	./$(BENCHMARK_APP)

clean:
	$(RM) $(APP) $(TEST_APP) $(BENCHMARK_APP) hardening.exe finance_asan.exe finance_analyzer.exe
	$(RM) *.o src/*.o tests/*.o
