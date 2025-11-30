#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

#define POINT_TYPE_ID 1
#define RECT_TYPE_ID 2

typedef struct {
    double x;
    double y;
} point_t;

typedef struct {
    double x;
    double y;
    double width;
    double height;
} rect_t;

/* #point [x y] */
static edn_value_t* point_reader(edn_value_t* value, edn_arena_t* arena,
                                 const char** error_message) {
    if (edn_type(value) != EDN_TYPE_VECTOR) {
        *error_message = "#point requires vector [x y]";
        return NULL;
    }

    if (edn_vector_count(value) != 2) {
        *error_message = "#point requires exactly 2 elements";
        return NULL;
    }

    edn_value_t* x_val = edn_vector_get(value, 0);
    edn_value_t* y_val = edn_vector_get(value, 1);

    double x, y;
    if (!edn_number_as_double(x_val, &x) || !edn_number_as_double(y_val, &y)) {
        *error_message = "#point elements must be numbers";
        return NULL;
    }

    point_t* point = edn_arena_alloc(arena, sizeof(point_t));
    if (!point) {
        *error_message = "Out of memory";
        return NULL;
    }

    point->x = x;
    point->y = y;

    return edn_external_create(arena, point, POINT_TYPE_ID);
}

/* #rect {:x :y :width :height} */
static edn_value_t* rect_reader(edn_value_t* value, edn_arena_t* arena,
                                const char** error_message) {
    if (edn_type(value) != EDN_TYPE_MAP) {
        *error_message = "#rect requires map";
        return NULL;
    }

    edn_value_t* x_val = edn_map_get_keyword(value, "x");
    edn_value_t* y_val = edn_map_get_keyword(value, "y");
    edn_value_t* w_val = edn_map_get_keyword(value, "width");
    edn_value_t* h_val = edn_map_get_keyword(value, "height");

    if (!x_val || !y_val || !w_val || !h_val) {
        *error_message = "#rect requires :x :y :width :height keys";
        return NULL;
    }

    double x, y, width, height;
    if (!edn_number_as_double(x_val, &x) || !edn_number_as_double(y_val, &y) ||
        !edn_number_as_double(w_val, &width) || !edn_number_as_double(h_val, &height)) {
        *error_message = "#rect values must be numbers";
        return NULL;
    }

    rect_t* rect = edn_arena_alloc(arena, sizeof(rect_t));
    if (!rect) {
        *error_message = "Out of memory";
        return NULL;
    }

    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;

    return edn_external_create(arena, rect, RECT_TYPE_ID);
}

