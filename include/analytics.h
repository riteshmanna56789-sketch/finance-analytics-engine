#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <stddef.h>
#include <stdint.h>

#include "category.h"
#include "expense.h"

typedef enum {
    ANALYTICS_SUCCESS = 0,
    ANALYTICS_INVALID_INPUT,
    ANALYTICS_MEMORY_ERROR,
    ANALYTICS_OVERFLOW,
    ANALYTICS_INVALID_CATEGORY
} AnalyticsResult;

typedef struct {
    int64_t total_paise;
    int64_t today_paise;
    int64_t current_month_paise;
    size_t transaction_count;
    int64_t average_paise;
    int64_t average_remainder_paise;
    int64_t minimum_paise;
    int64_t maximum_paise;
    int64_t *category_totals_paise;
    size_t category_count;
} AnalyticsSummary;

/*
 * For non-empty datasets, the average is represented exactly as
 * average_paise + average_remainder_paise / transaction_count paise.
 * minimum_paise and maximum_paise are meaningful only when the transaction
 * count is nonzero. Category totals include inactive historical categories.
 * Calculation takes O(expenses * categories) time and O(categories) space.
 */
AnalyticsResult analytics_calculate_summary(
    const ExpenseList *expenses,
    const CategoryList *categories,
    AnalyticsSummary *summary
);

void analytics_summary_destroy(AnalyticsSummary *summary);

#endif
