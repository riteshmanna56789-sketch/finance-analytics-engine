# Finance Analytics Engine

## Overview

Finance Analytics Engine is a single-user, offline C command-line expense
tracker. The V1 foundation focuses on reliable expense recording, local
storage, organization, and querying; V2 extends its analytics while retaining
the same local CLI architecture.

## Current Features

- Add expenses with automatically assigned IDs and local timestamps.
- Store money as integer paise using `int64_t`, without floating-point money
  storage.
- Create user-defined categories with stable IDs.
- Deactivate categories with soft deletion. Historical expenses retain their
  category references and continue to display the inactive category name.
- Save and load data from a local file.
- Edit expense amount, category, and note while retaining the expense ID and
  original timestamp.
- Delete expenses with confirmation while preserving the order of remaining
  expenses.
- Search expenses by category, exact amount, and note; filter by time, amount
  range, date range, or combinations of those filters.
- Sort expense views by date, amount, or category without changing the
  canonical expense-list order.
- View a spending summary with total, transaction count, exact integer
  average, minimum, maximum, today, current-month, and category totals.
- Analyze spending by day, month, year, or inclusive custom date range,
  including transaction count, total, average, minimum, and maximum.
- View category spending breakdowns for all time or a selected day, month,
  year, or inclusive date range, with per-category totals, transaction counts,
  and percentages of the selected-period total. Historical spending remains
  included for inactive categories.
- Compare spending between two months, two years, or two inclusive date
  ranges, including total/count/average changes and category-level changes.
  Percentage changes use controlled two-decimal precision and show as
  unavailable when the baseline is zero.
- Analyze consecutive monthly or yearly spending trends, including each
  period's total, transaction count, and average, plus sequence-wide total,
  average, highest/lowest period, transition counts, and first-to-last change.
- View objective financial insights for selected periods, including tied
  highest/lowest categories and periods, highest category share, average
  spending per trend period, and the largest absolute category change,
  increase, or decrease between compared periods. Empty results and
  zero-baseline shares are reported explicitly; the application does not
  generate subjective financial advice.
- Generate read-only overall, monthly, yearly, and custom inclusive date-range
  reports combining existing summaries, category breakdowns, percentages,
  and objective category insights.
- Validate user input and stored records, including amounts, IDs, categories,
  dates, and string boundaries.
- Run assertion-based hardening tests for data operations, querying, sorting,
  analytics, and persistence validation.

## Architecture

The project is organized into three directories:

- `include/` contains public module interfaces and data types.
- `src/` contains the application and module implementations.
- `tests/` contains the hardening test program.

Major module responsibilities:

- **expense** (`expense.h`, `expense.c`): expense list operations, ID
  allocation, amount parsing, validation, editing, and deletion.
- **category** (`category.h`, `category.c`): category list operations,
  category ID allocation, active-category lookup, and deactivation.
- **storage** (`storage.h`, `storage.c`): local save/load, record validation,
  and replacement of the saved file.
- **analytics** (`analytics.h`, `analytics.c`): checked total, today,
  current-month, and per-category spending calculations; transaction count,
  exact average representation, minimum, and maximum. Invalid category
  references and total overflow are reported explicitly. Day, month, year,
  and inclusive date-range summaries use linear scans and do not depend on
  category references. Category breakdowns group matching expenses, retain
  first-seen category order, and compute percentages rounded to two decimal
  places. Comparative analysis composes period summaries and category
  breakdowns, joins categories by stable ID, and reports signed absolute and
  percentage changes without subjective assessments. Trend analysis reuses
  period summaries, generates bounded consecutive periods, and reports
  sequence-level metrics without interpreting the results as advice.
  Derived insight functions operate on existing breakdown, trend, and
  comparison results, preserve all ties in source order, and own only their
  returned winner arrays until their corresponding destroy functions run.
  Report generation composes the existing summary, category breakdown, and
  category insight APIs without changing expense or category data; callers
  release the report's nested allocations with `analytics_report_destroy()`.
