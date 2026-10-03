#include "benchmark_auxiliary.h"

#include "benchmark_data.h"
#include "storage.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define BENCHMARK_PROCESS_ID _getpid
#else
#include <unistd.h>
#define BENCHMARK_PROCESS_ID getpid
#endif

static volatile uint64_t enumeration_sink;

static int path_exists(const char *filename)
{
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int remove_if_present(const char *filename)
{
    if (remove(filename) == 0 || errno == ENOENT) {
        return 1;
    }
    fprintf(stderr, "Unable to remove benchmark file: %s\n", filename);
    return 0;
}

static int elapsed_nanoseconds(
    const struct timespec *start,
    const struct timespec *end,
    double *elapsed
)
{
    if (end->tv_sec < start->tv_sec
        || (end->tv_sec == start->tv_sec && end->tv_nsec < start->tv_nsec)) {
        return 0;
    }

    *elapsed = (double)(end->tv_sec - start->tv_sec) * 1000000000.0
        + (double)(end->tv_nsec - start->tv_nsec);
    return 1;
}

static int file_size_bytes(const char *filename, long *size)
{
    FILE *file = fopen(filename, "rb");
    long file_size;
    int close_result;

    if (file == NULL) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    close_result = fclose(file);
    if (file_size < 0 || close_result != 0) {
        return 0;
    }
    *size = file_size;
    return 1;
}

static int benchmark_storage_size(
    size_t expense_count,
    size_t category_count,
    const char *filename
)
{
    const size_t repetitions = 3;
    CategoryList categories;
    ExpenseList expenses;
    CategoryList loaded_categories;
    ExpenseList loaded_expenses;
    struct timespec start;
    struct timespec end;
    double elapsed;
    long persisted_bytes;
    size_t input_bytes;
    StorageResult result;
    int success = 0;

    if (!benchmark_data_create(
            expense_count,
            category_count,
            &categories,
            &expenses
        )) {
        fprintf(stderr, "Unable to generate storage benchmark data (%zu)\n",
            expense_count);
        return 0;
    }

    input_bytes = categories.capacity * sizeof(*categories.items)
        + expenses.capacity * sizeof(*expenses.items);
    if (storage_save(filename, &categories, &expenses) != STORAGE_SUCCESS
        || !file_size_bytes(filename, &persisted_bytes)) {
        fprintf(stderr, "Unable to prepare storage benchmark file (%zu)\n",
            expense_count);
        goto cleanup_data;
    }

    if (timespec_get(&start, TIME_UTC) != TIME_UTC) {
        fprintf(stderr, "Unable to start storage benchmark timer\n");
        goto cleanup_data;
    }
    for (size_t iteration = 0; iteration < repetitions; iteration++) {
        result = storage_save(filename, &categories, &expenses);
        if (result != STORAGE_SUCCESS) {
            fprintf(stderr, "storage_save failed for %zu records (%d)\n",
                expense_count, result);
            goto cleanup_data;
        }
    }
    if (timespec_get(&end, TIME_UTC) != TIME_UTC
        || !elapsed_nanoseconds(&start, &end, &elapsed)) {
        fprintf(stderr, "Unable to finish storage save timer\n");
        goto cleanup_data;
    }
    printf("storage_save,%zu,%zu,%zu,%.3f,%.3f,%zu,%ld\n",
        expense_count,
        category_count,
        repetitions,
        elapsed / 1000000.0,
        elapsed / 1000000.0 / (double)repetitions,
        input_bytes,
        persisted_bytes);

    expense_list_init(&loaded_expenses);
    category_list_init(&loaded_categories);
    if (timespec_get(&start, TIME_UTC) != TIME_UTC) {
        fprintf(stderr, "Unable to start storage load timer\n");
        goto cleanup_loaded;
    }
    for (size_t iteration = 0; iteration < repetitions; iteration++) {
        result = storage_load(filename, &loaded_categories, &loaded_expenses);
        if (result != STORAGE_SUCCESS) {
            fprintf(stderr, "storage_load failed for %zu records (%d)\n",
                expense_count, result);
            goto cleanup_loaded;
        }
    }
    if (timespec_get(&end, TIME_UTC) != TIME_UTC
        || !elapsed_nanoseconds(&start, &end, &elapsed)) {
        fprintf(stderr, "Unable to finish storage load timer\n");
        goto cleanup_loaded;
    }
    printf("storage_load,%zu,%zu,%zu,%.3f,%.3f,%zu,%ld\n",
        expense_count,
        category_count,
        repetitions,
        elapsed / 1000000.0,
        elapsed / 1000000.0 / (double)repetitions,
        input_bytes,
        persisted_bytes);
    success = 1;

cleanup_loaded:
    category_list_destroy(&loaded_categories);
    expense_list_destroy(&loaded_expenses);
cleanup_data:
    category_list_destroy(&categories);
    expense_list_destroy(&expenses);
    {
        int cleanup_success = remove_if_present(filename);

        char temporary_filename[512];
        int length = snprintf(
            temporary_filename,
            sizeof(temporary_filename),
            "%s.tmp",
            filename
        );
        if (length < 0 || (size_t)length >= sizeof(temporary_filename)
            || !remove_if_present(temporary_filename)) {
            cleanup_success = 0;
        }
        success = success && cleanup_success;
    }
    return success;
}

static int enumerate_active_categories(
    const CategoryList *categories,
    uint64_t *result
)
{
    size_t active_count = category_active_count(categories);
    uint64_t checksum = 0;

    for (size_t index = 0; index < active_count; index++) {
        const Category *category = category_find_active_by_index(
            categories,
            index
        );
        if (category == NULL) {
            return 0;
        }
        checksum += (uint64_t)category->id;
    }
    *result = checksum;
    return 1;
}

static int benchmark_category_enumeration(size_t category_count)
{
    CategoryList categories;
    struct timespec start;
    struct timespec end;
    double elapsed;
    uint64_t checksum;
    size_t repetitions;

    if (!benchmark_categories_create(category_count, &categories)) {
        fprintf(stderr, "Unable to generate categories (%zu)\n",
            category_count);
        return 0;
    }

    if (category_count <= 100) {
        repetitions = 100;
    } else if (category_count <= 1000) {
        repetitions = 10;
    } else {
        repetitions = 1;
    }

    if (!enumerate_active_categories(&categories, &checksum)
        || timespec_get(&start, TIME_UTC) != TIME_UTC) {
        fprintf(stderr, "Unable to prepare category enumeration (%zu)\n",
            category_count);
        category_list_destroy(&categories);
        return 0;
    }
    for (size_t iteration = 0; iteration < repetitions; iteration++) {
        if (!enumerate_active_categories(&categories, &checksum)) {
            fprintf(stderr, "Category enumeration failed (%zu)\n",
                category_count);
            category_list_destroy(&categories);
            return 0;
        }
        enumeration_sink += checksum;
    }
    if (timespec_get(&end, TIME_UTC) != TIME_UTC
        || !elapsed_nanoseconds(&start, &end, &elapsed)) {
        fprintf(stderr, "Unable to finish category enumeration timer\n");
        category_list_destroy(&categories);
        return 0;
    }

    printf("category_enumeration,%zu,%zu,%.3f,%.3f\n",
        category_count,
        repetitions,
        elapsed / 1000000.0,
        elapsed / 1000000.0 / (double)repetitions);
    category_list_destroy(&categories);
    return 1;
}

int benchmark_storage_and_category_enumeration(void)
{
    static const size_t storage_sizes[] = {100, 1000, 10000};
    static const size_t category_sizes[] = {100, 1000, 10000, 100000};
    char filename[512];
    char temporary_filename[512];
    int filename_length = snprintf(
        filename,
        sizeof(filename),
        "finance_benchmark_storage_%lu.dat",
        (unsigned long)BENCHMARK_PROCESS_ID()
    );

    if (filename_length < 0 || (size_t)filename_length >= sizeof(filename)) {
        fprintf(stderr, "Unable to construct storage benchmark filename\n");
        return 0;
    }
    filename_length = snprintf(
        temporary_filename,
        sizeof(temporary_filename),
        "%s.tmp",
        filename
    );
    if (filename_length < 0
        || (size_t)filename_length >= sizeof(temporary_filename)) {
        fprintf(stderr, "Unable to construct temporary benchmark filename\n");
        return 0;
    }
    if (path_exists(filename) || path_exists(temporary_filename)) {
        fprintf(stderr, "Storage benchmark file already exists: %s\n",
            filename);
        return 0;
    }

    puts("storage_operation,expense_count,category_count,repetitions,total_elapsed_ms,mean_elapsed_ms,input_heap_bytes,file_bytes");
    for (size_t index = 0;
         index < sizeof(storage_sizes) / sizeof(storage_sizes[0]);
         index++) {
        if (!benchmark_storage_size(storage_sizes[index], 64, filename)) {
            return 0;
        }
    }
    puts("Storage at 100000 records: not run; current save validation and load duplicate checks use quadratic expense scans.");

    puts("category_enumeration,categories,repetitions,total_elapsed_ms,mean_elapsed_ms");
    for (size_t index = 0;
         index < sizeof(category_sizes) / sizeof(category_sizes[0]);
         index++) {
        if (!benchmark_category_enumeration(category_sizes[index])) {
            return 0;
        }
    }
    fprintf(stderr, "enumeration_sink=%llu\n",
        (unsigned long long)enumeration_sink);
    return 1;
}
