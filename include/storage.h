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

/*
 * categories and expenses must be initialized, valid lists. On success, their
 * existing contents are destroyed and replaced by the loaded lists. On
 * failure, their existing contents remain unchanged. The caller owns and
 * remains responsible for destroying the resulting lists.
 */
StorageResult storage_load(
    const char *filename,
    CategoryList *categories,
    ExpenseList *expenses
);

#endif /* FINANCE_STORAGE_H */