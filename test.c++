#include "test.h++"

namespace foo { }

typedef int new_t;

namespace ns1 {
  class c1 {
    int m;
  };

  namespace ns2 {
    void foo();

    void
    foo() {

    }
  }
}

int glob;

int
nl;

int main() {
  int i = 1;
  return i + glob;
}
