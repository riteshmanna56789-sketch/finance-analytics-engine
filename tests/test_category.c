#include "test_support.h"

#include <string.h>

void test_category(void)
{
    CategoryList categories;
    char long_category_name[50];

    category_list_init(&categories);
    assert(category_create(&categories, NULL) == CATEGORY_INVALID_INPUT);
    assert(category_deactivate(&categories, 1) == CATEGORY_NOT_FOUND);
    memset(long_category_name, 'X', sizeof(long_category_name) - 1);
    long_category_name[sizeof(long_category_name) - 1] = '\0';
    assert(category_create(&categories, long_category_name) == CATEGORY_SUCCESS);
    assert(category_create(&categories, "Food") == CATEGORY_SUCCESS);
    assert(category_create(&categories, "Petrol") == CATEGORY_SUCCESS);
    assert(category_create(&categories, "Rent") == CATEGORY_SUCCESS);
    assert(category_active_count(&categories) == 4);
    assert(category_list_add(&categories, categories.items[0])
        == CATEGORY_DUPLICATE);
    assert(category_deactivate(&categories, 3) == CATEGORY_SUCCESS);
    assert(category_find_by_id(&categories, 3)->is_active == 0);
    assert(category_active_count(&categories) == 3);
    assert(category_deactivate(&categories, 3) == CATEGORY_ALREADY_INACTIVE);
    assert(category_deactivate(&categories, 999) == CATEGORY_NOT_FOUND);
    assert(category_deactivate(NULL, 1) == CATEGORY_INVALID_INPUT);

    category_list_destroy(&categories);
}
