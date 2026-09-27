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

static void initialize_summary(
    AnalyticsSummary *summary,
    size_t category_count
)
{
    summary->total_paise = 0;
    summary->today_paise = 0;
    summary->current_month_paise = 0;
    summary->transaction_count = 0;
    summary->average_paise = 0;
    summary->average_remainder_paise = 0;
    summary->minimum_paise = 0;
    summary->maximum_paise = 0;
    summary->category_totals_paise = NULL;
    summary->category_count = category_count;
}

AnalyticsResult analytics_calculate_summary(
    const ExpenseList *expenses,
    const CategoryList *categories,
    AnalyticsSummary *summary
)
{
    time_t current_timestamp;
    struct tm *current_time = NULL;
    AnalyticsResult result = ANALYTICS_SUCCESS;

    if (expenses == NULL || categories == NULL || summary == NULL
        || expenses->size > expenses->capacity
        || (expenses->size > 0 && expenses->items == NULL)
        || categories->size > categories->capacity
        || (categories->size > 0 && categories->items == NULL)
        || categories->size
            > SIZE_MAX / sizeof(*summary->category_totals_paise)) {
        return ANALYTICS_INVALID_INPUT;
    }

    initialize_summary(summary, categories->size);

    if (categories->size > 0) {
        summary->category_totals_paise = calloc(
            categories->size,
            sizeof(*summary->category_totals_paise)
        );
        if (summary->category_totals_paise == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }

    if (expenses->size == 0) {
        return ANALYTICS_SUCCESS;
    }

    current_timestamp = time(NULL);
    current_time = localtime(&current_timestamp);
    if (current_time == NULL) {
        result = ANALYTICS_INVALID_INPUT;
        goto fail;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        const Expense *expense = &expenses->items[index];
        const Category *category = category_find_by_id(
            categories,
            expense->category_id
        );
        size_t category_index;

        if (expense->amount_paise <= 0) {
            result = ANALYTICS_INVALID_INPUT;
            goto fail;
        }
        if (category == NULL) {
            result = ANALYTICS_INVALID_CATEGORY;
            goto fail;
        }

        category_index = (size_t)(category - categories->items);
        if (!add_amount(&summary->total_paise, expense->amount_paise)
            || !add_amount(
                &summary->category_totals_paise[category_index],
                expense->amount_paise
            )
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
            result = ANALYTICS_OVERFLOW;
            goto fail;
        }

        if (summary->transaction_count == SIZE_MAX) {
            result = ANALYTICS_OVERFLOW;
            goto fail;
        }

        if (summary->transaction_count == 0
            || expense->amount_paise < summary->minimum_paise) {
            summary->minimum_paise = expense->amount_paise;
        }
        if (summary->transaction_count == 0
            || expense->amount_paise > summary->maximum_paise) {
            summary->maximum_paise = expense->amount_paise;
        }
        summary->transaction_count++;
    }

    if (summary->transaction_count <= (size_t)INT64_MAX) {
        int64_t divisor = (int64_t)summary->transaction_count;
        summary->average_paise = summary->total_paise / divisor;
        summary->average_remainder_paise = summary->total_paise % divisor;
    } else {
        summary->average_paise = 0;
        summary->average_remainder_paise = summary->total_paise;
    }

    return ANALYTICS_SUCCESS;

fail:
    analytics_summary_destroy(summary);
    return result;
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
    summary->transaction_count = 0;
    summary->average_paise = 0;
    summary->average_remainder_paise = 0;
    summary->minimum_paise = 0;
    summary->maximum_paise = 0;
}
