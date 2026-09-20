#include "analytics.h"

#include <limits.h>
#include <stdlib.h>
#include <time.h>

static int add_amount(int64_t *total, int64_t amount)
{
    if (amount < 0 || *total > INT64_MAX - amount) {
        return 0;
    }

    *total += amount;
    return 1;
}

static int is_today(const Timestamp *timestamp, const struct tm *current_time)
{
    return timestamp->year == current_time->tm_year + 1900
        && timestamp->month == current_time->tm_mon + 1
        && timestamp->day == current_time->tm_mday;
}

static int is_current_month(
    const Timestamp *timestamp,
    const struct tm *current_time
)
{
    return timestamp->year == current_time->tm_year + 1900
        && timestamp->month == current_time->tm_mon + 1;
}

AnalyticsResult analytics_calculate_summary(
    const ExpenseList *expenses,
    const CategoryList *categories,
    AnalyticsSummary *summary
)
{
    time_t current_timestamp;
    struct tm *current_time;

    if (expenses == NULL || categories == NULL || summary == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }

    summary->total_paise = 0;
    summary->today_paise = 0;
    summary->current_month_paise = 0;
    summary->category_totals_paise = NULL;
    summary->category_count = categories->size;

    if (categories->size > 0) {
        summary->category_totals_paise = calloc(
            categories->size,
            sizeof(*summary->category_totals_paise)
        );
        if (summary->category_totals_paise == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }

    current_timestamp = time(NULL);
    current_time = localtime(&current_timestamp);
    if (current_time == NULL) {
        analytics_summary_destroy(summary);
        return ANALYTICS_INVALID_INPUT;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        const Expense *expense = &expenses->items[index];
        const Category *category = category_find_by_id(
            categories,
            expense->category_id
        );

        if (!add_amount(&summary->total_paise, expense->amount_paise)
            || (is_today(&expense->timestamp, current_time)
                && !add_amount(
                    &summary->today_paise,
                    expense->amount_paise
                ))
            || (is_current_month(&expense->timestamp, current_time)
                && !add_amount(
                    &summary->current_month_paise,
                    expense->amount_paise
                ))) {
            analytics_summary_destroy(summary);
            return ANALYTICS_OVERFLOW;
        }

        if (category != NULL) {
            size_t category_index = (size_t)(category - categories->items);

            if (!add_amount(
                    &summary->category_totals_paise[category_index],
                    expense->amount_paise
                )) {
                analytics_summary_destroy(summary);
                return ANALYTICS_OVERFLOW;
            }
        }
    }

    return ANALYTICS_SUCCESS;
}

void analytics_summary_destroy(AnalyticsSummary *summary)
{
    if (summary == NULL) {
        return;
    }

    free(summary->category_totals_paise);
    summary->category_totals_paise = NULL;
    summary->category_count = 0;
    summary->total_paise = 0;
    summary->today_paise = 0;
    summary->current_month_paise = 0;
}