- **analytics_ui** (`analytics_ui.h`, `analytics_ui.c`): presentation and
  input for overall, time-based, category-based, trend, comparison,
  objective financial insights, and reports, keeping analytics UI out of
  `main`.
- **query** (`query.h`, `query.c`): category, amount, time, note, date/amount
  range, and combined expense queries.
- **sorting** (`sorting.h`, `sorting.c`): stable sorting of a temporary
  pointer view of expenses; it does not reorder the original list.
- **expense_management** (`expense_management.h`,
  `expense_management.c`): command-line interaction for editing and deleting
  expenses.
- **main** (`main.c`): application entry point, menus, expense/category
  interaction, and coordination between modules.

## Data Model

- **Expense** stores an ID, timestamp, amount in paise, category ID, and note.
- **Category** stores an ID, name, and `is_active` state.
- **Timestamp** stores year, month, day, hour, minute, and second.
- **ExpenseList** and **CategoryList** hold dynamically allocated arrays with
  size and capacity metadata. The expense list also tracks the next expense
  ID so deleted IDs are not automatically reused.

Money is stored as `int64_t amount_paise`, not as `float` or `double`. This
avoids floating-point rounding in stored monetary values.

The analytics average is kept exact as an integer quotient and remainder:
`average_paise` plus `average_remainder_paise / transaction_count` paise.
Minimum and maximum values are only meaningful when the transaction count is
nonzero.

Each expense stores a `category_id` rather than a copy of the category name.
This preserves the relationship to one stable category record, including when
that category is later deactivated. Deactivation is a soft delete:
`is_active` becomes `0`, but the category record remains available for
historical expense display, queries, analytics, and persistence. Inactive
categories are not available for new expenses.

## Persistence

Data is stored locally in `data/finance.dat` relative to the application’s
working directory. The text-based storage format contains category records,
expense records, and the next expense ID. The current format is version 2;
the loader also accepts version 1 files. Stored counts and records are
validated before loaded data replaces the in-memory lists.

## Build and Run

From the project root, compile with GCC:

```powershell
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o finance.exe
```

Run the application from the project root:

```powershell
.\finance.exe
```

## Testing

The tests use standard C assertions and temporary test data under `tests/`.
They should be run from the project root and do not use `data/finance.dat`.
The single `finance_tests.exe` executable runs independent suites for
categories, expenses, queries, sorting, analytics, storage, and cross-module
workflows. Assertions must remain enabled when building the tests.

Build and run the test program with GCC:

```powershell
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude tests/test_runner.c tests/test_category.c tests/test_expense.c tests/test_query.c tests/test_sorting.c tests/test_analytics.c tests/test_storage.c tests/test_workflows.c src/expense.c src/category.c src/storage.c src/analytics.c src/query.c src/sorting.c src/expense_management.c -o finance_tests.exe
.\finance_tests.exe
```

The repository’s lowercase `makefile` `test` target builds and runs the same
suite set as the manual command. `make`, `make test`, and `make clean` require
GNU Make to be installed.

## Benchmarks

The standalone `finance_benchmarks.exe` measures the current category lookup,
combined query, overall summary, category breakdown, 12-month trend, and
amount-sorting implementations. It generates deterministic in-memory data at
100, 1,000, 10,000, and 100,000 expenses; category lookup uses the same sizes
as category-list lengths. It compares a benchmark-only stable insertion-sort
baseline with the production stable merge sort, both sorting
amount-ascending pointer views. Production sorting uses merge sort with
O(n log n) expense comparisons and O(n) additional space; category sorting
retains the existing category-ID lookup in each comparison.
Both are timed at 100, 1,000, and 10,000 records; only merge sort is timed at
100,000 because insertion sort's quadratic scaling makes that case
impractical. Before timing, each implementation is checked for ordering,
stability, and preservation of the input list. Their output pointer order is
also compared wherever both are run.

Build and run with GCC from the project root:

