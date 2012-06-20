#include "test.h++"

namespace foo { }

typedef int new_t;

namespace ns1 {
  class c1 { };

  namespace ns2 {
    void foo();

    void
    foo() {

    }
  }
}

int glob;

int main() {
  int i = 1;
  return i + glob;
}
