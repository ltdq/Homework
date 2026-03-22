#include "Data.h"
#include "Display.h"

int file_modified = 0;

int main(void) {
  display_init();
  display_run();
  display_cleanup();
  return 0;
}
