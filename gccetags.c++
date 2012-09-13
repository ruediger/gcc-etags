/* -*- mode:C++; coding:utf-8; -*-
 *
 * Copyright (C) 2012 by Rüdiger Sonderfeld <ruediger@c-plusplus.de>
 *
 * Parts copied from http://www.codesynthesis.com/~boris/blog/2010/05/03/parsing-cxx-with-gcc-plugin-part-1/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

// g++ -S -fplugin=./gccetags.so test.c++

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
  std::string pattern;

  extended_tree(tree t) : decl(t),charpos(0) { }
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

// transform line to a pattern
std::string
line_to_pattern(tree decl, std::string const &line) {
  int const col = DECL_SOURCE_COLUMN(decl);
  tree type = TREE_TYPE(decl);
  int const tc = TREE_CODE(type);

  size_t beg = line.rfind(';', col > 0 ? col-1 : col);
  if(beg == std::string::npos) {
    beg = 0;
  }
  size_t n = std::string::npos;

  switch(tc) {
  case FUNCTION_TYPE: { // copy column to (
    n = line.find('(', col);
    break;
  }
  default:
    n = line.find(';', col);
  }

  if(n != std::string::npos) {
    n -= beg - 1;
  }
  return line.substr(beg, n);
}

// add charpos and pattern information
void
extend_trees(decl_set &s) {
  if(s.empty()) {
    return;
  }

  tree t = s.begin()->decl;
  expanded_location xloc = expand_location(DECL_SOURCE_LOCATION(t));
  if(not xloc.file) {
    cerr << "Nofile!\n";
    return;
  }
  std::ifstream in(xloc.file);
  std::string buffer;
  int line = 0;
  std::size_t charpos = 0;

  decl_set new_s;
  for(extended_tree xtree : s) { // sadly multiset has no non-const iterator
    xloc = expand_location(DECL_SOURCE_LOCATION(xtree.decl));

    while(std::getline(in, buffer)) {
      ++line;
      if(line >= xloc.line) {
        xtree.charpos = charpos + xloc.column;
        xtree.pattern = line_to_pattern(xtree.decl, buffer);
        new_s.insert(xtree);
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

  s = std::move(new_s);
}

char const *decl_name(tree decl) {
  assert(decl);
  tree id = DECL_NAME(decl);
  return id ? IDENTIFIER_POINTER(id) : "<unnamed>";
}

std::string format_namespaces(tree decl) {
  std::string s;
  std::string tmp;

  for(tree cntxt = CP_DECL_CONTEXT(decl);
      cntxt != global_namespace;
      cntxt = CP_DECL_CONTEXT(cntxt))
  {
    if(TREE_CODE(cntxt) == RECORD_TYPE) {
      cntxt = TYPE_NAME(cntxt);
    }

    tmp = std::string("::") + decl_name(cntxt) + s;
    s.swap(tmp);
  }

  return s;
}

char const DEL = '\x7f';
char const SOH = '\x01';

void
print_decl(extended_tree const &xtree) {
  tree decl = xtree.decl;
  const char *name = decl_name(decl);

  cout << xtree.pattern /* TODO pattern */<< DEL << format_namespaces(decl) << "::" << name << SOH
       << DECL_SOURCE_LINE(decl) << ',' << xtree.charpos << '\n';

#if 0
  int tc = TREE_CODE(decl);
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
collect_class(tree decl, decl_set &set) {
  tree type = TYPE_MAIN_VARIANT(TREE_TYPE(decl));

  for(tree d(TYPE_FIELDS(type));
      d != 0;
      d = TREE_CHAIN(d))
  {
    int const tc = TREE_CODE(d);
    if( (tc == TYPE_DECL and DECL_SELF_REFERENCE_P(d)) or
        (tc == FIELD_DECL and DECL_ARTIFICIAL(d)) )
    {
      continue;
    }

    set.insert(d);
  }

  for(tree d(TYPE_METHODS(type));
      d != 0;
      d = TREE_CHAIN (d))
  {
    if(not DECL_ARTIFICIAL (d)) {
      set.insert (d);
    }
  }
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

    if(TREE_CODE(decl) == TYPE_DECL and
       TREE_CODE(TREE_TYPE(decl)) == RECORD_TYPE and
       DECL_ARTIFICIAL(decl))  // if DECL_ARTIFICIAL is false this is a typedef and not a class declaration
    {
      collect_class(decl, set);
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
    print_decl(t);
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
