#include <stdio.h>

#include "Data.h"

int main(void) {
  // 初始化数据系统
  data_init();
  // 添加测试数据
  data_insert("Hello", "20250318", "World");
  data_insert("C", "20260318", "NB");
  // 测试数据 name 获取
  data_get("Hello");
  data_get("C");
  // 测试数据 id 获取
  data_get("20250318");
  data_get("20260318");
  // 测试数据保存
  data_save("test.dat");
  // 测试数据加载
  data_load("test.dat");
  // 测试数据加载后获取
  data_get("Hello");
  data_get("20250318");
  data_get("C");
  data_get("20260318");
  // 测试重复插入
  data_insert("Hello", "20250318", "Everyone");
  data_insert("C", "20260318", "Programming");
  // 测试数据 name 修改
  data_modify("Hello", "Everyone");
  data_get("Hello");
  data_modify("C", "Programming");
  data_get("C");
  // 测试数据 id 修改
  data_modify("20250318", "id_test_Hello");
  data_get("Hello");
  data_modify("20260318", "id_test_C");
  data_get("C");
  // 测试数据删除
  data_delete("20250318");
  data_get("Hello");
  data_get("20250318");
  data_delete("20260318");
  data_get("C");
  data_get("20260318");
  // 释放内存
  data_exit();
  return 0;
}