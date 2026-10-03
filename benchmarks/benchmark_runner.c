#include "analytics.h"
#include "benchmark_auxiliary.h"
#include "benchmark_data.h"
#include "query.h"
#include "sorting.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    OP_CATEGORY_LOOKUP,
    OP_QUERY,
    OP_ANALYTICS_SUMMARY,
    OP_CATEGORY_BREAKDOWN,
    OP_MONTHLY_TREND,
    OP_SORTING_INSERTION_BASELINE,
    OP_SORTING_PRODUCTION_MERGE
} BenchmarkOperation;

typedef struct {
    BenchmarkOperation operation;
    const ExpenseList *expenses;
    const CategoryList *categories;
    int category_id;
} BenchmarkContext;

static volatile uint64_t benchmark_sink;

static SortingResult benchmark_insertion_sort_view(
    const ExpenseList *expenses,
    const CategoryList *categories,
    ExpenseView *view
);
static int verify_sorted_view(
    const ExpenseList *expenses,
    const ExpenseView *view,
    const Expense *original_items
);

static void count_match(const Expense *expense, void *context)
{
    (void)expense;
    (*(size_t *)context)++;
}

static const char *operation_name(BenchmarkOperation operation)
{
    switch (operation) {
    case OP_CATEGORY_LOOKUP:
        return "category_lookup";
    case OP_QUERY:
        return "combined_query";
    case OP_ANALYTICS_SUMMARY:
        return "analytics_summary";
    case OP_CATEGORY_BREAKDOWN:
        return "category_breakdown";
    case OP_MONTHLY_TREND:
        return "monthly_trend_12";
    case OP_SORTING_INSERTION_BASELINE:
        return "sorting_insertion_baseline_amount";
    case OP_SORTING_PRODUCTION_MERGE:
        return "sorting_production_merge_amount";
    }

    return "unknown";
}

static int run_operation(const BenchmarkContext *context)
{
    uint64_t result = 0;

    switch (context->operation) {
    case OP_CATEGORY_LOOKUP: {
        const Category *category = category_find_by_id(
            context->categories,
            context->category_id
        );
        result = category == NULL ? 0 : (uint64_t)category->id;
        break;
    }
    case OP_QUERY: {
        QueryFilter filter = {0};
        size_t matches = 0;

        filter.category_enabled = 1;
        filter.category_id = 32;
        filter.amount_enabled = 1;
        filter.minimum_amount_paise = 1000;
        filter.maximum_amount_paise = 500000;
        filter.date_enabled = 1;
        filter.start_date = (QueryDate){2024, 4, 1};
        filter.end_date = (QueryDate){2024, 9, 28};
        filter.note_enabled = 1;
        filter.note = "benchmark";
        if (query_expenses(
                context->expenses,
                &filter,
                count_match,
                &matches
            ) != QUERY_SUCCESS) {
            return 0;
        }
        result = matches;
        break;
    }
    case OP_ANALYTICS_SUMMARY: {
        AnalyticsSummary summary = {0};
        AnalyticsResult status = analytics_calculate_summary(
            context->expenses,
            context->categories,
            &summary
        );
        if (status != ANALYTICS_SUCCESS) {
            return 0;
        }
        result = (uint64_t)summary.total_paise
            ^ (uint64_t)summary.transaction_count;
        analytics_summary_destroy(&summary);
        break;
    }
    case OP_CATEGORY_BREAKDOWN: {
        AnalyticsCategoryBreakdown breakdown = {0};
        AnalyticsResult status = analytics_calculate_category_breakdown(
            context->expenses,
            context->categories,
            NULL,
            &breakdown
        );
        if (status != ANALYTICS_SUCCESS) {
            return 0;
        }
        result = (uint64_t)breakdown.total_paise ^ (uint64_t)breakdown.size;
        analytics_category_breakdown_destroy(&breakdown);
        break;
    }
    case OP_MONTHLY_TREND: {
        AnalyticsTrend trend = {0};
        AnalyticsResult status = analytics_calculate_trend(
            context->expenses,
            ANALYTICS_TREND_MONTHLY,
            2024,
            1,
            12,
            &trend
        );
        if (status != ANALYTICS_SUCCESS) {
            return 0;
        }
        result = (uint64_t)trend.total_paise ^ (uint64_t)trend.period_count;
        analytics_trend_destroy(&trend);
        break;
    }
    case OP_SORTING_INSERTION_BASELINE: {
        ExpenseView view = {0};
        SortingResult status = benchmark_insertion_sort_view(
            context->expenses,
            context->categories,
            &view
        );
        if (status != SORTING_SUCCESS) {
            return 0;
        }
        result = view.size == 0 ? 0 : (uint64_t)view.items[0]->id;
        sorting_view_destroy(&view);
        break;
    }
    case OP_SORTING_PRODUCTION_MERGE: {
        ExpenseView view = {0};
        SortingResult status = sorting_create_view(
            context->expenses,
            context->categories,
            SORT_BY_AMOUNT_LOWEST,
            &view
        );
        if (status != SORTING_SUCCESS) {
            return 0;
        }
        result = view.size == 0 ? 0 : (uint64_t)view.items[0]->id;
        sorting_view_destroy(&view);
        break;
    }
    }

    benchmark_sink ^= result;
    return 1;
}

