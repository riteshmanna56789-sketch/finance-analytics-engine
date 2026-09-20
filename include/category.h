#ifndef CATEGORY_H
#define CATEGORY_H

#include <stddef.h>

typedef struct {
    int id;
    char name[50];
    int is_active;
} Category;

typedef struct {
    Category *items;
    size_t size;
    size_t capacity;
} CategoryList;

typedef enum {
    CATEGORY_SUCCESS = 0,
    CATEGORY_MEMORY_ERROR,
    CATEGORY_INVALID_INPUT,
    CATEGORY_DUPLICATE,
    CATEGORY_NOT_FOUND,
    CATEGORY_ALREADY_INACTIVE
} CategoryResult;

void category_list_init(CategoryList *list);
CategoryResult category_list_add(CategoryList *list, Category category);
CategoryResult category_create(CategoryList *list, const char *name);
CategoryResult category_deactivate(CategoryList *list, int category_id);
const Category *category_find_by_id(const CategoryList *list, int id);
size_t category_active_count(const CategoryList *list);
const Category *category_find_active_by_index(
    const CategoryList *list,
    size_t index
);
void category_list_destroy(CategoryList *list);

#endif