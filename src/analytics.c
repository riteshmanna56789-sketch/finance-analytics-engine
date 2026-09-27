#include "analytics.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int add_amount(int64_t *total, int64_t amount)
{
    if (amount < 0 || *total > INT64_MAX - amount) {
        return 0;
    }

    *total += amount;
    return 1;
}

static int summary_add_expense(
    AnalyticsSummary *summary,
    int64_t amount_paise
)
{
    if (amount_paise <= 0
        || !add_amount(&summary->total_paise, amount_paise)
        || summary->transaction_count == SIZE_MAX) {
        return 0;
    }

    if (summary->transaction_count == 0
        || amount_paise < summary->minimum_paise) {
        summary->minimum_paise = amount_paise;
    }
    if (summary->transaction_count == 0
        || amount_paise > summary->maximum_paise) {
        summary->maximum_paise = amount_paise;
    }
    summary->transaction_count++;
    return 1;
}

static void summary_set_average(AnalyticsSummary *summary)
{
    if (summary->transaction_count <= (size_t)INT64_MAX) {
        int64_t divisor = (int64_t)summary->transaction_count;
        if (divisor != 0) {
            summary->average_paise = summary->total_paise / divisor;
            summary->average_remainder_paise = summary->total_paise % divisor;
        }
    } else {
        summary->average_paise = 0;
        summary->average_remainder_paise = summary->total_paise;
    }
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
        if (!summary_add_expense(summary, expense->amount_paise)
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
    }

    summary_set_average(summary);

    return ANALYTICS_SUCCESS;

fail:
    analytics_summary_destroy(summary);
    return result;
}

static int compare_date(const Timestamp *left, const Timestamp *right)
{
    if (left->year != right->year) {
        return left->year < right->year ? -1 : 1;
    }
    if (left->month != right->month) {
        return left->month < right->month ? -1 : 1;
    }
    if (left->day != right->day) {
        return left->day < right->day ? -1 : 1;
    }
    return 0;
}

static int period_matches(const AnalyticsPeriod *period, const Timestamp *date)
{
    switch (period->type) {
    case ANALYTICS_PERIOD_ALL_TIME:
        return 1;
    case ANALYTICS_PERIOD_DAY:
        return compare_date(date, &period->date) == 0;
    case ANALYTICS_PERIOD_MONTH:
        return date->year == period->year && date->month == period->month;
    case ANALYTICS_PERIOD_YEAR:
        return date->year == period->year;
    case ANALYTICS_PERIOD_DATE_RANGE:
        return compare_date(date, &period->start_date) >= 0
            && compare_date(date, &period->end_date) <= 0;
    }
    return 0;
}

static int period_is_valid(const AnalyticsPeriod *period)
{
    switch (period->type) {
    case ANALYTICS_PERIOD_ALL_TIME:
        return 1;
    case ANALYTICS_PERIOD_DAY:
        return expense_date_is_valid(
            period->date.year,
            period->date.month,
            period->date.day
        );
    case ANALYTICS_PERIOD_MONTH:
        return period->year >= 1 && period->month >= 1 && period->month <= 12;
    case ANALYTICS_PERIOD_YEAR:
        return period->year >= 1;
    case ANALYTICS_PERIOD_DATE_RANGE:
        return expense_date_is_valid(
                period->start_date.year,
                period->start_date.month,
                period->start_date.day
            )
            && expense_date_is_valid(
                period->end_date.year,
                period->end_date.month,
                period->end_date.day
            )
            && compare_date(&period->start_date, &period->end_date) <= 0;
    }
    return 0;
}

static AnalyticsResult calculate_period_summary(
    const ExpenseList *expenses,
    const AnalyticsPeriod *period,
    AnalyticsSummary *summary
)
{
    AnalyticsResult result = ANALYTICS_SUCCESS;

    if (expenses == NULL || period == NULL || summary == NULL
        || !period_is_valid(period)
        || expenses->size > expenses->capacity
        || expenses->capacity > SIZE_MAX / sizeof(*expenses->items)
        || (expenses->size > 0 && expenses->items == NULL)) {
        return ANALYTICS_INVALID_INPUT;
    }

    initialize_summary(summary, 0);

    for (size_t index = 0; index < expenses->size; index++) {
        const Expense *expense = &expenses->items[index];

        if (expense->amount_paise <= 0
            || !expense_date_is_valid(
                expense->timestamp.year,
                expense->timestamp.month,
                expense->timestamp.day
            )) {
            result = ANALYTICS_INVALID_INPUT;
            goto fail;
        }

        if (period_matches(period, &expense->timestamp)
            && !summary_add_expense(summary, expense->amount_paise)) {
            result = ANALYTICS_OVERFLOW;
            goto fail;
        }
    }

    summary_set_average(summary);
    return ANALYTICS_SUCCESS;

fail:
    analytics_summary_destroy(summary);
    return result;
}