static size_t operation_repetitions(
    BenchmarkOperation operation,
    size_t data_size
)
{
    if (operation == OP_SORTING_INSERTION_BASELINE
        || operation == OP_SORTING_PRODUCTION_MERGE) {
        if (data_size <= 100) {
            return 10000;
        }
        if (data_size <= 1000) {
            return 100;
        }
        if (data_size <= 10000) {
            return 10;
        }
        return 10;
    }
    if (data_size <= 100) {
        return 10000;
    }
    if (data_size <= 1000) {
        return 1000;
    }
    if (data_size <= 10000) {
        return 100;
    }
    return 25;
}

static int compare_amounts(const Expense *left, const Expense *right)
{
    if (left->amount_paise < right->amount_paise) {
        return -1;
    }
    if (left->amount_paise > right->amount_paise) {
        return 1;
    }
    return 0;
}

static SortingResult benchmark_insertion_sort_view(
    const ExpenseList *expenses,
    const CategoryList *categories,
    ExpenseView *view
)
{
    if (expenses == NULL || categories == NULL || view == NULL
        || expenses->size > expenses->capacity
        || (expenses->size > 0 && expenses->items == NULL)
        || expenses->size > SIZE_MAX / sizeof(*view->items)) {
        return SORTING_INVALID_INPUT;
    }

    view->items = NULL;
    view->size = 0;
    if (expenses->size == 0) {
        return SORTING_SUCCESS;
    }

    view->items = malloc(expenses->size * sizeof(*view->items));
    if (view->items == NULL) {
        return SORTING_MEMORY_ERROR;
    }
    view->size = expenses->size;
    for (size_t index = 0; index < expenses->size; index++) {
        view->items[index] = &expenses->items[index];
    }

    for (size_t index = 1; index < expenses->size; index++) {
        const Expense *current = view->items[index];
        size_t insertion_index = index;

        while (insertion_index > 0
            && compare_amounts(current, view->items[insertion_index - 1]) < 0) {
            view->items[insertion_index] = view->items[insertion_index - 1];
            insertion_index--;
        }
        view->items[insertion_index] = current;
    }

    return SORTING_SUCCESS;
}

static int verify_sorted_view(
    const ExpenseList *expenses,
    const ExpenseView *view,
    const Expense *original_items
)
{
    unsigned char *seen;

    if (view->size != expenses->size
        || (view->size > 0 && view->items == NULL)
        || memcmp(
            expenses->items,
            original_items,
            expenses->size * sizeof(*expenses->items)
        ) != 0) {
        return 0;
    }

    seen = calloc(view->size, sizeof(*seen));
    if (seen == NULL) {
        return 0;
    }

    for (size_t index = 0; index < view->size; index++) {
        const Expense *expense = view->items[index];
        size_t source_index;

        if (expense < expenses->items
            || expense >= expenses->items + expenses->size) {
            free(seen);
            return 0;
        }
        source_index = (size_t)(expense - expenses->items);
        if (source_index != (size_t)(expense->id - 1)
            || seen[source_index]) {
            free(seen);
            return 0;
        }
        seen[source_index] = 1;
        if (index > 0) {
            const Expense *previous = view->items[index - 1];
            if (previous->amount_paise > expense->amount_paise
                || (previous->amount_paise == expense->amount_paise
                    && previous->id > expense->id)) {
                free(seen);
                return 0;
            }
        }
    }

    free(seen);
    return 1;
}

static int verify_sorting_implementations(
    const ExpenseList *expenses,
    const CategoryList *categories,
    size_t expense_count
)
{
    const Expense *original_items = NULL;
    ExpenseView insertion_view = {0};
    ExpenseView production_view = {0};
    int success = 0;
    int verify_insertion = expense_count <= 10000;

    if (expense_count > SIZE_MAX / sizeof(*original_items)) {
        return 0;
    }
    {
        Expense *snapshot = malloc(expense_count * sizeof(*snapshot));
        if (snapshot == NULL) {
            return 0;
        }
        memcpy(snapshot, expenses->items, expense_count * sizeof(*snapshot));
        original_items = snapshot;
    }

    if (verify_insertion) {
        if (benchmark_insertion_sort_view(
                expenses,
                categories,
                &insertion_view
            ) != SORTING_SUCCESS
            || !verify_sorted_view(
                expenses,
                &insertion_view,
                original_items
            )) {
            fprintf(stderr, "Insertion-sort correctness check failed (%zu)\n",
                expense_count);
            goto cleanup;
        }
    }

    if (sorting_create_view(
            expenses,
            categories,
            SORT_BY_AMOUNT_LOWEST,
            &production_view
        ) != SORTING_SUCCESS
        || !verify_sorted_view(expenses, &production_view, original_items)) {
        fprintf(stderr, "Production merge-sort correctness check failed (%zu)\n",
            expense_count);
        goto cleanup;
    }

    if (verify_insertion) {
        for (size_t index = 0; index < expense_count; index++) {
            if (insertion_view.items[index] != production_view.items[index]) {
                fprintf(stderr, "Sorting outputs differ at index %zu (%zu)\n",
                    index, expense_count);
                goto cleanup;
            }
        }
    }
    success = 1;

cleanup:
    free((void *)original_items);
    sorting_view_destroy(&insertion_view);
    sorting_view_destroy(&production_view);
    return success;
}

