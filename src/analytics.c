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

static int subtract_amounts(int64_t left, int64_t right, int64_t *difference)
{
    if (difference == NULL
        || (right > 0 && left < INT64_MIN + right)
        || (right < 0 && left > INT64_MAX + right)) {
        return 0;
    }

    *difference = left - right;
    return 1;
}

static uint64_t scaled_ratio_threshold(
    uint64_t denominator,
    uint64_t numerator,
    uint64_t scale
)
{
    uint64_t quotient = denominator / scale;
    uint64_t remainder = denominator % scale;

    return quotient * numerator
        + (remainder * numerator + scale - 1) / scale;
}

static int ratio_basis_points(
    uint64_t numerator,
    uint64_t denominator,
    uint64_t *basis_points
)
{
    const uint64_t scale = 10000;
    uint64_t whole;
    uint64_t remainder;
    uint64_t low = 0;
    uint64_t high = scale;
    uint64_t fractional;

    if (denominator == 0 || basis_points == NULL) {
        return 0;
    }
    if (numerator == 0) {
        *basis_points = 0;
        return 1;
    }

    whole = numerator / denominator;
    remainder = numerator % denominator;
    if (whole > UINT64_MAX / scale) {
        return 0;
    }

    while (low < high) {
        uint64_t candidate = low + (high - low + 1) / 2;
        uint64_t threshold = scaled_ratio_threshold(
            denominator,
            candidate,
            scale
        );

        if (threshold <= remainder) {
            low = candidate;
        } else {
            high = candidate - 1;
        }
    }

    fractional = low;
    if (remainder >= scaled_ratio_threshold(
            denominator,
            fractional * 2 + 1,
            scale * 2
        )) {
        fractional++;
    }

    if (whole == UINT64_MAX / scale && fractional > UINT64_MAX % scale) {
        return 0;
    }
    *basis_points = whole * scale + fractional;
    return 1;
}

