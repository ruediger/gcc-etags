// copied from http://www.codesynthesis.com/~boris/blog/2010/05/03/parsing-cxx-with-gcc-plugin-part-1/

// g++ -std=c++0x -Wall -Wextra -I`g++ -print-file-name=plugin`/include -fPIC -shared plugin.c++ -o plugin.so
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
#include <set>

using namespace std;

int plugin_is_GPL_compatible;

struct decl_comparator {
  bool
  operator() (tree x, tree y) const {
    location_t xl = DECL_SOURCE_LOCATION(x);
    location_t yl = DECL_SOURCE_LOCATION(y);
 
    return xl < yl;
  }
};

typedef std::multiset<tree, decl_comparator> decl_set;

void
print_decl(tree decl) {
  int tc = TREE_CODE(decl);
  tree id = DECL_NAME(decl);
  const char *name =
    id
    ? IDENTIFIER_POINTER(id)
    : "<unnamed>";

  cerr << tree_code_name[tc] << " " << name << " at "
       << DECL_SOURCE_FILE (decl) << ":"
       << DECL_SOURCE_LINE (decl) << endl;
}

void
collect(tree ns, decl_set& set) {
  tree decl;
  cp_binding_level *level = NAMESPACE_LEVEL(ns);
 
  // Collect declarations.
  for(decl = level->names; decl != 0; decl = TREE_CHAIN (decl)) {
    if(DECL_IS_BUILTIN(decl)) {
      continue;
    }
 
    set.insert(decl);
  }
 
  // Traverse namespaces.
  for(decl = level->namespaces; decl != 0; decl = TREE_CHAIN (decl)) {
    if(DECL_IS_BUILTIN(decl)) {
      continue;
    }
 
    collect(decl, set);
  }
}

static void
traverse(tree ns) {
  decl_set decls;
  collect(ns, decls);
 
  for(tree const &t : decls) {
    print_decl(t);
  }
}

extern "C" void
gate_callback(void*, void*) {
  // If there were errors during compilation, let GCC handle the exit.
  if(errorcount or sorrycount) {
    return;
  }

  int r = 0;
  cerr << "processing " << main_input_filename << endl;

  traverse(global_namespace);

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
