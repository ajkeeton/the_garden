
// Apparently to include common code in a different directory we either need to
// make a library, or have a subdir named 'src' that links to the common code 
#include "src/animations/animations.h"
#include "src/mux.h"

Mux_Read mux;

void setup() {
  test();
}

void loop() {
}