AnalyticsResult analytics_calculate_daily_summary(
    const ExpenseList *expenses,
    const Timestamp *date,
    AnalyticsSummary *summary
)
{
    AnalyticsPeriod period = {0};

    if (date == NULL
        || !expense_date_is_valid(date->year, date->month, date->day)) {
        return ANALYTICS_INVALID_INPUT;
    }

    period.type = ANALYTICS_PERIOD_DAY;
    period.date = *date;
    return calculate_period_summary(expenses, &period, summary);
}

AnalyticsResult analytics_calculate_monthly_summary(
    const ExpenseList *expenses,
    int year,
    int month,
    AnalyticsSummary *summary
)
{
    AnalyticsPeriod period = {0};

    if (year < 1 || month < 1 || month > 12) {
        return ANALYTICS_INVALID_INPUT;
    }

    period.type = ANALYTICS_PERIOD_MONTH;
    period.year = year;
    period.month = month;
    return calculate_period_summary(expenses, &period, summary);
}

AnalyticsResult analytics_calculate_yearly_summary(
    const ExpenseList *expenses,
    int year,
    AnalyticsSummary *summary
)
{
    AnalyticsPeriod period = {0};

    if (year < 1) {
        return ANALYTICS_INVALID_INPUT;
    }

    period.type = ANALYTICS_PERIOD_YEAR;
    period.year = year;
    return calculate_period_summary(expenses, &period, summary);
}

AnalyticsResult analytics_calculate_date_range_summary(
    const ExpenseList *expenses,
    const Timestamp *start_date,
    const Timestamp *end_date,
    AnalyticsSummary *summary
)
{
    AnalyticsPeriod period = {0};

    if (start_date == NULL || end_date == NULL
        || !expense_date_is_valid(
            start_date->year,
            start_date->month,
            start_date->day
        )
        || !expense_date_is_valid(
            end_date->year,
            end_date->month,
            end_date->day
        )
        || compare_date(start_date, end_date) > 0) {
        return ANALYTICS_INVALID_INPUT;
    }

    period.type = ANALYTICS_PERIOD_DATE_RANGE;
    period.start_date = *start_date;
    period.end_date = *end_date;
    return calculate_period_summary(expenses, &period, summary);
}

static uint64_t percentage_threshold(
    uint64_t total,
    uint64_t numerator,
    uint64_t denominator
)
{
    uint64_t quotient = total / denominator;
    uint64_t remainder = total % denominator;

    /* Split division first so the scaled total never needs a wide multiply. */
    return quotient * numerator
        + (remainder * numerator + denominator - 1) / denominator;
}

static int category_percentage_basis_points(int64_t part, int64_t total)
{
    const uint64_t scale = 10000;
    uint64_t low = 0;
    uint64_t high = scale;
    uint64_t unsigned_total = (uint64_t)total;
    uint64_t unsigned_part = (uint64_t)part;
    uint64_t floor_basis_points;
    uint64_t midpoint_numerator;
    uint64_t round_up_threshold;

    if (part <= 0 || total <= 0) {
        return 0;
    }

    while (low < high) {
        uint64_t candidate = low + (high - low + 1) / 2;
        uint64_t threshold = percentage_threshold(
            unsigned_total,
            candidate,
            scale
        );

        if (threshold <= unsigned_part) {
            low = candidate;
        } else {
            high = candidate - 1;
        }
    }

    floor_basis_points = low;
    midpoint_numerator = floor_basis_points * 2 + 1;
    round_up_threshold = percentage_threshold(
        unsigned_total,
        midpoint_numerator,
        scale * 2
    );

    if (unsigned_part >= round_up_threshold && floor_basis_points < scale) {
        floor_basis_points++;
    }

    return (int)floor_basis_points;
}

