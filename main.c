#include "Data.h"
#include "Display.h"

int main(void) {
  display_init();
  display_run();
  display_cleanup();
  return 0;
}
