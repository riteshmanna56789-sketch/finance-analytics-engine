# Finance Analytics Engine

## Overview

Finance Analytics Engine V1 is a single-user, offline C command-line expense
tracker. The long-term project may evolve into a broader finance analytics
engine, but V1 focuses on reliable expense recording, local storage,
organization, querying, and basic spending analysis.

## Current V1 Features

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
- View a spending summary with total, today, current-month, and category
  totals.
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
- **analytics** (`analytics.h`, `analytics.c`): total, today, current-month,
  and per-category spending calculations.
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

The hardening tests use C assertions and temporary test data under `tests/`.
They should be run from the project root and do not use
`data/finance.dat`.

Compile the test program:

```powershell
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude tests/hardening.c src/expense.c src/category.c src/storage.c src/analytics.c src/query.c src/sorting.c src/expense_management.c -o hardening.exe
```

Run it:

```powershell
.\hardening.exe
```

The repository’s lowercase `makefile` also defines `make`, `make test`, and
`make clean` targets. These targets require GNU Make to be installed.

## Project Structure

```text
Finance Analytics Engine/
├── data/
│   └── finance.dat          # Local application data (created/updated on save)
├── include/
│   ├── analytics.h
│   ├── category.h
│   ├── expense.h
│   ├── expense_management.h
│   ├── query.h
│   ├── sorting.h
│   └── storage.h
├── src/
│   ├── analytics.c
│   ├── category.c
│   ├── expense.c
│   ├── expense_management.c
│   ├── main.c
│   ├── query.c
│   ├── sorting.c
│   └── storage.c
├── tests/
│   └── hardening.c
├── makefile
└── README.md
```

## V1 Scope

V1 is a local command-line program. It is not a web application, database
server, multi-user system, or cloud-synchronization service.

## Future Direction

Future versions may introduce deeper financial analytics. Such capabilities
are not part of the current V1 implementation.