static int report_benchmark(
    BenchmarkContext context,
    size_t data_size,
    size_t repetitions
)
{
    clock_t start;
    clock_t end;
    double elapsed_seconds;

    if (!run_operation(&context)) {
        fprintf(stderr, "Benchmark warm-up failed: %s\n",
            operation_name(context.operation));
        return 0;
    }

    start = clock();
    if (start == (clock_t)-1) {
        fprintf(stderr, "clock() failed before %s\n",
            operation_name(context.operation));
        return 0;
    }
    for (size_t iteration = 0; iteration < repetitions; iteration++) {
        if (!run_operation(&context)) {
            fprintf(stderr, "Benchmark operation failed: %s\n",
                operation_name(context.operation));
            return 0;
        }
    }
    end = clock();
    if (end == (clock_t)-1 || end < start) {
        fprintf(stderr, "clock() failed during %s\n",
            operation_name(context.operation));
        return 0;
    }

    elapsed_seconds = (double)(end - start) / (double)CLOCKS_PER_SEC;
    printf("%s,%zu,%zu,%.6f,%.3f,%" PRIu64 "\n",
        operation_name(context.operation),
        data_size,
        repetitions,
        elapsed_seconds * 1000.0,
        repetitions == 0
            ? 0.0
            : elapsed_seconds * 1000000000.0 / (double)repetitions,
        benchmark_sink);
    return 1;
}

static int benchmark_category_lookup(size_t category_count)
{
    CategoryList categories;
    BenchmarkContext context = {0};
    size_t repetitions = 2000000 / category_count;
    int success;

    if (repetitions < 100) {
        repetitions = 100;
    }
    if (!benchmark_categories_create(category_count, &categories)) {
        fprintf(stderr, "Unable to generate categories (%zu)\n", category_count);
        return 0;
    }

    context.operation = OP_CATEGORY_LOOKUP;
    context.categories = &categories;
    context.category_id = (int)category_count;
    success = report_benchmark(context, category_count, repetitions);
    category_list_destroy(&categories);
    return success;
}

static int benchmark_expense_operations(size_t expense_count)
{
    const size_t category_count = 64;
    CategoryList categories;
    ExpenseList expenses;
    const BenchmarkOperation operations[] = {
        OP_QUERY,
        OP_ANALYTICS_SUMMARY,
        OP_CATEGORY_BREAKDOWN,
        OP_MONTHLY_TREND,
        OP_SORTING_INSERTION_BASELINE,
        OP_SORTING_PRODUCTION_MERGE
    };
    int success = 1;

    if (!benchmark_data_create(
            expense_count,
            category_count,
            &categories,
            &expenses
        )) {
        fprintf(stderr, "Unable to generate expenses (%zu)\n", expense_count);
        return 0;
    }

    if (!verify_sorting_implementations(
            &expenses,
            &categories,
            expense_count
        )) {
        expense_list_destroy(&expenses);
        category_list_destroy(&categories);
        return 0;
    }

    for (size_t index = 0;
         index < sizeof(operations) / sizeof(operations[0]);
         index++) {
        BenchmarkContext context = {0};
        size_t repetitions = operation_repetitions(
            operations[index],
            expense_count
        );

        if (operations[index] == OP_SORTING_INSERTION_BASELINE
            && expense_count > 10000) {
            continue;
        }

        context.operation = operations[index];
        context.expenses = &expenses;
        context.categories = &categories;
        if (!report_benchmark(context, expense_count, repetitions)) {
            success = 0;
            break;
        }
    }

    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
    return success;
}

int main(void)
{
    const size_t dataset_sizes[] = {100, 1000, 10000, 100000};

    puts("operation,dataset_size,repetitions,total_cpu_ms,mean_cpu_ns_per_operation,checksum");
    for (size_t index = 0;
         index < sizeof(dataset_sizes) / sizeof(dataset_sizes[0]);
         index++) {
        if (!benchmark_category_lookup(dataset_sizes[index])
            || !benchmark_expense_operations(dataset_sizes[index])) {
            return EXIT_FAILURE;
        }
    }

    if (!benchmark_storage_and_category_enumeration()) {
        return EXIT_FAILURE;
    }

    fprintf(stderr, "benchmark_sink=%" PRIu64 "\n", benchmark_sink);
    return EXIT_SUCCESS;
}
