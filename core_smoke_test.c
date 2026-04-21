#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Auth.h"
#include "Data.h"
#include "List.h"
#include "Log.h"
#include "Trie.h"
#include "memory.h"

/* 每个测试用例都采用“无参、无返回值”的统一函数签名。 */
typedef void (*TestCaseFn)(void);

/* TestCase 把测试名称和执行函数绑定在一起。 */
typedef struct TestCase {
  /* 测试名称，用于输出通过信息。 */
  const char* name;
  /* 实际执行测试逻辑的函数。 */
  TestCaseFn run;
} TestCase;

/* 保存回环测试使用的临时文件名。 */
static const char* SAVE_FILE = "core_smoke_roundtrip.dat";
/* 新建文件测试使用的临时文件名。 */
static const char* NEW_FILE = "core_smoke_new.dat";
/* 非法文件格式测试使用的临时文件名。 */
static const char* INVALID_FILE = "core_smoke_invalid.dat";
/* 篡改 flag 的临时文件名。 */
static const char* FLAG_TAMPER_FILE = "core_smoke_flag_tamper.dat";
/* 篡改 payload 的临时文件名。 */
static const char* PAYLOAD_TAMPER_FILE = "core_smoke_payload_tamper.dat";

/* smoke test 统一使用这一组演示密码。 */
static const char* PASSWORD = "demo123";
/* 错误密码场景使用另一组口令。 */
static const char* WRONG_PASSWORD = "wrong-password";

/* 下面这些常量与 Auth 模块约定的文件头布局保持一致。 */
#define AUTH_MAGIC_SIZE_TEST 8
#define AUTH_VERSION_SIZE_TEST 4
#define AUTH_SALT_SIZE_TEST 16
#define AUTH_NONCE_SIZE_TEST 24
#define AUTH_FLAG_SIZE_OFFSET_TEST                                              \
  (AUTH_MAGIC_SIZE_TEST + AUTH_VERSION_SIZE_TEST + AUTH_SALT_SIZE_TEST +        \
   AUTH_NONCE_SIZE_TEST + AUTH_NONCE_SIZE_TEST)
#define AUTH_PAYLOAD_SIZE_OFFSET_TEST (AUTH_FLAG_SIZE_OFFSET_TEST + 8)
#define AUTH_HEADER_SIZE_TEST (AUTH_PAYLOAD_SIZE_OFFSET_TEST + 8)

static void fail_impl(const char* expr, const char* file, int line,
                      const char* detail) {
  /* 在标准错误输出失败表达式和位置信息。 */
  fprintf(stderr, "[FAIL] %s:%d: %s", file, line, expr);

  /* detail 非空时，把更细的失败原因追加到后面。 */
  if (detail && detail[0] != '\0') {
    fprintf(stderr, " | %s", detail);
  }

  /* 补一个换行，方便日志阅读。 */
  fputc('\n', stderr);

  /* 失败时清理测试过程中产生的临时文件。 */
  remove(SAVE_FILE);
  remove(NEW_FILE);
  remove(INVALID_FILE);
  remove(FLAG_TAMPER_FILE);
  remove(PAYLOAD_TAMPER_FILE);

  /* 立即以失败状态退出整个测试程序。 */
  exit(EXIT_FAILURE);
}

