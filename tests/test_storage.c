#include "test_support.h"

#include "storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define TEST_PROCESS_ID _getpid
#else
#include <unistd.h>
#define TEST_PROCESS_ID getpid
#endif

static void write_file(const char *filename, const char *contents)
{
    FILE *file = fopen(filename, "wb");
    assert(file != NULL);
    assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    assert(fclose(file) == 0);
}

void test_storage(void)
{
    char test_file[128];
    CategoryList categories;
    CategoryList loaded_categories;
    CategoryList verify_categories;
    ExpenseList expenses;
    ExpenseList loaded_expenses;
    ExpenseList verify_expenses;
    char maximum_note[200];

    assert(snprintf(test_file, sizeof(test_file),
        "tests/storage_test_%ld.dat", (long)TEST_PROCESS_ID()) > 0);
    test_init_categories(&categories);
    category_list_init(&loaded_categories);
    category_list_init(&verify_categories);
    expense_list_init(&expenses);
    expense_list_init(&loaded_expenses);
    expense_list_init(&verify_expenses);
    assert(expense_list_add(&expenses,
        test_make_expense(4, 2, 2026, 9, 20, 1, "new"))
        == EXPENSE_SUCCESS);
    memset(maximum_note, 'N', sizeof(maximum_note) - 1);
    maximum_note[sizeof(maximum_note) - 1] = '\0';

    assert(storage_save(test_file, &categories, &expenses) == STORAGE_SUCCESS);
    assert(storage_load(
        test_file,
        &loaded_categories,
        &loaded_expenses
    ) == STORAGE_SUCCESS);
    assert(loaded_categories.items[2].is_active == 0);
    assert(loaded_expenses.size == 1 && loaded_expenses.items[0].id == 4);
    assert(loaded_expenses.next_id == 5);

    strcpy(loaded_expenses.items[0].note, maximum_note);
    assert(storage_save(
        test_file,
        &loaded_categories,
        &loaded_expenses
    ) == STORAGE_SUCCESS);
    assert(storage_load(
        test_file,
        &loaded_categories,
        &loaded_expenses
    ) == STORAGE_SUCCESS);
    assert(strlen(loaded_categories.items[0].name) == 49);
    assert(strlen(loaded_expenses.items[0].note) == 199);

    loaded_expenses.items[0].amount_paise = -1;
    assert(storage_save(test_file, &loaded_categories, &loaded_expenses)
        == STORAGE_INVALID_DATA);
    loaded_expenses.items[0].amount_paise = 1;
    assert(storage_load(
        test_file,
        &verify_categories,
        &verify_expenses
    ) == STORAGE_SUCCESS);
    assert(verify_expenses.items[0].amount_paise == 1);

    {
        const char *malformed_files[] = {
            "",
            "wrong header\n",
            "FAE_STORAGE 3\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID -1\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES -1\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY -1 1 4\nFood\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 2 30 0 0 0 1 1 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 1 1 0 0 0 1 99 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 1 1 0 0 0 -1 1 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 1 1 0 0 0 1 1 5\nabc\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFoo\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES "
                "18446744073709551615\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 2\n"
                "EXPENSE 1 2026 1 1 0 0 0 1 1 0\n\n"
                "EXPENSE 1 2026 1 2 0 0 0 1 1 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 0\nEXTRA\n"
        };

        for (size_t index = 0;
             index < sizeof(malformed_files) / sizeof(malformed_files[0]);
             index++) {
            write_file(test_file, malformed_files[index]);
            assert(storage_load(
                test_file,
                &loaded_categories,
                &loaded_expenses
            ) == STORAGE_INVALID_DATA);
            assert(loaded_categories.size == categories.size);
            assert(loaded_categories.items[0].id == categories.items[0].id);
            assert(strcmp(
                loaded_categories.items[0].name,
                categories.items[0].name
            ) == 0);
            assert(loaded_categories.items[2].id == categories.items[2].id);
            assert(strcmp(
                loaded_categories.items[2].name,
                categories.items[2].name
            ) == 0);
            assert(loaded_categories.items[2].is_active == 0);
            assert(loaded_expenses.size == 1
                && loaded_expenses.items[0].id == 4);
            assert(loaded_expenses.items[0].timestamp.year == 2026);
            assert(loaded_expenses.items[0].timestamp.month == 9);
            assert(loaded_expenses.items[0].timestamp.day == 20);
            assert(loaded_expenses.items[0].amount_paise == 1);
            assert(loaded_expenses.items[0].category_id == 2);
            assert(strcmp(loaded_expenses.items[0].note, maximum_note) == 0);
            assert(loaded_expenses.next_id == 5);
        }
    }

    write_file(
        test_file,
        "FAE_STORAGE 2\nNEXT_EXPENSE_ID 1\nCATEGORIES 0\nEXPENSES 0"
    );
    assert(storage_load(test_file, &loaded_categories, &loaded_expenses)
        == STORAGE_SUCCESS);
    assert(loaded_categories.size == 0 && loaded_expenses.size == 0);

    assert(remove(test_file) == 0);
    {
        char temporary_file[128];
        assert(snprintf(
            temporary_file,
            sizeof(temporary_file),
            "%s.tmp",
            test_file
        ) > 0);
        remove(temporary_file);
    }

    expense_list_destroy(&verify_expenses);
    expense_list_destroy(&loaded_expenses);
    expense_list_destroy(&expenses);
    category_list_destroy(&verify_categories);
    category_list_destroy(&loaded_categories);
    category_list_destroy(&categories);
}

