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

#include <string>
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>

// add missing macros
#ifndef LOCATION_COLUMN
// copied from 4.8+
#define LOCATION_COLUMN(LOC)((expand_location (LOC)).column)
#endif
#ifndef DECL_SOURCE_COLUMN
#define DECL_SOURCE_COLUMN(NODE) LOCATION_COLUMN (DECL_SOURCE_LOCATION (NODE))
#endif

using namespace std;

int plugin_is_GPL_compatible;

namespace {

// Adds charpos and line data to GCC's tree structure
struct extended_tree {
  tree decl;
  size_t charpos;
  std::string line;

  extended_tree(tree t) : decl(t) { }
};

struct decl_comparator {
  bool
  operator() (extended_tree const &x, extended_tree const &y) const {
    location_t xl = DECL_SOURCE_LOCATION(x.decl);
    location_t yl = DECL_SOURCE_LOCATION(y.decl);

    return xl < yl;
  }
};

typedef std::multiset<extended_tree, decl_comparator> decl_set;

// add charpos and line information
void
extend_trees(decl_set &s) {
  if(s.empty()) {
    return;
  }

  tree t = s.begin()->decl;
  expanded_location xloc = expand_location(DECL_SOURCE_LOCATION(t));
  if(not xloc.file) {
    return;
  }
  std::ifstream in(xloc.file);
  std::string buffer;
  int line = 0;
  std::size_t charpos = 0;
  for(auto xtree : s) {
    xloc = expand_location(DECL_SOURCE_LOCATION(xtree.decl));

    while(std::getline(in, buffer)) {
      ++line;
      if(line >= xloc.line) {
        xtree.charpos = charpos + xloc.column;
        xtree.line = buffer;
        break;
      }
      else {
        charpos += buffer.size();
      }
    }

    if(line < xloc.line) {
      cerr << "IOError!\n";
      return;
    }
  }
}

char const *decl_name(tree decl) {
  assert(decl);
  tree id = DECL_NAME(decl);
  return id ? IDENTIFIER_POINTER(id) : "<unnamed>";
}

std::string format_namespaces(tree decl) {
  tree cntxt = DECL_CONTEXT(decl);
  if(cntxt) {
    int tc = TREE_CODE(cntxt);
    if(tc == NAMESPACE_DECL) {
      return std::string("::") + decl_name(cntxt) + format_namespaces(cntxt);
    }
  }
  return "";
}

char const DEL = '\x7f';
char const SOH = '\x01';

void
print_decl(tree decl) {
  int tc = TREE_CODE(decl);
  const char *name = decl_name(decl);

  cout << /* TODO pattern */ DEL << format_namespaces(decl) << "::" << name << SOH
       << DECL_SOURCE_LINE(decl) << ',' << DECL_SOURCE_COLUMN(decl) << '\n';

  // TODO charpos instead of column!

#if 0
  cerr << tree_code_name[tc] << ' ' << name << " at "
       << DECL_SOURCE_FILE(decl) << ':'
       << DECL_SOURCE_LINE(decl) << ':'
       << DECL_SOURCE_COLUMN(decl) << endl;

  cerr << format_namespaces(decl) << "::" << name << endl;

  tree cntxt = DECL_CONTEXT(decl);
  if(cntxt) {
    tc = TREE_CODE(cntxt);
    cerr << '\t' << tree_code_name[tc] << " " << decl_name(cntxt) << '\n';
  }
#endif
}

void
collect(tree ns, decl_set &set) {
  tree decl;
  cp_binding_level *level = NAMESPACE_LEVEL(ns);

  // Collect declarations.
  for(decl = level->names; decl != 0; decl = TREE_CHAIN (decl)) {
    if(DECL_IS_BUILTIN(decl) or DECL_EXTERNAL(decl)) {
      continue;
    }

    set.insert(decl);
  }

  // Traverse namespaces.
  for(decl = level->namespaces; decl != 0; decl = TREE_CHAIN (decl)) {
    if(DECL_IS_BUILTIN(decl) or DECL_EXTERNAL(decl)) {
      continue;
    }

    collect(decl, set);
  }
}

void
traverse(tree ns) {
  decl_set decls;
  collect(ns, decls);
  extend_trees(decls);

  if(decls.empty()) {
    cerr << "no declarations\n";
    return;
  }

  cout << "\f\n" << DECL_SOURCE_FILE(decls.begin()->decl) << ',' << /* todo unsint? */ '\n';
  // unsint -> size_of_entries

  for(extended_tree const &t : decls) {
    print_decl(t.decl);
  }
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
