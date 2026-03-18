#include <stdio.h>

#include "Data.h"

int main(void) {
  data_init();
  data_insert("Hello", "20250318", "World");
  data_insert("C", "20260318", "NB");
  data_get("Hello");
  data_get("20250318");
  data_get("C");
  data_get("20260318");
  data_save("test.dat");
  data_load("test.dat");
  data_get("Hello");
  data_get("20250318");
  data_get("C");
  data_get("20260318");
  data_insert("Hello", "20250318", "Everyone");
  data_modify("Hello", "Everyone");
  data_get("Hello");
  data_modify("20250318", "id_test_Hello");
  data_get("Hello");
  data_insert("C", "20260318", "Programming");
  data_modify("C", "Programming");
  data_get("C");
  data_modify("20260318", "id_test_C");
  data_get("C");
  data_delete("20250318");
  data_get("Hello");
  data_get("20250318");
  data_delete("20260318");
  data_get("C");
  data_get("20260318");
  data_exit();
  return 0;
}