/* 断言表达式为真。 */
#define EXPECT_TRUE(expr)                                                     \
  do {                                                                        \
    if (!(expr)) {                                                            \
      fail_impl(#expr, __FILE__, __LINE__, "condition is false");             \
    }                                                                         \
  } while (0)

/* 断言实际状态码与预期状态码一致。 */
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

/* 断言实际认证状态码与预期状态码一致。 */
#define EXPECT_AUTH_STATUS(actual, expected)                                  \
  do {                                                                        \
    AuthStatus actual_value__ = (actual);                                     \
    AuthStatus expected_value__ = (expected);                                 \
    if (actual_value__ != expected_value__) {                                 \
      char detail__[128];                                                     \
      snprintf(detail__, sizeof(detail__), "actual=%d expected=%d",           \
               actual_value__, expected_value__);                             \
      fail_impl(#actual, __FILE__, __LINE__, detail__);                       \
    }                                                                         \
  } while (0)

/* 断言两个字符串内容完全一致。 */
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
  /* 每个测试开始前都重置日志系统。 */
  log_init();
  /* 重置数据层，清空所有内存结构。 */
  data_init();
  /* 清理上一次测试残留的文件。 */
  remove(SAVE_FILE);
  remove(NEW_FILE);
  remove(INVALID_FILE);
  remove(FLAG_TAMPER_FILE);
  remove(PAYLOAD_TAMPER_FILE);
}

static uint64_t read_u64_le(const unsigned char* src) {
  /* value 用来累加 8 个字节拼出的无符号整数。 */
  uint64_t value = 0;

  /* 按小端顺序把每个字节移动到对应位置。 */
  for (int i = 0; i < 8; ++i) {
    value |= (uint64_t)src[i] << (i * 8);
  }

  /* 返回最终结果。 */
  return value;
}

static unsigned char* read_binary_file(const char* filename, size_t* out_size) {
  /* file 保存打开后的文件句柄。 */
  FILE* file = fopen(filename, "rb");

  /* file_size_long 保存 ftell 读出的文件长度。 */
  long file_size_long;

  /* data 保存完整文件内容。 */
  unsigned char* data;

  /* 输出长度参数必须有效。 */
  EXPECT_TRUE(out_size != NULL);
  /* 打开文件必须成功。 */
  EXPECT_TRUE(file != NULL);

  /* 先跳到文件尾部读取长度。 */
  EXPECT_TRUE(fseek(file, 0, SEEK_END) == 0);
  file_size_long = ftell(file);
  EXPECT_TRUE(file_size_long >= 0);
  EXPECT_TRUE(fseek(file, 0, SEEK_SET) == 0);

  /* 为整文件内容分配缓冲区。 */
  data = malloc((size_t)file_size_long);
  memory_check(data);

  /* 一次性读取完整文件。 */
  EXPECT_TRUE(fread(data, 1, (size_t)file_size_long, file) ==
              (size_t)file_size_long);
  fclose(file);

  /* 把长度写回调用方。 */
  *out_size = (size_t)file_size_long;
  return data;
}

static void write_binary_file(const char* filename, const unsigned char* data,
                              size_t size) {
  /* file 保存目标文件句柄。 */
  FILE* file = fopen(filename, "wb");

  /* 打开文件必须成功。 */
  EXPECT_TRUE(file != NULL);

  /* 把指定字节流原样写回磁盘。 */
  EXPECT_TRUE(fwrite(data, 1, size, file) == size);
  fclose(file);
}

static AuthStatus save_encrypted_snapshot(const char* filename,
                                          const char* password) {
  /* json_text 保存从 Data 模块导出的 JSON 文本。 */
  char* json_text = NULL;

  /* json_len 保存 JSON 文本的字节长度。 */
  size_t json_len = 0;

  /* status 保存最终的 Auth 状态。 */
  AuthStatus status;

  /* 先导出当前链表对应的 JSON 明文。 */
  EXPECT_STATUS(data_export_json(&json_text, &json_len), DATA_OK);

  /* 再调用 Auth 模块把明文加密保存到文件。 */
  status = auth_save_file(filename, password, (const unsigned char*)json_text,
                          json_len);

  /* 把导出的明文 JSON 擦除后释放。 */
  auth_wipe_text(json_text, json_len + 1);
  free(json_text);

  /* 保存成功后，当前内存与磁盘内容一致。 */
  if (status == AUTH_OK) {
    data_mark_clean();
  }

  /* 返回最终状态。 */
  return status;
}

static AuthStatus load_plaintext_from_file(const char* filename,
                                           const char* password,
                                           unsigned char** out_plaintext,
                                           size_t* out_plaintext_len) {
  /* 直接把请求转给 Auth 模块。 */
  return auth_load_file(filename, password, out_plaintext, out_plaintext_len);
}

static DataStatus import_plaintext_into_data(unsigned char* plaintext,
                                             size_t plaintext_len) {
  /* status 保存 Data 模块的导入结果。 */
  DataStatus status =
      data_import_json((const char*)plaintext, plaintext_len);

  /* Data 模块使用完明文后，立刻擦除并释放缓冲区。 */
  auth_wipe_text((char*)plaintext, plaintext_len + 1);
  auth_free_buffer(plaintext);

  /* 返回最终状态。 */
  return status;
}

static const DataNode* require_record(const char* id) {
  /* 先按 ID 查找记录。 */
  const DataNode* node = data_find_by_id(id);
  /* 这条记录必须存在。 */
  EXPECT_TRUE(node != NULL);
  /* 返回查到的记录指针。 */
  return node;
}

static void expect_record(const char* id, const char* name, const char* value) {
  /* 先确保目标记录存在。 */
  const DataNode* node = require_record(id);
  /* 校验 ID。 */
  EXPECT_STRING(node->id, id);
  /* 校验姓名。 */
  EXPECT_STRING(node->name, name);
  /* 校验 value。 */
  EXPECT_STRING(node->value, value);
}

static int result_contains_id(DataNode** results, int count, const char* id) {
  /* 在线性结果数组中查找指定 ID。 */
  for (int i = 0; i < count; ++i) {
    if (strcmp(results[i]->id, id) == 0) {
      return 1;
    }
  }
  /* 遍历结束仍未找到时返回 0。 */
  return 0;
}

static void expect_prefix_ids(const char* prefix, const char* const* ids,
                              int expected_count) {
  /* count 由 trie_search 写出实际命中数量。 */
  int count = 0;
  /* results 是 Trie 返回的动态结果数组。 */
  DataNode** results = trie_search(prefix, &count);

  /* 先校验命中数量。 */
  if (count != expected_count) {
    char detail[128];
    snprintf(detail, sizeof(detail), "prefix='%s' actual=%d expected=%d",
             prefix, count, expected_count);
    fail_impl("trie_search count", __FILE__, __LINE__, detail);
  }

  /* 再校验预期中的每个 ID 都出现在结果里。 */
  for (int i = 0; i < expected_count; ++i) {
    if (!result_contains_id(results, count, ids[i])) {
      char detail[128];
      snprintf(detail, sizeof(detail), "prefix='%s' missing id='%s'", prefix,
               ids[i]);
      fail_impl("trie_search result_contains_id", __FILE__, __LINE__, detail);
    }
  }

  /* 结果数组由调用方负责释放。 */
  free(results);
}

static void seed_featured_records(void) {
  /* 写入几条覆盖中文、英文、同前缀场景的样例记录。 */
  EXPECT_STATUS(data_insert("陈晨", "CS26A01", "算法设计 91", 1), DATA_OK);
  EXPECT_STATUS(data_insert("陈辰", "CS26A02", "数据结构 88", 1), DATA_OK);
  EXPECT_STATUS(data_insert("陈晨曦", "CS26A03", "编译原理 95", 1), DATA_OK);
  EXPECT_STATUS(data_insert("Alice", "SE24C21", "Software Engineering 94", 1),
                DATA_OK);
  EXPECT_STATUS(data_insert("Alicia", "SE24C22", "Computer Networks 87", 1),
                DATA_OK);
}

static void test_hash_and_list_order(void) {
  /* head 和 tail 用来检查双向链表顺序。 */
  DataNode* head;
  DataNode* tail;

  /* 每个测试都从干净环境开始。 */
  reset_environment();
  /* 先写入一组基础数据。 */
  seed_featured_records();

  /* 验证按 ID 的查找结果。 */
  expect_record("CS26A01", "陈晨", "算法设计 91");
  expect_record("SE24C22", "Alicia", "Computer Networks 87");

  /* 验证重复 ID 会被拒绝。 */
  EXPECT_STATUS(data_insert("重复用户", "CS26A01", "Dup", 1), DATA_DUPLICATE_ID);

  /* 读取链表头尾。 */
  head = list_head();
  tail = list_tail();

  /* 头尾都应该存在。 */
  EXPECT_TRUE(head != NULL);
  EXPECT_TRUE(tail != NULL);

  /* 头节点应该是最后一次头插的记录。 */
  EXPECT_STRING(head->id, "SE24C22");
  EXPECT_STRING(head->name, "Alicia");

  /* 尾节点应该是最早插入的记录。 */
  EXPECT_STRING(tail->id, "CS26A01");
  EXPECT_STRING(tail->name, "陈晨");

  /* 再验证链表内部的 next 指针关系。 */
  EXPECT_TRUE(head->next != NULL);
  EXPECT_STRING(head->next->id, "SE24C21");
}

static void test_trie_prefix_search(void) {
  /* 这组 ID 对应前缀“陈”的期望结果。 */
  static const char* chen_ids[] = {"CS26A01", "CS26A02", "CS26A03"};
  /* 这组 ID 对应前缀“陈晨”的期望结果。 */
  static const char* chenchen_ids[] = {"CS26A01", "CS26A03"};
  /* 这组 ID 对应前缀“Ali”的期望结果。 */
  static const char* ali_ids[] = {"SE24C21", "SE24C22"};

  reset_environment();
  seed_featured_records();

  /* 验证多个中文前缀场景。 */
  expect_prefix_ids("陈", chen_ids, 3);
  expect_prefix_ids("陈晨", chenchen_ids, 2);

  /* 验证英文前缀搜索。 */
  expect_prefix_ids("Ali", ali_ids, 2);
  expect_prefix_ids("Alic", ali_ids, 2);

  {
    /* count 初始为 0，用于接收查询结果数。 */
    int count = 0;
    /* 不存在的前缀应当返回 NULL 且 count 为 0。 */
    DataNode** results = trie_search("不存在", &count);
    EXPECT_TRUE(results == NULL);
    EXPECT_TRUE(count == 0);
  }
}

static void test_modify_delete_and_undo(void) {
  reset_environment();
  seed_featured_records();

  /* 先验证修改功能。 */
  EXPECT_STATUS(data_modify("CS26A02", "数据结构 90", 1), DATA_OK);
  expect_record("CS26A02", "陈辰", "数据结构 90");

  /* 撤销修改后，旧值应该恢复。 */
  EXPECT_STATUS(data_undo(), DATA_OK);
  expect_record("CS26A02", "陈辰", "数据结构 88");

  /* 再验证删除功能。 */
  EXPECT_STATUS(data_delete("SE24C21", 1), DATA_OK);
  EXPECT_TRUE(data_find_by_id("SE24C21") == NULL);

  /* 撤销删除后，记录应该回到系统里。 */
  EXPECT_STATUS(data_undo(), DATA_OK);
  expect_record("SE24C21", "Alice", "Software Engineering 94");

  /* 删除不存在的 ID 应返回未找到。 */
  EXPECT_STATUS(data_delete("NO_SUCH_ID", 1), DATA_NOT_FOUND);
}

static void test_save_load_and_dirty_guards(void) {
  /* plaintext 保存 Auth 解密出来的 JSON 明文。 */
  unsigned char* plaintext = NULL;

  /* plaintext_len 保存 JSON 明文长度。 */
  size_t plaintext_len = 0;

  /* json_text 保存导出的 JSON 文本。 */
  char* json_text = NULL;

  /* json_len 保存导出的 JSON 文本长度。 */
  size_t json_len = 0;

  reset_environment();
  seed_featured_records();

  /* 首次保存应成功，且脏标记清零。 */
  EXPECT_AUTH_STATUS(save_encrypted_snapshot(SAVE_FILE, PASSWORD), AUTH_OK);
  EXPECT_TRUE(data_is_modified() == 0);

  /* 修改后脏标记应为 1。 */
  EXPECT_STATUS(data_modify("CS26A01", "算法设计 99", 1), DATA_OK);
  EXPECT_TRUE(data_is_modified() == 1);

  /* 有未保存修改时，Data 层会阻止直接导入新的明文 JSON。 */
  EXPECT_STATUS(data_export_json(&json_text, &json_len), DATA_OK);
  EXPECT_STATUS(data_import_json(json_text, json_len), DATA_DIRTY_BLOCKED);
  auth_wipe_text(json_text, json_len + 1);
  free(json_text);

  /* 先重新保存，再经过 Auth + Data 的组合路径重新加载。 */
  EXPECT_AUTH_STATUS(save_encrypted_snapshot(SAVE_FILE, PASSWORD), AUTH_OK);
  data_init();
  EXPECT_AUTH_STATUS(load_plaintext_from_file(SAVE_FILE, PASSWORD, &plaintext,
                                              &plaintext_len),
                     AUTH_OK);
  EXPECT_STATUS(import_plaintext_into_data(plaintext, plaintext_len), DATA_OK);
  expect_record("CS26A01", "陈晨", "算法设计 99");
  EXPECT_TRUE(data_is_modified() == 0);
}

static void test_wrong_password_preserves_memory(void) {
  /* plaintext 保存 Auth 解密出来的 JSON 明文。 */
  unsigned char* plaintext = NULL;

  /* plaintext_len 保存 JSON 明文长度。 */
  size_t plaintext_len = 0;

  reset_environment();
  seed_featured_records();

  /* 先保存一份正确的加密快照。 */
  EXPECT_AUTH_STATUS(save_encrypted_snapshot(SAVE_FILE, PASSWORD), AUTH_OK);

  /* 再准备一组当前内存里的基线数据，用来验证失败后状态不变。 */
  data_init();
  EXPECT_STATUS(data_insert("保留用户", "KEEP01", "Keep Value", 1), DATA_OK);

  /* 错误密码应当在 Auth 层直接失败。 */
  EXPECT_AUTH_STATUS(load_plaintext_from_file(SAVE_FILE, WRONG_PASSWORD,
                                              &plaintext, &plaintext_len),
                     AUTH_PASSWORD_ERROR);
  EXPECT_TRUE(plaintext == NULL);
  EXPECT_TRUE(plaintext_len == 0);

  /* 当前内存数据应保持原样。 */
  expect_record("KEEP01", "保留用户", "Keep Value");
  EXPECT_TRUE(data_find_by_id("CS26A01") == NULL);
}

static void test_auth_validation_and_new_file(void) {
  /* duplicate_json 用来验证重复 ID 导入会整体失败并回滚。 */
  static const char* duplicate_json =
      "[{\"name\":\"重复一\",\"id\":\"DUP01\",\"value\":\"V1\"},"
      "{\"name\":\"重复二\",\"id\":\"DUP01\",\"value\":\"V2\"}]";

  /* file_data 保存读出的加密文件内容。 */
  unsigned char* file_data;

  /* file_size 保存加密文件总长度。 */
  size_t file_size;

  /* flag_size 保存头部里记录的 flag 密文长度。 */
  uint64_t flag_size;

  /* plaintext 保存 Auth 解密出来的 JSON 明文。 */
  unsigned char* plaintext = NULL;

  /* plaintext_len 保存 JSON 明文长度。 */
  size_t plaintext_len = 0;

  reset_environment();
  seed_featured_records();

  /* 先保存一份合法加密文件。 */
  EXPECT_AUTH_STATUS(save_encrypted_snapshot(SAVE_FILE, PASSWORD), AUTH_OK);

  /* 读出完整文件内容，后续基于它构造篡改样本。 */
  file_data = read_binary_file(SAVE_FILE, &file_size);
  EXPECT_TRUE(file_size > AUTH_HEADER_SIZE_TEST);

  /* 先篡改 magic，验证文件头校验。 */
  file_data[0] ^= 0x01;
  write_binary_file(INVALID_FILE, file_data, file_size);
  EXPECT_AUTH_STATUS(load_plaintext_from_file(INVALID_FILE, PASSWORD, &plaintext,
                                              &plaintext_len),
                     AUTH_FORMAT_ERROR);
  EXPECT_TRUE(plaintext == NULL);
  EXPECT_TRUE(plaintext_len == 0);

  /* 恢复原始文件内容，再构造 flag 篡改样本。 */
  free(file_data);
  file_data = read_binary_file(SAVE_FILE, &file_size);
  flag_size = read_u64_le(file_data + AUTH_FLAG_SIZE_OFFSET_TEST);
  file_data[AUTH_HEADER_SIZE_TEST] ^= 0x01;
  write_binary_file(FLAG_TAMPER_FILE, file_data, file_size);
  EXPECT_AUTH_STATUS(load_plaintext_from_file(FLAG_TAMPER_FILE, PASSWORD,
                                              &plaintext, &plaintext_len),
                     AUTH_PASSWORD_ERROR);
  EXPECT_TRUE(plaintext == NULL);
  EXPECT_TRUE(plaintext_len == 0);

  /* 再篡改 payload，验证真正数据段的认证失败分支。 */
  file_data[AUTH_HEADER_SIZE_TEST] ^= 0x01;
  file_data[AUTH_HEADER_SIZE_TEST + (size_t)flag_size] ^= 0x01;
  write_binary_file(PAYLOAD_TAMPER_FILE, file_data, file_size);
  EXPECT_AUTH_STATUS(load_plaintext_from_file(PAYLOAD_TAMPER_FILE, PASSWORD,
                                              &plaintext, &plaintext_len),
                     AUTH_CRYPTO_ERROR);
  EXPECT_TRUE(plaintext == NULL);
  EXPECT_TRUE(plaintext_len == 0);

  /* 当前内存数据仍然保持保存前的状态。 */
  expect_record("CS26A03", "陈晨曦", "编译原理 95");

  /* 新建空加密文件后，把内存切到空状态。 */
  EXPECT_AUTH_STATUS(auth_save_file(NEW_FILE, PASSWORD,
                                    (const unsigned char*)"[]", 2),
                     AUTH_OK);
  data_init();
  EXPECT_TRUE(data_find_by_id("CS26A01") == NULL);
  EXPECT_TRUE(list_head() == NULL);
  EXPECT_TRUE(list_tail() == NULL);

  /* 再加载空加密文件，验证空数组可以正常导入。 */
  EXPECT_AUTH_STATUS(load_plaintext_from_file(NEW_FILE, PASSWORD, &plaintext,
                                              &plaintext_len),
                     AUTH_OK);
  EXPECT_STATUS(import_plaintext_into_data(plaintext, plaintext_len), DATA_OK);
  EXPECT_TRUE(data_find_by_id("CS26A01") == NULL);
  EXPECT_TRUE(list_head() == NULL);
  EXPECT_TRUE(list_tail() == NULL);
  EXPECT_TRUE(data_is_modified() == 0);

  /* 手工传入一段结构错误的 JSON 明文，验证 Data 层的格式校验。 */
  EXPECT_STATUS(data_import_json("{ \"broken\": true }",
                                 strlen("{ \"broken\": true }")),
                DATA_FORMAT_ERROR);

  /* 重复 ID 的 JSON 导入也应整体失败，并保持空状态。 */
  EXPECT_STATUS(data_import_json(duplicate_json, strlen(duplicate_json)),
                DATA_FORMAT_ERROR);
  EXPECT_TRUE(data_find_by_id("DUP01") == NULL);
  EXPECT_TRUE(list_head() == NULL);
  EXPECT_TRUE(list_tail() == NULL);
  EXPECT_TRUE(data_is_modified() == 0);

  /* 释放最后一份文件缓冲区。 */
  free(file_data);
}

static void test_long_name_delete_safety(void) {
  /* 构造一个较长的名字，覆盖 Trie 删除路径较深的场景。 */
  char long_name[401];

  reset_environment();

  /* 用 'Z' 填满前 400 个字符。 */
  memset(long_name, 'Z', sizeof(long_name) - 1);
  /* 手动补上字符串结束符。 */
  long_name[sizeof(long_name) - 1] = '\0';

  /* 验证长名字可以成功插入。 */
  EXPECT_STATUS(data_insert(long_name, "LONG400", "Long Record", 1), DATA_OK);
  expect_record("LONG400", long_name, "Long Record");

  /* 删除后，这条记录应该立刻消失。 */
  EXPECT_STATUS(data_delete("LONG400", 1), DATA_OK);
  EXPECT_TRUE(data_find_by_id("LONG400") == NULL);

  /* 撤销后，长名字记录应当安全恢复。 */
  EXPECT_STATUS(data_undo(), DATA_OK);
  expect_record("LONG400", long_name, "Long Record");
}

int main(void) {
  /* tests 数组集中列出全部 smoke test。 */
  static const TestCase tests[] = {
      {"hash_and_list_order", test_hash_and_list_order},
      {"trie_prefix_search", test_trie_prefix_search},
      {"modify_delete_and_undo", test_modify_delete_and_undo},
      {"save_load_and_dirty_guards", test_save_load_and_dirty_guards},
      {"wrong_password_preserves_memory", test_wrong_password_preserves_memory},
      {"auth_validation_and_new_file", test_auth_validation_and_new_file},
      {"long_name_delete_safety", test_long_name_delete_safety},
  };

  /* count 表示测试用例总数。 */
  size_t count = sizeof(tests) / sizeof(tests[0]);

  /* 逐个执行测试。 */
  for (size_t i = 0; i < count; ++i) {
    tests[i].run();
    printf("[PASS] %s\n", tests[i].name);
  }

  /* 全部通过后清理临时文件。 */
  remove(SAVE_FILE);
  remove(NEW_FILE);
  remove(INVALID_FILE);
  remove(FLAG_TAMPER_FILE);
  remove(PAYLOAD_TAMPER_FILE);

  /* 输出总通过信息。 */
  puts("core smoke test passed");

  /* 返回 0 表示测试程序执行成功。 */
  return 0;
}
