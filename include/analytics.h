#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <stdint.h>

#include "category.h"
#include "expense.h"

typedef enum {
    ANALYTICS_SUCCESS = 0,
    ANALYTICS_INVALID_INPUT,
    ANALYTICS_MEMORY_ERROR,
    ANALYTICS_OVERFLOW
} AnalyticsResult;

typedef struct {
    int64_t total_paise;
    int64_t today_paise;
    int64_t current_month_paise;
    int64_t *category_totals_paise;
    size_t category_count;
} AnalyticsSummary;

AnalyticsResult analytics_calculate_summary(
    const ExpenseList *expenses,
    const CategoryList *categories,
    AnalyticsSummary *summary
);

void analytics_summary_destroy(AnalyticsSummary *summary);

#endif