/* Test basic external value creation and retrieval */
TEST(external_create_and_get) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_read_with_options("#point [3.5 4.5]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_EXTERNAL);

    void* data;
    uint32_t type_id;
    assert(edn_external_get(result.value, &data, &type_id));
    assert(type_id == POINT_TYPE_ID);
    assert(data != NULL);

    point_t* point = (point_t*) data;
    assert(point->x == 3.5);
    assert(point->y == 4.5);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test external_is_type helper */
TEST(external_is_type) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_read_with_options("#point [1 2]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(edn_external_is_type(result.value, POINT_TYPE_ID) == true);
    assert(edn_external_is_type(result.value, RECT_TYPE_ID) == false);
    assert(edn_external_is_type(result.value, 999) == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test external value in collection */
TEST(external_in_collection) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_read_with_options("[#point [0 0] #point [10 20]]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    edn_value_t* p1 = edn_vector_get(result.value, 0);
    edn_value_t* p2 = edn_vector_get(result.value, 1);

    assert(edn_external_is_type(p1, POINT_TYPE_ID));
    assert(edn_external_is_type(p2, POINT_TYPE_ID));

    void* data1;
    void* data2;
    edn_external_get(p1, &data1, NULL);
    edn_external_get(p2, &data2, NULL);

    point_t* point1 = (point_t*) data1;
    point_t* point2 = (point_t*) data2;

    assert(point1->x == 0.0 && point1->y == 0.0);
    assert(point2->x == 10.0 && point2->y == 20.0);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test multiple external types */
TEST(multiple_external_types) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);
    edn_reader_register(registry, "rect", rect_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result =
        edn_read_with_options("{:origin #point [0 0] :bounds #rect {:x 0 :y 0 :width 100 :height "
                              "200}}",
                              0, &opts);

    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_value_t* origin = edn_map_get_keyword(result.value, "origin");
    edn_value_t* bounds = edn_map_get_keyword(result.value, "bounds");

    assert(edn_external_is_type(origin, POINT_TYPE_ID));
    assert(edn_external_is_type(bounds, RECT_TYPE_ID));

    void* origin_data;
    void* bounds_data;
    edn_external_get(origin, &origin_data, NULL);
    edn_external_get(bounds, &bounds_data, NULL);

    point_t* pt = (point_t*) origin_data;
    rect_t* rect = (rect_t*) bounds_data;

    assert(pt->x == 0.0 && pt->y == 0.0);
    assert(rect->x == 0.0 && rect->y == 0.0);
    assert(rect->width == 100.0 && rect->height == 200.0);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test external_get with NULL outputs */
TEST(external_get_null_outputs) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_read_with_options("#point [5 6]", 0, &opts);

    assert(result.error == EDN_OK);

    uint32_t type_id;
    assert(edn_external_get(result.value, NULL, &type_id));
    assert(type_id == POINT_TYPE_ID);

    void* data;
    assert(edn_external_get(result.value, &data, NULL));
    assert(data != NULL);

    assert(edn_external_get(result.value, NULL, NULL));

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test external_get on non-external type */
TEST(external_get_wrong_type) {
    edn_result_t result = edn_read("42", 0);

    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    void* data;
    uint32_t type_id;
    assert(edn_external_get(result.value, &data, &type_id) == false);
    assert(edn_external_is_type(result.value, POINT_TYPE_ID) == false);

    edn_free(result.value);
}

/* Test external_get on NULL */
TEST(external_get_null) {
    void* data;
    uint32_t type_id;
    assert(edn_external_get(NULL, &data, &type_id) == false);
    assert(edn_external_is_type(NULL, POINT_TYPE_ID) == false);
}

/* Test reader error handling */
TEST(external_reader_error) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Wrong type - not a vector */
    edn_result_t result = edn_read_with_options("#point 42", 0, &opts);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
    assert(strstr(result.error_message, "vector") != NULL);

    /* Wrong element count */
    result = edn_read_with_options("#point [1 2 3]", 0, &opts);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);

    /* Wrong element types */
    result = edn_read_with_options("#point [:x :y]", 0, &opts);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);

    edn_reader_registry_destroy(registry);
}

/* Test that external values work with integer coordinates */
TEST(external_integer_coords) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_read_with_options("#point [100 200]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_EXTERNAL);

    void* data;
    edn_external_get(result.value, &data, NULL);
    point_t* point = (point_t*) data;

    assert(point->x == 100.0);
    assert(point->y == 200.0);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

static bool point_equal(const void* a, const void* b) {
    const point_t* pa = (const point_t*) a;
    const point_t* pb = (const point_t*) b;
    return pa->x == pb->x && pa->y == pb->y;
}

static uint64_t point_hash(const void* data) {
    const point_t* p = (const point_t*) data;
    union {
        double d;
        uint64_t u;
    } xu, yu;
    xu.d = p->x;
    yu.d = p->y;
    return xu.u ^ (yu.u * 31);
}

/* Test equality with registered equal function */
TEST(external_equality_registered) {
    assert(edn_external_register_type(POINT_TYPE_ID, point_equal, point_hash));

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result1 = edn_read_with_options("#point [3 4]", 0, &opts);
    edn_result_t result2 = edn_read_with_options("#point [3 4]", 0, &opts);

    assert(result1.error == EDN_OK);
    assert(result2.error == EDN_OK);

    assert(edn_value_equal(result1.value, result2.value) == true);

    edn_result_t result3 = edn_read_with_options("#point [5 6]", 0, &opts);
    assert(result3.error == EDN_OK);

    assert(edn_value_equal(result1.value, result3.value) == false);

    edn_free(result1.value);
    edn_free(result2.value);
    edn_free(result3.value);
    edn_reader_registry_destroy(registry);
    edn_external_unregister_type(POINT_TYPE_ID);
}

/* Test equality without registered function (pointer equality) */
TEST(external_equality_pointer_fallback) {
    /* Make sure no equality function is registered */
    edn_external_unregister_type(POINT_TYPE_ID);

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Parse two points with same coordinates */
    edn_result_t result1 = edn_read_with_options("#point [3 4]", 0, &opts);
    edn_result_t result2 = edn_read_with_options("#point [3 4]", 0, &opts);

    assert(result1.error == EDN_OK);
    assert(result2.error == EDN_OK);

    /* Without registered equality, they should NOT be equal (different pointers) */
    assert(edn_value_equal(result1.value, result2.value) == false);

    /* Same value should equal itself */
    assert(edn_value_equal(result1.value, result1.value) == true);

    edn_free(result1.value);
    edn_free(result2.value);
    edn_reader_registry_destroy(registry);
}

/* Test equality with different type_ids */
TEST(external_equality_different_types) {
    edn_external_register_type(POINT_TYPE_ID, point_equal, point_hash);

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);
    edn_reader_register(registry, "rect", rect_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t point_result = edn_read_with_options("#point [0 0]", 0, &opts);
    edn_result_t rect_result =
        edn_read_with_options("#rect {:x 0 :y 0 :width 10 :height 10}", 0, &opts);

    assert(point_result.error == EDN_OK);
    assert(rect_result.error == EDN_OK);

    /* Different type_ids should never be equal */
    assert(edn_value_equal(point_result.value, rect_result.value) == false);

    edn_free(point_result.value);
    edn_free(rect_result.value);
    edn_reader_registry_destroy(registry);
    edn_external_unregister_type(POINT_TYPE_ID);
}

/* Test hash with registered hash function */
TEST(external_hash_registered) {
    edn_external_register_type(POINT_TYPE_ID, point_equal, point_hash);

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Parse two identical points */
    edn_result_t result1 = edn_read_with_options("#point [3 4]", 0, &opts);
    edn_result_t result2 = edn_read_with_options("#point [3 4]", 0, &opts);

    assert(result1.error == EDN_OK);
    assert(result2.error == EDN_OK);

    /* Equal values should have same hash */
    uint64_t hash1 = edn_value_hash(result1.value);
    uint64_t hash2 = edn_value_hash(result2.value);
    assert(hash1 == hash2);

    edn_result_t result3 = edn_read_with_options("#point [5 6]", 0, &opts);
    assert(result3.error == EDN_OK);
    uint64_t hash3 = edn_value_hash(result3.value);
    assert(hash1 != hash3);

    edn_free(result1.value);
    edn_free(result2.value);
    edn_free(result3.value);
    edn_reader_registry_destroy(registry);
    edn_external_unregister_type(POINT_TYPE_ID);
}

/* Test external values in set (requires equality for duplicate detection) */
TEST(external_in_set_with_equality) {
    edn_external_register_type(POINT_TYPE_ID, point_equal, point_hash);

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "point", point_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_read_with_options("#{#point [1 2] #point [3 4]}", 0, &opts);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 2);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
    edn_external_unregister_type(POINT_TYPE_ID);
}

TEST(external_register_null_equal) {
    assert(edn_external_register_type(999, NULL, NULL) == false);
}

/* Test re-registering updates the functions */
TEST(external_register_update) {
    /* Register with one function */
    assert(edn_external_register_type(POINT_TYPE_ID, point_equal, NULL));

    /* Re-register with hash function added */
    assert(edn_external_register_type(POINT_TYPE_ID, point_equal, point_hash));

    /* Clean up */
    edn_external_unregister_type(POINT_TYPE_ID);
}

int main(void) {
    printf("Running external value tests...\n\n");

    RUN_TEST(external_create_and_get);
    RUN_TEST(external_is_type);
    RUN_TEST(external_in_collection);
    RUN_TEST(multiple_external_types);
    RUN_TEST(external_get_null_outputs);
    RUN_TEST(external_get_wrong_type);
    RUN_TEST(external_get_null);
    RUN_TEST(external_reader_error);
    RUN_TEST(external_integer_coords);

    printf("\nEquality tests:\n");
    RUN_TEST(external_equality_registered);
    RUN_TEST(external_equality_pointer_fallback);
    RUN_TEST(external_equality_different_types);
    RUN_TEST(external_hash_registered);
    RUN_TEST(external_in_set_with_equality);
    RUN_TEST(external_register_null_equal);
    RUN_TEST(external_register_update);

    TEST_SUMMARY("external value");
}