static AnalyticsResult calculate_percentage_change(
    int64_t baseline,
    int64_t change,
    AnalyticsPercentageChange *percentage
)
{
    uint64_t magnitude;

    if (percentage == NULL || baseline < 0) {
        return ANALYTICS_INVALID_INPUT;
    }

    percentage->state = ANALYTICS_PERCENTAGE_DEFINED;
    percentage->is_negative = change < 0;
    percentage->basis_points = 0;

    if (baseline == 0) {
        if (change == 0) {
            percentage->state =
                ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE;
            percentage->is_negative = 0;
            return ANALYTICS_SUCCESS;
        }
        if (change > 0) {
            percentage->state =
                ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE;
            percentage->is_negative = 0;
            return ANALYTICS_SUCCESS;
        }
        return ANALYTICS_INVALID_INPUT;
    }

    magnitude = (uint64_t)(change < 0 ? -change : change);
    if (!ratio_basis_points(
            magnitude,
            (uint64_t)baseline,
            &percentage->basis_points
        )) {
        return ANALYTICS_OVERFLOW;
    }
    return ANALYTICS_SUCCESS;
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

        if (expense->amount_paise <= 0
            || !expense_timestamp_is_valid(&expense->timestamp)) {
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
            || !expense_timestamp_is_valid(&expense->timestamp)) {
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

static AnalyticsCategoryComparison *find_category_comparison(
    AnalyticsComparison *comparison,
    int category_id
)
{
    for (size_t index = 0; index < comparison->category_count; index++) {
        if (comparison->categories[index].category_id == category_id) {
            return &comparison->categories[index];
        }
    }
    return NULL;
}

static AnalyticsResult append_category_comparison(
    AnalyticsComparison *comparison,
    const AnalyticsCategoryTotal *source,
    AnalyticsCategoryComparison **appended
)
{
    size_t new_capacity;
    AnalyticsCategoryComparison *resized;
    AnalyticsCategoryComparison *item;

    if (comparison->category_count == comparison->category_capacity) {
        if (comparison->category_capacity == 0) {
            new_capacity = 4;
        } else {
            if (comparison->category_capacity > SIZE_MAX / 2) {
                return ANALYTICS_MEMORY_ERROR;
            }
            new_capacity = comparison->category_capacity * 2;
        }
        if (new_capacity > SIZE_MAX / sizeof(*comparison->categories)) {
            return ANALYTICS_MEMORY_ERROR;
        }

        resized = realloc(
            comparison->categories,
            new_capacity * sizeof(*resized)
        );
        if (resized == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
        comparison->categories = resized;
        comparison->category_capacity = new_capacity;
    }

    item = &comparison->categories[comparison->category_count++];
    memset(item, 0, sizeof(*item));
    item->category_id = source->category_id;
    memcpy(item->category_name, source->category_name, sizeof(item->category_name));
    *appended = item;
    return ANALYTICS_SUCCESS;
}

static void copy_category_period_a(
    AnalyticsCategoryComparison *target,
    const AnalyticsCategoryTotal *source
)
{
    target->period_a_total_paise = source->total_paise;
    target->period_a_transaction_count = source->transaction_count;
    target->period_a_percentage_basis_points =
        source->percentage_basis_points;
}

static void copy_category_period_b(
    AnalyticsCategoryComparison *target,
    const AnalyticsCategoryTotal *source
)
{
    target->period_b_total_paise = source->total_paise;
    target->period_b_transaction_count = source->transaction_count;
    target->period_b_percentage_basis_points =
        source->percentage_basis_points;
}

AnalyticsResult analytics_compare_periods(
    const ExpenseList *expenses,
    const CategoryList *categories,
    const AnalyticsPeriod *period_a,
    const AnalyticsPeriod *period_b,
    AnalyticsComparison *comparison
)
{
    AnalyticsCategoryBreakdown breakdown_a = {0};
    AnalyticsCategoryBreakdown breakdown_b = {0};
    AnalyticsResult result;

    if (comparison == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }

    memset(comparison, 0, sizeof(*comparison));
    if (expenses == NULL || categories == NULL || period_a == NULL
        || period_b == NULL || !period_is_valid(period_a)
        || !period_is_valid(period_b)) {
        return ANALYTICS_INVALID_INPUT;
    }

    result = calculate_period_summary(
        expenses,
        period_a,
        &comparison->period_a
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }
    result = calculate_period_summary(
        expenses,
        period_b,
        &comparison->period_b
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }
    if (!subtract_amounts(
            comparison->period_b.total_paise,
            comparison->period_a.total_paise,
            &comparison->absolute_change_paise
        )) {
        result = ANALYTICS_OVERFLOW;
        goto fail;
    }
    result = calculate_percentage_change(
        comparison->period_a.total_paise,
        comparison->absolute_change_paise,
        &comparison->percentage_change
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }

    result = analytics_calculate_category_breakdown(
        expenses,
        categories,
        period_a,
        &breakdown_a
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }
    result = analytics_calculate_category_breakdown(
        expenses,
        categories,
        period_b,
        &breakdown_b
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }

    for (size_t index = 0; index < breakdown_a.size; index++) {
        AnalyticsCategoryComparison *item;

        result = append_category_comparison(
            comparison,
            &breakdown_a.items[index],
            &item
        );
        if (result != ANALYTICS_SUCCESS) {
            goto fail;
        }
        copy_category_period_a(item, &breakdown_a.items[index]);
    }

    for (size_t index = 0; index < breakdown_b.size; index++) {
        AnalyticsCategoryComparison *item = find_category_comparison(
            comparison,
            breakdown_b.items[index].category_id
        );

        if (item == NULL) {
            result = append_category_comparison(
                comparison,
                &breakdown_b.items[index],
                &item
            );
            if (result != ANALYTICS_SUCCESS) {
                goto fail;
            }
        }
        copy_category_period_b(item, &breakdown_b.items[index]);
    }

    for (size_t index = 0; index < comparison->category_count; index++) {
        AnalyticsCategoryComparison *item = &comparison->categories[index];

        if (!subtract_amounts(
                item->period_b_total_paise,
                item->period_a_total_paise,
                &item->absolute_change_paise
            )) {
            result = ANALYTICS_OVERFLOW;
            goto fail;
        }
        result = calculate_percentage_change(
            item->period_a_total_paise,
            item->absolute_change_paise,
            &item->percentage_change
        );
        if (result != ANALYTICS_SUCCESS) {
            goto fail;
        }
    }

    analytics_category_breakdown_destroy(&breakdown_a);
    analytics_category_breakdown_destroy(&breakdown_b);
    return ANALYTICS_SUCCESS;

fail:
    analytics_category_breakdown_destroy(&breakdown_a);
    analytics_category_breakdown_destroy(&breakdown_b);
    analytics_comparison_destroy(comparison);
    return result;
}

void analytics_comparison_destroy(AnalyticsComparison *comparison)
{
    if (comparison == NULL) {
        return;
    }

    analytics_summary_destroy(&comparison->period_a);
    analytics_summary_destroy(&comparison->period_b);
    free(comparison->categories);
    comparison->categories = NULL;
    comparison->category_count = 0;
    comparison->category_capacity = 0;
    comparison->absolute_change_paise = 0;
    comparison->percentage_change.state =
        ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE;
    comparison->percentage_change.is_negative = 0;
    comparison->percentage_change.basis_points = 0;
}

static void initialize_trend(AnalyticsTrend *trend)
{
    memset(trend, 0, sizeof(*trend));
    trend->first_to_last_percentage_change.state =
        ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE;
}

static AnalyticsResult calculate_trend_period(
    const ExpenseList *expenses,
    AnalyticsTrendType type,
    int year,
    int month,
    AnalyticsTrendPeriod *trend_period
)
{
    AnalyticsPeriod period = {0};
    AnalyticsSummary summary;
    AnalyticsResult result;

    if (type == ANALYTICS_TREND_MONTHLY) {
        period.type = ANALYTICS_PERIOD_MONTH;
        period.year = year;
        period.month = month;
        result = calculate_period_summary(expenses, &period, &summary);
    } else {
        period.type = ANALYTICS_PERIOD_YEAR;
        period.year = year;
        result = calculate_period_summary(expenses, &period, &summary);
    }

    if (result != ANALYTICS_SUCCESS) {
        return result;
    }

    trend_period->year = year;
    trend_period->month = type == ANALYTICS_TREND_MONTHLY ? month : 0;
    trend_period->total_paise = summary.total_paise;
    trend_period->transaction_count = summary.transaction_count;
    trend_period->average_paise = summary.average_paise;
    trend_period->average_remainder_paise =
        summary.average_remainder_paise;
    analytics_summary_destroy(&summary);
    return ANALYTICS_SUCCESS;
}

AnalyticsResult analytics_calculate_trend(
    const ExpenseList *expenses,
    AnalyticsTrendType type,
    int start_year,
    int start_month,
    size_t period_count,
    AnalyticsTrend *trend
)
{
    size_t maximum_period_count;
    int year;
    int month;
    AnalyticsResult result = ANALYTICS_SUCCESS;

    if (trend == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }
    initialize_trend(trend);

    if (type == ANALYTICS_TREND_MONTHLY) {
        maximum_period_count = ANALYTICS_MAX_MONTHLY_TREND_PERIODS;
        if (start_month < 1 || start_month > 12) {
            return ANALYTICS_INVALID_INPUT;
        }
    } else if (type == ANALYTICS_TREND_YEARLY) {
        maximum_period_count = ANALYTICS_MAX_YEARLY_TREND_PERIODS;
        if (start_month != 0) {
            return ANALYTICS_INVALID_INPUT;
        }
    } else {
        return ANALYTICS_INVALID_INPUT;
    }

    if (expenses == NULL || start_year < 1 || start_year > 9999
        || period_count == 0 || period_count > maximum_period_count
        || expenses->size > expenses->capacity
        || expenses->capacity > SIZE_MAX / sizeof(*expenses->items)
        || (expenses->size > 0 && expenses->items == NULL)
        || period_count > SIZE_MAX / sizeof(*trend->periods)) {
        return ANALYTICS_INVALID_INPUT;
    }

    if (type == ANALYTICS_TREND_MONTHLY) {
        size_t months_after_start = period_count - 1;
        size_t starting_month_index = (size_t)(start_year - 1) * 12
            + (size_t)(start_month - 1);
        size_t final_month_index = starting_month_index + months_after_start;
        if (final_month_index >= (size_t)9999 * 12) {
            return ANALYTICS_INVALID_INPUT;
        }
    } else if (period_count - 1 > (size_t)(9999 - start_year)) {
        return ANALYTICS_INVALID_INPUT;
    }

    trend->periods = calloc(period_count, sizeof(*trend->periods));
    if (trend->periods == NULL) {
        return ANALYTICS_MEMORY_ERROR;
    }
    trend->period_count = period_count;
    year = start_year;
    month = start_month;

    for (size_t index = 0; index < period_count; index++) {
        AnalyticsTrendPeriod *period = &trend->periods[index];

        result = calculate_trend_period(
            expenses,
            type,
            year,
            month,
            period
        );
        if (result != ANALYTICS_SUCCESS) {
            goto fail;
        }
        if (!add_amount(&trend->total_paise, period->total_paise)) {
            result = ANALYTICS_OVERFLOW;
            goto fail;
        }

        if (index == 0) {
            trend->highest_period_index = 0;
            trend->lowest_period_index = 0;
        } else {
            const int64_t previous =
                trend->periods[index - 1].total_paise;

            if (period->total_paise > previous) {
                trend->increasing_transitions++;
            } else if (period->total_paise < previous) {
                trend->decreasing_transitions++;
            } else {
                trend->unchanged_transitions++;
            }

            if (period->total_paise
                > trend->periods[trend->highest_period_index].total_paise) {
                trend->highest_period_index = index;
            }
            if (period->total_paise
                < trend->periods[trend->lowest_period_index].total_paise) {
                trend->lowest_period_index = index;
            }
        }

        if (type == ANALYTICS_TREND_MONTHLY) {
            if (month == 12) {
                month = 1;
                year++;
            } else {
                month++;
            }
        } else {
            year++;
        }
    }

    trend->average_period_paise =
        trend->total_paise / (int64_t)period_count;
    trend->average_period_remainder_paise =
        trend->total_paise % (int64_t)period_count;

    if (!subtract_amounts(
            trend->periods[period_count - 1].total_paise,
            trend->periods[0].total_paise,
            &trend->first_to_last_change_paise)) {
        result = ANALYTICS_OVERFLOW;
        goto fail;
    }
    result = calculate_percentage_change(
        trend->periods[0].total_paise,
        trend->first_to_last_change_paise,
        &trend->first_to_last_percentage_change
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }
    return ANALYTICS_SUCCESS;

fail:
    analytics_trend_destroy(trend);
    return result;
}

void analytics_trend_destroy(AnalyticsTrend *trend)
{
    if (trend == NULL) {
        return;
    }

    free(trend->periods);
    initialize_trend(trend);
}

static uint64_t amount_magnitude(int64_t amount)
{
    if (amount >= 0) {
        return (uint64_t)amount;
    }
    return (uint64_t)(-(amount + 1)) + 1;
}

static AnalyticsResult copy_category_insight_winners(
    const AnalyticsCategoryBreakdown *breakdown,
    int64_t highest,
    int64_t lowest,
    AnalyticsCategoryInsights *insights
)
{
    for (size_t index = 0; index < breakdown->size; index++) {
        if (breakdown->items[index].total_paise == highest) {
            insights->highest_count++;
        }
        if (breakdown->items[index].total_paise == lowest) {
            insights->lowest_count++;
        }
    }

    if (insights->highest_count > SIZE_MAX / sizeof(*insights->highest_spending)
        || insights->lowest_count > SIZE_MAX / sizeof(*insights->lowest_spending)) {
        return ANALYTICS_MEMORY_ERROR;
    }

    if (insights->highest_count > 0) {
        insights->highest_spending = malloc(
            insights->highest_count * sizeof(*insights->highest_spending)
        );
        if (insights->highest_spending == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }
    if (insights->lowest_count > 0) {
        insights->lowest_spending = malloc(
            insights->lowest_count * sizeof(*insights->lowest_spending)
        );
        if (insights->lowest_spending == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }

    insights->highest_count = 0;
    insights->lowest_count = 0;
    for (size_t index = 0; index < breakdown->size; index++) {
        const AnalyticsCategoryTotal *category = &breakdown->items[index];

        if (category->total_paise == highest) {
            insights->highest_spending[insights->highest_count++] = *category;
        }
        if (category->total_paise == lowest) {
            insights->lowest_spending[insights->lowest_count++] = *category;
        }
    }
    return ANALYTICS_SUCCESS;
}

AnalyticsResult analytics_extract_category_insights(
    const AnalyticsCategoryBreakdown *breakdown,
    AnalyticsCategoryInsights *insights
)
{
    int64_t highest;
    int64_t lowest;
    int64_t category_total = 0;
    size_t transaction_count = 0;
    AnalyticsResult result;

    if (insights == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }
    memset(insights, 0, sizeof(*insights));
    insights->highest_share_state =
        ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE;

    if (breakdown == NULL
        || breakdown->size > breakdown->capacity
        || breakdown->capacity > SIZE_MAX / sizeof(*breakdown->items)
        || (breakdown->size > 0 && breakdown->items == NULL)
        || breakdown->total_paise < 0) {
        return ANALYTICS_INVALID_INPUT;
    }

    if (breakdown->size == 0) {
        return breakdown->total_paise == 0
                && breakdown->transaction_count == 0
            ? ANALYTICS_SUCCESS
            : ANALYTICS_INVALID_INPUT;
    }

    highest = breakdown->items[0].total_paise;
    lowest = highest;
    for (size_t index = 0; index < breakdown->size; index++) {
        const AnalyticsCategoryTotal *category = &breakdown->items[index];

        if (category->category_id < 1
            || category->total_paise <= 0
            || category->transaction_count == 0
            || category->percentage_basis_points > 10000
            || memchr(
                category->category_name,
                '\0',
                sizeof(category->category_name)
            ) == NULL) {
            return ANALYTICS_INVALID_INPUT;
        }
        if (!add_amount(&category_total, category->total_paise)
            || category->transaction_count > SIZE_MAX - transaction_count) {
            return ANALYTICS_OVERFLOW;
        }
        transaction_count += category->transaction_count;
        if (category->total_paise > highest) {
            highest = category->total_paise;
        }
        if (category->total_paise < lowest) {
            lowest = category->total_paise;
        }
    }

    if (category_total != breakdown->total_paise
        || transaction_count != breakdown->transaction_count) {
        return ANALYTICS_INVALID_INPUT;
    }

    insights->total_paise = breakdown->total_paise;
    insights->highest_share_state = ANALYTICS_PERCENTAGE_DEFINED;
    result = copy_category_insight_winners(
        breakdown,
        highest,
        lowest,
        insights
    );
    if (result != ANALYTICS_SUCCESS) {
        analytics_category_insights_destroy(insights);
        return result;
    }
    insights->total_paise = breakdown->total_paise;
    insights->highest_share_state = ANALYTICS_PERCENTAGE_DEFINED;
    return ANALYTICS_SUCCESS;
}

static AnalyticsResult copy_period_insight_winners(
    const AnalyticsTrend *trend,
    int64_t highest,
    int64_t lowest,
    AnalyticsPeriodInsights *insights
)
{
    for (size_t index = 0; index < trend->period_count; index++) {
        if (trend->periods[index].total_paise == highest) {
            insights->highest_count++;
        }
        if (trend->periods[index].total_paise == lowest) {
            insights->lowest_count++;
        }
    }

    if (insights->highest_count > SIZE_MAX / sizeof(*insights->highest_spending)
        || insights->lowest_count > SIZE_MAX / sizeof(*insights->lowest_spending)) {
        return ANALYTICS_MEMORY_ERROR;
    }

    if (insights->highest_count > 0) {
        insights->highest_spending = malloc(
            insights->highest_count * sizeof(*insights->highest_spending)
        );
        if (insights->highest_spending == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }
    if (insights->lowest_count > 0) {
        insights->lowest_spending = malloc(
            insights->lowest_count * sizeof(*insights->lowest_spending)
        );
        if (insights->lowest_spending == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }

    insights->highest_count = 0;
    insights->lowest_count = 0;
    for (size_t index = 0; index < trend->period_count; index++) {
        const AnalyticsTrendPeriod *period = &trend->periods[index];

        if (period->total_paise == highest) {
            insights->highest_spending[insights->highest_count++] = *period;
        }
        if (period->total_paise == lowest) {
            insights->lowest_spending[insights->lowest_count++] = *period;
        }
    }
    return ANALYTICS_SUCCESS;
}

AnalyticsResult analytics_extract_period_insights(
    const AnalyticsTrend *trend,
    AnalyticsPeriodInsights *insights
)
{
    int64_t highest;
    int64_t lowest;
    int64_t total = 0;
    AnalyticsResult result;

    if (insights == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }
    memset(insights, 0, sizeof(*insights));

    if (trend == NULL || trend->periods == NULL || trend->period_count == 0
        || trend->period_count > SIZE_MAX / sizeof(*trend->periods)
        || trend->period_count > (size_t)INT64_MAX) {
        return ANALYTICS_INVALID_INPUT;
    }

    highest = trend->periods[0].total_paise;
    lowest = highest;
    for (size_t index = 0; index < trend->period_count; index++) {
        const AnalyticsTrendPeriod *period = &trend->periods[index];
        int64_t expected_average;
        int64_t expected_remainder;

        if (period->year < 1 || period->year > 9999
            || (period->month != 0
                && (period->month < 1 || period->month > 12))
            || period->total_paise < 0
            || period->average_paise < 0
            || period->average_remainder_paise < 0
            || (period->transaction_count == 0
                && (period->average_paise != 0
                    || period->average_remainder_paise != 0
                    || period->total_paise != 0))) {
            return ANALYTICS_INVALID_INPUT;
        }
        if (!add_amount(&total, period->total_paise)) {
            return ANALYTICS_OVERFLOW;
        }

        if (period->transaction_count == 0) {
            expected_average = 0;
            expected_remainder = 0;
        } else if (period->transaction_count <= (size_t)INT64_MAX) {
            int64_t divisor = (int64_t)period->transaction_count;
            expected_average = period->total_paise / divisor;
            expected_remainder = period->total_paise % divisor;
        } else {
            expected_average = 0;
            expected_remainder = period->total_paise;
        }
        if (period->average_paise != expected_average
            || period->average_remainder_paise != expected_remainder
            || (period->transaction_count > 0
                && (uint64_t)period->average_remainder_paise
                    >= (uint64_t)period->transaction_count)) {
            return ANALYTICS_INVALID_INPUT;
        }

        if (period->total_paise > highest) {
            highest = period->total_paise;
        }
        if (period->total_paise < lowest) {
            lowest = period->total_paise;
        }
    }

    if (total != trend->total_paise
        || trend->average_period_paise
            != total / (int64_t)trend->period_count
        || trend->average_period_remainder_paise
            != total % (int64_t)trend->period_count) {
        return ANALYTICS_INVALID_INPUT;
    }

    insights->total_paise = total;
    insights->average_period_paise = trend->average_period_paise;
    insights->average_period_remainder_paise =
        trend->average_period_remainder_paise;
    result = copy_period_insight_winners(
        trend,
        highest,
        lowest,
        insights
    );
    if (result != ANALYTICS_SUCCESS) {
        analytics_period_insights_destroy(insights);
        return result;
    }
    insights->total_paise = total;
    insights->average_period_paise = trend->average_period_paise;
    insights->average_period_remainder_paise =
        trend->average_period_remainder_paise;
    return ANALYTICS_SUCCESS;
}

static AnalyticsResult copy_comparison_insight_winners(
    const AnalyticsComparison *comparison,
    uint64_t largest_absolute,
    int64_t largest_increase,
    int64_t largest_decrease,
    AnalyticsComparisonInsights *insights
)
{
    for (size_t index = 0; index < comparison->category_count; index++) {
        const AnalyticsCategoryComparison *category =
            &comparison->categories[index];
        if (amount_magnitude(category->absolute_change_paise)
            == largest_absolute) {
            insights->largest_absolute_count++;
        }
        if (largest_increase > 0
            && category->absolute_change_paise == largest_increase) {
            insights->largest_increase_count++;
        }
        if (largest_decrease < 0
            && category->absolute_change_paise == largest_decrease) {
            insights->largest_decrease_count++;
        }
    }

    if (insights->largest_absolute_count
            > SIZE_MAX / sizeof(*insights->largest_absolute_change)
        || insights->largest_increase_count
            > SIZE_MAX / sizeof(*insights->largest_increase)
        || insights->largest_decrease_count
            > SIZE_MAX / sizeof(*insights->largest_decrease)) {
        return ANALYTICS_MEMORY_ERROR;
    }

    if (insights->largest_absolute_count > 0) {
        insights->largest_absolute_change = malloc(
            insights->largest_absolute_count
                * sizeof(*insights->largest_absolute_change)
        );
        if (insights->largest_absolute_change == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }
    if (insights->largest_increase_count > 0) {
        insights->largest_increase = malloc(
            insights->largest_increase_count
                * sizeof(*insights->largest_increase)
        );
        if (insights->largest_increase == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }
    if (insights->largest_decrease_count > 0) {
        insights->largest_decrease = malloc(
            insights->largest_decrease_count
                * sizeof(*insights->largest_decrease)
        );
        if (insights->largest_decrease == NULL) {
            return ANALYTICS_MEMORY_ERROR;
        }
    }

    insights->largest_absolute_count = 0;
    insights->largest_increase_count = 0;
    insights->largest_decrease_count = 0;
    for (size_t index = 0; index < comparison->category_count; index++) {
        const AnalyticsCategoryComparison *category =
            &comparison->categories[index];
        const int64_t change = category->absolute_change_paise;

        if (amount_magnitude(change) == largest_absolute) {
            insights->largest_absolute_change[
                insights->largest_absolute_count++
            ] = *category;
        }
        if (largest_increase > 0 && change == largest_increase) {
            insights->largest_increase[
                insights->largest_increase_count++
            ] = *category;
        }
        if (largest_decrease < 0 && change == largest_decrease) {
            insights->largest_decrease[
                insights->largest_decrease_count++
            ] = *category;
        }
    }
    return ANALYTICS_SUCCESS;
}

AnalyticsResult analytics_extract_comparison_insights(
    const AnalyticsComparison *comparison,
    AnalyticsComparisonInsights *insights
)
{
    uint64_t largest_absolute = 0;
    int64_t largest_increase = 0;
    int64_t largest_decrease = 0;
    AnalyticsResult result;

    if (insights == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }
    memset(insights, 0, sizeof(*insights));

    if (comparison == NULL
        || comparison->category_count > comparison->category_capacity
        || comparison->category_capacity
            > SIZE_MAX / sizeof(*comparison->categories)
        || (comparison->category_count > 0
            && comparison->categories == NULL)) {
        return ANALYTICS_INVALID_INPUT;
    }

    for (size_t index = 0; index < comparison->category_count; index++) {
        const AnalyticsCategoryComparison *category =
            &comparison->categories[index];
        int64_t expected_change;
        uint64_t magnitude;

        if (category->category_id < 1
            || category->period_a_total_paise < 0
            || category->period_b_total_paise < 0
            || category->period_a_percentage_basis_points > 10000
            || category->period_b_percentage_basis_points > 10000
            || memchr(
                category->category_name,
                '\0',
                sizeof(category->category_name)
            ) == NULL
            || !subtract_amounts(
                category->period_b_total_paise,
                category->period_a_total_paise,
                &expected_change
            )
            || category->absolute_change_paise != expected_change) {
            return ANALYTICS_INVALID_INPUT;
        }

        magnitude = amount_magnitude(category->absolute_change_paise);
        if (index == 0 || magnitude > largest_absolute) {
            largest_absolute = magnitude;
        }
        if (category->absolute_change_paise > largest_increase) {
            largest_increase = category->absolute_change_paise;
        }
        if (category->absolute_change_paise < largest_decrease) {
            largest_decrease = category->absolute_change_paise;
        }
    }

    result = copy_comparison_insight_winners(
        comparison,
        largest_absolute,
        largest_increase,
        largest_decrease,
        insights
    );
    if (result != ANALYTICS_SUCCESS) {
        analytics_comparison_insights_destroy(insights);
    }
    return result;
}

void analytics_category_insights_destroy(
    AnalyticsCategoryInsights *insights
)
{
    if (insights == NULL) {
        return;
    }

    free(insights->highest_spending);
    free(insights->lowest_spending);
    memset(insights, 0, sizeof(*insights));
    insights->highest_share_state =
        ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE;
}

void analytics_period_insights_destroy(AnalyticsPeriodInsights *insights)
{
    if (insights == NULL) {
        return;
    }

    free(insights->highest_spending);
    free(insights->lowest_spending);
    memset(insights, 0, sizeof(*insights));
}

void analytics_comparison_insights_destroy(
    AnalyticsComparisonInsights *insights
)
{
    if (insights == NULL) {
        return;
    }

    free(insights->largest_absolute_change);
    free(insights->largest_increase);
    free(insights->largest_decrease);
    memset(insights, 0, sizeof(*insights));
}

AnalyticsResult analytics_calculate_report(
    const ExpenseList *expenses,
    const CategoryList *categories,
    const AnalyticsPeriod *period,
    AnalyticsReport *report
)
{
    AnalyticsResult result;

    if (report == NULL) {
        return ANALYTICS_INVALID_INPUT;
    }
    memset(report, 0, sizeof(*report));

    if (expenses == NULL || categories == NULL || period == NULL
        || !period_is_valid(period)
        || (period->type != ANALYTICS_PERIOD_ALL_TIME
            && period->type != ANALYTICS_PERIOD_MONTH
            && period->type != ANALYTICS_PERIOD_YEAR
            && period->type != ANALYTICS_PERIOD_DATE_RANGE)) {
        return ANALYTICS_INVALID_INPUT;
    }

    report->period = *period;
    if (period->type == ANALYTICS_PERIOD_ALL_TIME) {
        result = analytics_calculate_summary(
            expenses,
            categories,
            &report->summary
        );
    } else if (period->type == ANALYTICS_PERIOD_MONTH) {
        result = analytics_calculate_monthly_summary(
            expenses,
            period->year,
            period->month,
            &report->summary
        );
    } else if (period->type == ANALYTICS_PERIOD_YEAR) {
        result = analytics_calculate_yearly_summary(
            expenses,
            period->year,
            &report->summary
        );
    } else {
        result = analytics_calculate_date_range_summary(
            expenses,
            &period->start_date,
            &period->end_date,
            &report->summary
        );
    }
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }

    result = analytics_calculate_category_breakdown(
        expenses,
        categories,
        period->type == ANALYTICS_PERIOD_ALL_TIME ? NULL : period,
        &report->category_breakdown
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }
    if (report->summary.total_paise != report->category_breakdown.total_paise
        || report->summary.transaction_count
            != report->category_breakdown.transaction_count) {
        result = ANALYTICS_INVALID_INPUT;
        goto fail;
    }

    result = analytics_extract_category_insights(
        &report->category_breakdown,
        &report->category_insights
    );
    if (result != ANALYTICS_SUCCESS) {
        goto fail;
    }

    return ANALYTICS_SUCCESS;

fail:
    analytics_report_destroy(report);
    return result;
}

void analytics_report_destroy(AnalyticsReport *report)
{
    if (report == NULL) {
        return;
    }

    analytics_summary_destroy(&report->summary);
    analytics_category_breakdown_destroy(&report->category_breakdown);
    analytics_category_insights_destroy(&report->category_insights);
    memset(&report->period, 0, sizeof(report->period));
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
