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

typedef enum {
    ANALYTICS_PERIOD_ALL_TIME = 0,
    ANALYTICS_PERIOD_DAY,
    ANALYTICS_PERIOD_MONTH,
    ANALYTICS_PERIOD_YEAR,
    ANALYTICS_PERIOD_DATE_RANGE
} AnalyticsPeriodType;

typedef struct {
    AnalyticsPeriodType type;
    int year;
    int month;
    Timestamp date;
    Timestamp start_date;
    Timestamp end_date;
} AnalyticsPeriod;

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

typedef struct {
    int category_id;
    char category_name[sizeof(((Category *)0)->name)];
    int64_t total_paise;
    size_t transaction_count;
    unsigned int percentage_basis_points;
} AnalyticsCategoryTotal;

typedef struct {
    AnalyticsCategoryTotal *items;
    size_t size;
    size_t capacity;
    int64_t total_paise;
    size_t transaction_count;
} AnalyticsCategoryBreakdown;

typedef enum {
    ANALYTICS_PERCENTAGE_DEFINED = 0,
    ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE
} AnalyticsPercentageState;

typedef struct {
    AnalyticsPercentageState state;
    int is_negative;
    uint64_t basis_points;
} AnalyticsPercentageChange;

typedef struct {
    int category_id;
    char category_name[sizeof(((Category *)0)->name)];
    int64_t period_a_total_paise;
    int64_t period_b_total_paise;
    int64_t absolute_change_paise;
    size_t period_a_transaction_count;
    size_t period_b_transaction_count;
    unsigned int period_a_percentage_basis_points;
    unsigned int period_b_percentage_basis_points;
    AnalyticsPercentageChange percentage_change;
} AnalyticsCategoryComparison;

typedef struct {
    AnalyticsSummary period_a;
    AnalyticsSummary period_b;
    int64_t absolute_change_paise;
    AnalyticsPercentageChange percentage_change;
    AnalyticsCategoryComparison *categories;
    size_t category_count;
    size_t category_capacity;
} AnalyticsComparison;

typedef enum {
    ANALYTICS_TREND_MONTHLY = 1,
    ANALYTICS_TREND_YEARLY
} AnalyticsTrendType;

typedef struct {
    int year;
    int month;
    int64_t total_paise;
    size_t transaction_count;
    int64_t average_paise;
    int64_t average_remainder_paise;
} AnalyticsTrendPeriod;

typedef struct {
    AnalyticsTrendPeriod *periods;
    size_t period_count;
    int64_t total_paise;
    int64_t average_period_paise;
    int64_t average_period_remainder_paise;
    size_t highest_period_index;
    size_t lowest_period_index;
    size_t increasing_transitions;
    size_t decreasing_transitions;
    size_t unchanged_transitions;
    int64_t first_to_last_change_paise;
    AnalyticsPercentageChange first_to_last_percentage_change;
} AnalyticsTrend;

#define ANALYTICS_MAX_MONTHLY_TREND_PERIODS 1200
#define ANALYTICS_MAX_YEARLY_TREND_PERIODS 200

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

/*
 * Time-filtered summaries ignore category references and use only expense
 * amounts and dates. Date ranges are inclusive. These operations take O(n)
 * time and O(1) additional space.
 */
AnalyticsResult analytics_calculate_daily_summary(
    const ExpenseList *expenses,
    const Timestamp *date,
    AnalyticsSummary *summary
);

AnalyticsResult analytics_calculate_monthly_summary(
    const ExpenseList *expenses,
    int year,
    int month,
    AnalyticsSummary *summary
);

AnalyticsResult analytics_calculate_yearly_summary(
    const ExpenseList *expenses,
    int year,
    AnalyticsSummary *summary
);

AnalyticsResult analytics_calculate_date_range_summary(
    const ExpenseList *expenses,
    const Timestamp *start_date,
    const Timestamp *end_date,
    AnalyticsSummary *summary
);

/*
 * A NULL period means all time; otherwise use a validated AnalyticsPeriod.
 * The result owns items until analytics_category_breakdown_destroy() is
 * called. Results preserve first-seen category order. Percentages are rounded
 * half-up to the nearest 0.01 percent and stored as basis points.
 * Grouping takes O(n*c) time; category-name resolution adds O(c*k), where k
 * is the number of categories. Result storage is O(c).
 */
AnalyticsResult analytics_calculate_category_breakdown(
    const ExpenseList *expenses,
    const CategoryList *categories,
    const AnalyticsPeriod *period,
    AnalyticsCategoryBreakdown *breakdown
);

void analytics_category_breakdown_destroy(
    AnalyticsCategoryBreakdown *breakdown
);

/*
 * Computes period summaries using existing analytics primitives, then joins
 * category results by stable category ID. The result owns its embedded
 * summaries and category array until analytics_comparison_destroy().
 * Percentage changes use rounded basis points; if that derived value cannot
 * fit in uint64_t basis points, ANALYTICS_OVERFLOW is returned.
 */
AnalyticsResult analytics_compare_periods(
    const ExpenseList *expenses,
    const CategoryList *categories,
    const AnalyticsPeriod *period_a,
    const AnalyticsPeriod *period_b,
    AnalyticsComparison *comparison
);

void analytics_comparison_destroy(AnalyticsComparison *comparison);

/*
 * Monthly trends are limited to 1200 consecutive months, yearly trends to
 * 200 consecutive years, and generated years must not exceed 9999. The
 * returned period array is owned by the trend and released by
 * analytics_trend_destroy(). Calculation is O(period_count * expense_count).
 */
AnalyticsResult analytics_calculate_trend(
    const ExpenseList *expenses,
    AnalyticsTrendType type,
    int start_year,
    int start_month,
    size_t period_count,
    AnalyticsTrend *trend
);

void analytics_trend_destroy(AnalyticsTrend *trend);

void analytics_summary_destroy(AnalyticsSummary *summary);

#endif
