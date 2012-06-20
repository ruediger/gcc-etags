// copied from http://www.codesynthesis.com/~boris/blog/2010/05/03/parsing-cxx-with-gcc-plugin-part-1/

// g++ -I`g++ -print-file-name=plugin`/include -fPIC -shared plugin.c++ -o plugin.so
// g++ -S -fplugin=./plugin.so test.c++

#include <stdlib.h>
#include <gmp.h>

#include <cstdlib>

extern "C" { // order is important
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "intl.h"

#include "tm.h"

#include "cp/cp-tree.h"
#include "c-family/c-common.h"
#include "c-family/c-pragma.h"
#include "diagnostic.h"
}

#include <iostream>

using namespace std;

int plugin_is_GPL_compatible;

extern "C" void
gate_callback(void*, void*) {
  // If there were errors during compilation, let GCC handle the exit.
  if(errorcount or sorrycount) {
    return;
  }

  int r = 0;

  //
  // Process AST. Issue diagnostics and set r
  // to 1 in case of an error.
  //
  cerr << "processing " << main_input_filename << endl;

  exit(r);
}

extern "C" int
plugin_init(plugin_name_args *info, plugin_gcc_version *version) {
  cerr << "starting " << info->base_name << endl;

  if(not plugin_default_version_check(version, &gcc_version)) {
    cerr << "ERROR: failed version check!\n";
    return 1;
  }

  asm_file_name = HOST_BIT_BUCKET; // Disable assembly output.

  register_callback(info->base_name,
                    PLUGIN_OVERRIDE_GATE,
                    &gate_callback,
                    0);
  return 0;
}
