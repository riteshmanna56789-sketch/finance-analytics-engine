#ifndef FINANCE_STORAGE_H
#define FINANCE_STORAGE_H

#include "category.h"
#include "expense.h"

typedef enum {
    STORAGE_SUCCESS = 0,
    STORAGE_FILE_ERROR,
    STORAGE_NOT_FOUND,
    STORAGE_INVALID_DATA,
    STORAGE_MEMORY_ERROR
} StorageResult;

StorageResult storage_save(
    const char *filename,
    const CategoryList *categories,
    const ExpenseList *expenses
);

StorageResult storage_load(
    const char *filename,
    CategoryList *categories,
    ExpenseList *expenses
);

#endif /* FINANCE_STORAGE_H */