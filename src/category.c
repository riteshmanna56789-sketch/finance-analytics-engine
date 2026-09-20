#include "category.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

static const size_t INITIAL_CAPACITY = 4;

static int category_name_is_blank(const char *name)
{
    while (*name != '\0') {
        if (!isspace((unsigned char)*name)) {
            return 0;
        }
        name++;
    }

    return 1;
}

static int category_name_exists(
    const CategoryList *list,
    const char *name
)
{
    for (size_t index = 0; index < list->size; index++) {
        if (strcmp(list->items[index].name, name) == 0) {
            return 1;
        }
    }

    return 0;
}

static CategoryResult category_next_id(
    const CategoryList *list,
    int *next_id
)
{
    int highest_id = 0;

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id > highest_id) {
            highest_id = list->items[index].id;
        }
    }

    if (highest_id == INT_MAX) {
        return CATEGORY_INVALID_INPUT;
    }

    *next_id = highest_id + 1;
    return CATEGORY_SUCCESS;
}

void category_list_init(CategoryList *list)
{
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}

CategoryResult category_list_add(CategoryList *list, Category category)
{
    if (list == NULL) {
        return CATEGORY_INVALID_INPUT;
    }

    if (list->size == list->capacity) {
        size_t new_capacity = list->capacity == 0
            ? INITIAL_CAPACITY
            : list->capacity * 2;
        Category *resized_items = realloc(
            list->items,
            new_capacity * sizeof(*resized_items)
        );

        if (resized_items == NULL) {
            return CATEGORY_MEMORY_ERROR;
        }

        list->items = resized_items;
        list->capacity = new_capacity;
    }

    list->items[list->size] = category;
    list->size++;

    return CATEGORY_SUCCESS;
}

CategoryResult category_create(CategoryList *list, const char *name)
{
    size_t name_length;
    int next_id;
    Category category;
    CategoryResult result;

    if (list == NULL || name == NULL) {
        return CATEGORY_INVALID_INPUT;
    }

    name_length = strlen(name);
    if (name_length == 0
        || name_length >= sizeof(category.name)
        || category_name_is_blank(name)) {
        return CATEGORY_INVALID_INPUT;
    }

    if (category_name_exists(list, name)) {
        return CATEGORY_DUPLICATE;
    }

    result = category_next_id(list, &next_id);
    if (result != CATEGORY_SUCCESS) {
        return result;
    }

    category.id = next_id;
    category.is_active = 1;
    memcpy(category.name, name, name_length + 1);

    return category_list_add(list, category);
}

CategoryResult category_deactivate(CategoryList *list, int category_id)
{
    Category *category = NULL;

    if (list == NULL || category_id < 1) {
        return CATEGORY_INVALID_INPUT;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id == category_id) {
            category = &list->items[index];
            break;
        }
    }

    if (category == NULL) {
        return CATEGORY_NOT_FOUND;
    }

    if (!category->is_active) {
        return CATEGORY_ALREADY_INACTIVE;
    }

    category->is_active = 0;
    return CATEGORY_SUCCESS;
}

const Category *category_find_by_id(const CategoryList *list, int id)
{
    if (list == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id == id) {
            return &list->items[index];
        }
    }

    return NULL;
}

size_t category_active_count(const CategoryList *list)
{
    size_t active_count = 0;

    if (list == NULL) {
        return 0;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].is_active) {
            active_count++;
        }
    }

    return active_count;
}

const Category *category_find_active_by_index(
    const CategoryList *list,
    size_t index
)
{
    size_t active_index = 0;

    if (list == NULL) {
        return NULL;
    }

    for (size_t item_index = 0; item_index < list->size; item_index++) {
        if (list->items[item_index].is_active) {
            if (active_index == index) {
                return &list->items[item_index];
            }
            active_index++;
        }
    }

    return NULL;
}

void category_list_destroy(CategoryList *list)
{
    if (list == NULL) {
        return;
    }

    free(list->items);
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}