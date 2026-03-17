#include <stdio.h>

#include "Data.h"

int main(void) {
  data_init();
  data_insert("Hello", "World");
  data_insert("C", "NB");
  data_get("Hello");
  data_get("C");
  data_save("test.dat");
  data_load("test.dat");
  data_get("Hello");
  data_get("C");
  data_insert("Hello", "Everyone");
  data_modify("Hello", "Everyone");
  data_get("Hello");
  data_insert("C", "Programming");
  data_modify("C", "Programming");
  data_get("C");
  data_delete("Hello");
  data_get("Hello");
  data_delete("C");
  data_get("C");
  data_exit();
  return 0;
}