void test_storage_failure_paths(void)
{
    char missing_file[128];
    CategoryList categories;
    ExpenseList expenses;
    int remove_result;

    assert(snprintf(missing_file, sizeof(missing_file),
        "tests/storage_missing_%ld.dat", (long)TEST_PROCESS_ID()) > 0);
    errno = 0;
    remove_result = remove(missing_file);
    assert(remove_result == 0 || errno == ENOENT);

    test_init_categories(&categories);
    expense_list_init(&expenses);
    assert(expense_list_add(&expenses,
        test_make_expense(14, 2, 2026, 4, 5, 650, "preserve caller state"))
        == EXPENSE_SUCCESS);

    assert(storage_load(missing_file, &categories, &expenses)
        == STORAGE_NOT_FOUND);
    assert(categories.size == 4);
    assert(categories.items[0].id == 1);
    assert(strlen(categories.items[0].name) == 49);
    assert(categories.items[0].name[0] == 'X');
    assert(categories.items[0].name[48] == 'X');
    assert(categories.items[0].is_active == 1);
    assert(categories.items[1].id == 2);
    assert(strcmp(categories.items[1].name, "Food") == 0);
    assert(categories.items[2].id == 3);
    assert(strcmp(categories.items[2].name, "Petrol") == 0);
    assert(categories.items[2].is_active == 0);
    assert(categories.items[3].id == 4);
    assert(strcmp(categories.items[3].name, "Rent") == 0);
    assert(expenses.size == 1);
    assert(expenses.items[0].id == 14);
    assert(expenses.items[0].timestamp.year == 2026);
    assert(expenses.items[0].timestamp.month == 4);
    assert(expenses.items[0].timestamp.day == 5);
    assert(expenses.items[0].amount_paise == 650);
    assert(expenses.items[0].category_id == 2);
    assert(strcmp(expenses.items[0].note, "preserve caller state") == 0);
    assert(expenses.next_id == 15);

    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
}

void test_storage_v1_compatibility(void)
{
    char test_file[128];
    CategoryList categories;
    ExpenseList expenses;
    const char *version_one_fixture =
        "FAE_STORAGE 1\n"
        "CATEGORIES 2\n"
        "CATEGORY 2 0 4\n"
        "Food\n"
        "CATEGORY 7 1 5\n"
        "Other\n"
        "EXPENSES 2\n"
        "EXPENSE 3 2024 2 29 12 34 56 12345 2 11\n"
        "Legacy meal\n"
        "EXPENSE 8 2025 12 31 23 59 59 250 7 4\n"
        "Taxi\n";

    assert(snprintf(test_file, sizeof(test_file),
        "tests/storage_v1_test_%ld.dat", (long)TEST_PROCESS_ID()) > 0);
    category_list_init(&categories);
    expense_list_init(&expenses);
    write_file(test_file, version_one_fixture);

    assert(storage_load(test_file, &categories, &expenses) == STORAGE_SUCCESS);
    assert(categories.size == 2);
    assert(categories.items[0].id == 2);
    assert(strcmp(categories.items[0].name, "Food") == 0);
    assert(categories.items[0].is_active == 0);
    assert(categories.items[1].id == 7);
    assert(strcmp(categories.items[1].name, "Other") == 0);
    assert(categories.items[1].is_active == 1);
    assert(expenses.size == 2);
    assert(expenses.items[0].id == 3);
    assert(expenses.items[0].timestamp.year == 2024);
    assert(expenses.items[0].timestamp.month == 2);
    assert(expenses.items[0].timestamp.day == 29);
    assert(expenses.items[0].timestamp.hour == 12);
    assert(expenses.items[0].timestamp.minute == 34);
    assert(expenses.items[0].timestamp.second == 56);
    assert(expenses.items[0].amount_paise == 12345);
    assert(expenses.items[0].category_id == 2);
    assert(strcmp(expenses.items[0].note, "Legacy meal") == 0);
    assert(expenses.items[1].id == 8);
    assert(expenses.items[1].timestamp.year == 2025);
    assert(expenses.items[1].timestamp.month == 12);
    assert(expenses.items[1].timestamp.day == 31);
    assert(expenses.items[1].timestamp.hour == 23);
    assert(expenses.items[1].timestamp.minute == 59);
    assert(expenses.items[1].timestamp.second == 59);
    assert(expenses.items[1].amount_paise == 250);
    assert(expenses.items[1].category_id == 7);
    assert(strcmp(expenses.items[1].note, "Taxi") == 0);
    assert(expenses.next_id == 9);

    assert(remove(test_file) == 0);
    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
}