static AnalyticsResult grow_category_results(
    AnalyticsCategoryBreakdown *breakdown
)
{
    size_t new_capacity;
    AnalyticsCategoryTotal *resized_items;

    if (breakdown->capacity == 0) {
        new_capacity = 4;
    } else {
        if (breakdown->capacity > SIZE_MAX / 2) {
            return ANALYTICS_MEMORY_ERROR;
        }
        new_capacity = breakdown->capacity * 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(*breakdown->items)) {
        return ANALYTICS_MEMORY_ERROR;
    }

    resized_items = realloc(
        breakdown->items,
        new_capacity * sizeof(*resized_items)
    );
    if (resized_items == NULL) {
        return ANALYTICS_MEMORY_ERROR;
    }

    breakdown->items = resized_items;
    breakdown->capacity = new_capacity;
    return ANALYTICS_SUCCESS;
}

AnalyticsResult analytics_calculate_category_breakdown(
    const ExpenseList *expenses,
    const CategoryList *categories,
    const AnalyticsPeriod *period,
    AnalyticsCategoryBreakdown *breakdown
)
{
    AnalyticsResult result = ANALYTICS_SUCCESS;

    if (breakdown == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }

    breakdown->items = NULL;
    breakdown->size = 0;
    breakdown->capacity = 0;
    breakdown->total_paise = 0;
    breakdown->transaction_count = 0;

    if (expenses == NULL || categories == NULL
        || expenses->size > expenses->capacity
        || expenses->capacity > SIZE_MAX / sizeof(*expenses->items)
        || (expenses->size > 0 && expenses->items == NULL)
        || categories->size > categories->capacity
        || categories->capacity > SIZE_MAX / sizeof(*categories->items)
        || (categories->size > 0 && categories->items == NULL)
        || (period != NULL && !period_is_valid(period))) {
        return ANALYTICS_INVALID_INPUT;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        const Expense *expense = &expenses->items[index];
        size_t result_index;

        if (expense->amount_paise <= 0
            || !expense_date_is_valid(
                expense->timestamp.year,
                expense->timestamp.month,
                expense->timestamp.day
            )) {
            result = ANALYTICS_INVALID_INPUT;
            goto fail;
        }
        if (period != NULL && !period_matches(period, &expense->timestamp)) {
            continue;
        }

        for (result_index = 0; result_index < breakdown->size; result_index++) {
            if (breakdown->items[result_index].category_id
                == expense->category_id) {
                break;
            }
        }

        if (result_index == breakdown->size) {
            const Category *category = category_find_by_id(
                categories,
                expense->category_id
            );

            if (category == NULL
                || memchr(
                    category->name,
                    '\0',
                    sizeof(category->name)
                ) == NULL) {
                result = ANALYTICS_INVALID_CATEGORY;
                goto fail;
            }
            if (breakdown->size == breakdown->capacity) {
                result = grow_category_results(breakdown);
                if (result != ANALYTICS_SUCCESS) {
                    goto fail;
                }
            }

            breakdown->items[result_index].category_id = category->id;
            memcpy(
                breakdown->items[result_index].category_name,
                category->name,
                sizeof(category->name)
            );
            breakdown->items[result_index].total_paise = 0;
            breakdown->items[result_index].transaction_count = 0;
            breakdown->items[result_index].percentage_basis_points = 0;
            breakdown->size++;
        }

        if (!add_amount(
                &breakdown->items[result_index].total_paise,
                expense->amount_paise
            )
            || !add_amount(&breakdown->total_paise, expense->amount_paise)
            || breakdown->items[result_index].transaction_count == SIZE_MAX
            || breakdown->transaction_count == SIZE_MAX) {
            result = ANALYTICS_OVERFLOW;
            goto fail;
        }
        breakdown->items[result_index].transaction_count++;
        breakdown->transaction_count++;
    }

    if (breakdown->total_paise > 0) {
        for (size_t index = 0; index < breakdown->size; index++) {
            breakdown->items[index].percentage_basis_points =
                (unsigned int)category_percentage_basis_points(
                    breakdown->items[index].total_paise,
                    breakdown->total_paise
                );
        }
    }

    return ANALYTICS_SUCCESS;

fail:
    analytics_category_breakdown_destroy(breakdown);
    return result;
}

void analytics_category_breakdown_destroy(
    AnalyticsCategoryBreakdown *breakdown
)
{
    if (breakdown == NULL) {
        return;
    }

    free(breakdown->items);
    breakdown->items = NULL;
    breakdown->size = 0;
    breakdown->capacity = 0;
    breakdown->total_paise = 0;
    breakdown->transaction_count = 0;
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
