#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Data.h"
#include "List.h"
#include "Log.h"
#include "Trie.h"

typedef void (*TestCaseFn)(void);

typedef struct TestCase {
  const char* name;
  TestCaseFn run;
} TestCase;

static const char* SAVE_FILE = "core_smoke_roundtrip.json";
static const char* NEW_FILE = "core_smoke_new.json";
static const char* INVALID_FILE = "core_smoke_invalid.json";

static void fail_impl(const char* expr, const char* file, int line,
                      const char* detail) {
  fprintf(stderr, "[FAIL] %s:%d: %s", file, line, expr);
  if (detail && detail[0] != '\0') {
    fprintf(stderr, " | %s", detail);
  }
  fputc('\n', stderr);
  remove(SAVE_FILE);
  remove(NEW_FILE);
  remove(INVALID_FILE);
  exit(EXIT_FAILURE);
}

#define EXPECT_TRUE(expr)                                                     \
  do {                                                                        \
    if (!(expr)) {                                                            \
      fail_impl(#expr, __FILE__, __LINE__, "condition is false");             \
    }                                                                         \
  } while (0)

#define EXPECT_STATUS(actual, expected)                                       \
  do {                                                                        \
    DataStatus actual_value__ = (actual);                                     \
    DataStatus expected_value__ = (expected);                                 \
    if (actual_value__ != expected_value__) {                                 \
      char detail__[128];                                                     \
      snprintf(detail__, sizeof(detail__), "actual=%d expected=%d",           \
               actual_value__, expected_value__);                             \
      fail_impl(#actual, __FILE__, __LINE__, detail__);                       \
    }                                                                         \
  } while (0)

#define EXPECT_STRING(actual, expected)                                       \
  do {                                                                        \
    const char* actual_value__ = (actual);                                    \
    const char* expected_value__ = (expected);                                \
    if (strcmp(actual_value__, expected_value__) != 0) {                      \
      char detail__[256];                                                     \
      snprintf(detail__, sizeof(detail__), "actual='%s' expected='%s'",       \
               actual_value__, expected_value__);                             \
      fail_impl(#actual, __FILE__, __LINE__, detail__);                       \
    }                                                                         \
  } while (0)

static void reset_environment(void) {
  log_init();
  data_init();
  remove(SAVE_FILE);
  remove(NEW_FILE);
  remove(INVALID_FILE);
}

static void write_text_file(const char* filename, const char* content) {
  FILE* file = fopen(filename, "wb");
  EXPECT_TRUE(file != NULL);
  fputs(content, file);
  fclose(file);
}

static const DataNode* require_record(const char* id) {
  const DataNode* node = data_find_by_id(id);
  EXPECT_TRUE(node != NULL);
  return node;
}

static void expect_record(const char* id, const char* name, const char* value) {
  const DataNode* node = require_record(id);
  EXPECT_STRING(node->id, id);
  EXPECT_STRING(node->name, name);
  EXPECT_STRING(node->value, value);
}

static int result_contains_id(DataNode** results, int count, const char* id) {
  for (int i = 0; i < count; ++i) {
    if (strcmp(results[i]->id, id) == 0) {
      return 1;
    }
  }
  return 0;
}

static void expect_prefix_ids(const char* prefix, const char* const* ids,
                              int expected_count) {
  int count = 0;
  DataNode** results = trie_search(prefix, &count);
  if (count != expected_count) {
    char detail[128];
    snprintf(detail, sizeof(detail), "prefix='%s' actual=%d expected=%d",
             prefix, count, expected_count);
    fail_impl("trie_search count", __FILE__, __LINE__, detail);
  }
  for (int i = 0; i < expected_count; ++i) {
    if (!result_contains_id(results, count, ids[i])) {
      char detail[128];
      snprintf(detail, sizeof(detail), "prefix='%s' missing id='%s'", prefix,
               ids[i]);
      fail_impl("trie_search result_contains_id", __FILE__, __LINE__, detail);
    }
  }
  free(results);
}

static void seed_featured_records(void) {
  EXPECT_STATUS(data_insert("陈晨", "CS26A01", "算法设计 91", 1), DATA_OK);
  EXPECT_STATUS(data_insert("陈辰", "CS26A02", "数据结构 88", 1), DATA_OK);
  EXPECT_STATUS(data_insert("陈晨曦", "CS26A03", "编译原理 95", 1), DATA_OK);
  EXPECT_STATUS(data_insert("Alice", "SE24C21", "Software Engineering 94", 1),
                DATA_OK);
  EXPECT_STATUS(data_insert("Alicia", "SE24C22", "Computer Networks 87", 1),
                DATA_OK);
}

static void test_hash_and_list_order(void) {
  DataNode* head;
  DataNode* tail;

  reset_environment();
  seed_featured_records();

  expect_record("CS26A01", "陈晨", "算法设计 91");
  expect_record("SE24C22", "Alicia", "Computer Networks 87");
  EXPECT_STATUS(data_insert("重复用户", "CS26A01", "Dup", 1), DATA_DUPLICATE_ID);

  head = list_head();
  tail = list_tail();
  EXPECT_TRUE(head != NULL);
  EXPECT_TRUE(tail != NULL);
  EXPECT_STRING(head->id, "SE24C22");
  EXPECT_STRING(head->name, "Alicia");
  EXPECT_STRING(tail->id, "CS26A01");
  EXPECT_STRING(tail->name, "陈晨");
  EXPECT_TRUE(head->next != NULL);
  EXPECT_STRING(head->next->id, "SE24C21");
}

static void test_trie_prefix_search(void) {
  static const char* chen_ids[] = {"CS26A01", "CS26A02", "CS26A03"};
  static const char* chenchen_ids[] = {"CS26A01", "CS26A03"};
  static const char* ali_ids[] = {"SE24C21", "SE24C22"};

  reset_environment();
  seed_featured_records();

  expect_prefix_ids("陈", chen_ids, 3);
  expect_prefix_ids("陈晨", chenchen_ids, 2);
  expect_prefix_ids("Ali", ali_ids, 2);
  expect_prefix_ids("Alic", ali_ids, 2);

  {
    int count = 0;
    DataNode** results = trie_search("不存在", &count);
    EXPECT_TRUE(results == NULL);
    EXPECT_TRUE(count == 0);
  }
}

static void test_modify_delete_and_undo(void) {
  reset_environment();
  seed_featured_records();

  EXPECT_STATUS(data_modify("CS26A02", "数据结构 90", 1), DATA_OK);
  expect_record("CS26A02", "陈辰", "数据结构 90");
  EXPECT_STATUS(data_undo(), DATA_OK);
  expect_record("CS26A02", "陈辰", "数据结构 88");

  EXPECT_STATUS(data_delete("SE24C21", 1), DATA_OK);
  EXPECT_TRUE(data_find_by_id("SE24C21") == NULL);
  EXPECT_STATUS(data_undo(), DATA_OK);
  expect_record("SE24C21", "Alice", "Software Engineering 94");

  EXPECT_STATUS(data_delete("NO_SUCH_ID", 1), DATA_NOT_FOUND);
}

static void test_save_load_and_dirty_guards(void) {
  reset_environment();
  seed_featured_records();

  EXPECT_STATUS(data_save(SAVE_FILE), DATA_OK);
  EXPECT_TRUE(data_is_modified() == 0);

  EXPECT_STATUS(data_modify("CS26A01", "算法设计 99", 1), DATA_OK);
  EXPECT_TRUE(data_is_modified() == 1);
  EXPECT_STATUS(data_load(SAVE_FILE), DATA_DIRTY_BLOCKED);
  EXPECT_STATUS(data_new(NEW_FILE), DATA_DIRTY_BLOCKED);

  EXPECT_STATUS(data_save(SAVE_FILE), DATA_OK);
  EXPECT_STATUS(data_load(SAVE_FILE), DATA_OK);
  expect_record("CS26A01", "陈晨", "算法设计 99");
  EXPECT_TRUE(data_is_modified() == 0);
}

static void test_file_validation_and_new_file(void) {
  reset_environment();
  seed_featured_records();

  EXPECT_STATUS(data_save(SAVE_FILE), DATA_OK);
  write_text_file(INVALID_FILE, "{ \"broken\": true }");
  EXPECT_STATUS(data_load(INVALID_FILE), DATA_FORMAT_ERROR);
  expect_record("CS26A03", "陈晨曦", "编译原理 95");

  EXPECT_STATUS(data_new(NEW_FILE), DATA_OK);
  EXPECT_TRUE(data_find_by_id("CS26A01") == NULL);
  EXPECT_TRUE(list_head() == NULL);
  EXPECT_TRUE(list_tail() == NULL);

  EXPECT_STATUS(data_load(SAVE_FILE), DATA_OK);
  expect_record("SE24C22", "Alicia", "Computer Networks 87");
}

static void test_long_name_delete_safety(void) {
  char long_name[401];

  reset_environment();
  memset(long_name, 'Z', sizeof(long_name) - 1);
  long_name[sizeof(long_name) - 1] = '\0';

  EXPECT_STATUS(data_insert(long_name, "LONG400", "Long Record", 1), DATA_OK);
  expect_record("LONG400", long_name, "Long Record");
  EXPECT_STATUS(data_delete("LONG400", 1), DATA_OK);
  EXPECT_TRUE(data_find_by_id("LONG400") == NULL);
  EXPECT_STATUS(data_undo(), DATA_OK);
  expect_record("LONG400", long_name, "Long Record");
}

int main(void) {
  static const TestCase tests[] = {
      {"hash_and_list_order", test_hash_and_list_order},
      {"trie_prefix_search", test_trie_prefix_search},
      {"modify_delete_and_undo", test_modify_delete_and_undo},
      {"save_load_and_dirty_guards", test_save_load_and_dirty_guards},
      {"file_validation_and_new_file", test_file_validation_and_new_file},
      {"long_name_delete_safety", test_long_name_delete_safety},
  };

  size_t count = sizeof(tests) / sizeof(tests[0]);

  for (size_t i = 0; i < count; ++i) {
    tests[i].run();
    printf("[PASS] %s\n", tests[i].name);
  }

  remove(SAVE_FILE);
  remove(NEW_FILE);
  remove(INVALID_FILE);
  puts("core smoke test passed");
  return 0;
}
