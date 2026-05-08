// Copyright 2021 - 2025 The QLever Authors, in particular:
//
// 2025 Johannes Kalmbach <kalmbach@cs.uni-freiburg.de>, UFR

// UFR = University of Freiburg, Chair of Algorithms and Data Structures

// You may not use this file except in compliance with the Apache 2.0 License,
// which can be found in the `LICENSE` file at the root of the QLever project.

#ifndef QLEVER_SRC_BACKPORTS_MEMORY_RESOURCE_H
#define QLEVER_SRC_BACKPORTS_MEMORY_RESOURCE_H

// This file defines the `ql::pmr` namespace as a drop-in replacement for
// `std::pmr`. If `QLEVER_CPP_17` is defined, then the types from the
// `boost::container::pmr` namespace are used instead. Note1: `std::pmr` is
// technically part of C++17, but GCC 8.3 which we are targeting is not yet
// supporting it. Note2: the backported version requires linking against
// `Boost::container` for the `monotonic_buffer_resource`.

#ifdef QLEVER_CPP_17
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#else
#include <memory_resource>
#endif

#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Define `ql::pmr` as a real namespace (not an alias) so that other headers
// (e.g. PmrUtils.h) can reopen it and add helpers.  We pull in the core PMR
// types via using-declarations.

namespace ql {
namespace pmr {
#ifdef QLEVER_CPP_17
using ::boost::container::pmr::get_default_resource;
using ::boost::container::pmr::memory_resource;
using ::boost::container::pmr::monotonic_buffer_resource;
using ::boost::container::pmr::new_delete_resource;
using ::boost::container::pmr::null_memory_resource;
using ::boost::container::pmr::polymorphic_allocator;
using ::boost::container::pmr::set_default_resource;
#else
using ::std::pmr::get_default_resource;
using ::std::pmr::memory_resource;
using ::std::pmr::monotonic_buffer_resource;
using ::std::pmr::new_delete_resource;
using ::std::pmr::null_memory_resource;
using ::std::pmr::polymorphic_allocator;
using ::std::pmr::set_default_resource;
#endif

// PMR container aliases
template <typename T>
using vector = std::vector<T, polymorphic_allocator<T>>;

using string = std::basic_string<char, std::char_traits<char>,
                                 polymorphic_allocator<char>>;

template <typename T, typename Compare = std::less<T>>
using set = std::set<T, Compare, polymorphic_allocator<T>>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
using unordered_map =
    std::unordered_map<K, V, Hash, KeyEqual,
                       polymorphic_allocator<std::pair<const K, V>>>;

template <typename T, typename Hash = std::hash<T>,
          typename KeyEqual = std::equal_to<T>>
using unordered_set =
    std::unordered_set<T, Hash, KeyEqual, polymorphic_allocator<T>>;

}  // namespace pmr
}  // namespace ql

#endif  // QLEVER_SRC_BACKPORTS_MEMORY_RESOURCE_H