```powershell
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude benchmarks/benchmark_runner.c benchmarks/benchmark_data.c benchmarks/benchmark_auxiliary.c src/expense.c src/category.c src/storage.c src/analytics.c src/query.c src/sorting.c -o finance_benchmarks.exe
.\finance_benchmarks.exe
```

The benchmark writes CSV rows to standard output with operation, data size,
repetition count, total CPU time, mean CPU nanoseconds per operation, and a
checksum. Timing uses standard C `clock()` CPU time. Synthetic-data creation
is outside the timed interval; API-owned output allocation and matching
destruction are included in measured operations. Sorting correctness checks
are outside the timed interval. Both algorithms use identical repetition
counts per dataset; repetition counts vary by dataset size to keep runtimes
bounded. Run from the project root to reproduce the same data and output
shape; absolute timings vary by machine, compiler, and system load. The
insertion-sort row preserves a comparable baseline after the production
algorithm change. The lowercase `makefile` also provides `make benchmark`.

The benchmark also measures versioned storage save/load using the same
deterministic expense generator and 64 categories, and active-category
enumeration using `category_active_count()` followed by
`category_find_active_by_index()` for each active entry. These auxiliary
measurements use wall-clock time from C17 `timespec_get(TIME_UTC)`, exclude
data generation, and report repetition counts, mean elapsed time, estimated
input array bytes, and serialized file bytes for storage. Storage runs at
100, 1,000, and 10,000 expenses; 100,000 is explicitly skipped because the
current storage validation and loading duplicate checks require quadratic
expense scans. Enumeration runs at 100, 1,000, 10,000, and 100,000 active
categories. Storage uses a process-specific benchmark file in the project
root and removes it and its temporary save file afterward; it does not access
`data/finance.dat`.

### V3.2 Performance Conclusion

The baseline uses deterministic synthetic datasets of 100, 1,000, 10,000, and
100,000 expenses. Data generation and sorting correctness checks are outside
the timed interval; operations are timed with standard C `clock()` CPU time,
with operation-owned output allocation and cleanup included where applicable.
At 100,000 records, the measured results were:

| Operation | CPU time |
| --- | ---: |
| Category lookup | 130 µs |
| Combined query | 800 µs |
| Overall summary | 8.24 ms |
| Category breakdown | 7.2 ms |
| 12-month trend | 29.4 ms |
| Production merge sort | 20.0 ms |

Sorting demonstrated a meaningful scaling bottleneck and was changed from
stable insertion sort to stable merge sort. The measured results for the
other operations do not justify additional data structures, indexing,
caching, or algorithmic complexity at the current project scale, so their
implementations are retained. Future optimization should be driven by
measured bottlenecks rather than theoretical complexity alone.

## Project Structure

```text
Finance Analytics Engine/
├── data/
│   └── finance.dat          # Local application data (created/updated on save)
├── include/
│   ├── analytics.h
│   ├── analytics_ui.h
│   ├── category.h
│   ├── expense.h
│   ├── expense_management.h
│   ├── query.h
│   ├── sorting.h
│   └── storage.h
├── src/
│   ├── analytics.c
│   ├── analytics_ui.c
│   ├── category.c
│   ├── expense.c
│   ├── expense_management.c
│   ├── main.c
│   ├── query.c
│   ├── sorting.c
│   └── storage.c
├── tests/
│   ├── test_analytics.c
│   ├── test_category.c
│   ├── test_expense.c
│   ├── test_query.c
│   ├── test_runner.c
│   ├── test_sorting.c
│   ├── test_storage.c
│   ├── test_support.h
│   └── test_workflows.c
├── benchmarks/
│   ├── benchmark_data.c
│   ├── benchmark_data.h
│   └── benchmark_runner.c
├── makefile
└── README.md
```

## V1 Scope

V1 is a local command-line program. It is not a web application, database
server, multi-user system, or cloud-synchronization service.

## Future Direction

Future versions may introduce deeper financial analytics. Such capabilities
are not part of the current V1 implementation.
