// Copyright 2025 The QLever Authors, in particular:
//
// 2025 BMW AG

// PMR utility functions.
// The container aliases (vector, string, set, unordered_map, unordered_set)
// come from the namespace alias `ql::pmr = std::pmr` (or
// boost::container::pmr). This header adds smart-pointer helpers and string
// conversion helpers in the `ql::pmr` namespace.

#ifndef QLEVER_SRC_UTIL_PMR_UTILS_H
#define QLEVER_SRC_UTIL_PMR_UTILS_H

#include <memory>
#include <string_view>
#include <type_traits>

#include "backports/memory_resource.h"

namespace ql::pmr {

// ---------------------------------------------------------------------------
// allocate_shared / allocate_unique  — construct objects via a PMR allocator
// ---------------------------------------------------------------------------

/// Create a `std::shared_ptr<T>` whose control-block and object storage are
/// both obtained from `resource`.
template <typename T, typename... Args>
std::shared_ptr<T> allocate_shared(ql::pmr::memory_resource* const resource,
                                   Args&&... args) {
  ql::pmr::polymorphic_allocator<T> alloc{resource};
  return std::allocate_shared<T>(alloc, std::forward<Args>(args)...);
}

/// Create a `std::unique_ptr<T>` whose storage is obtained from `resource`.
/// The custom deleter calls `deallocate` on the same resource, so the
/// returned pointer is safe to move but the resource must outlive it.
template <typename T, typename... Args>
auto allocate_unique(ql::pmr::memory_resource* const resource, Args&&... args) {
  ql::pmr::polymorphic_allocator<T> alloc{resource};
  T* ptr = alloc.allocate(1);
  try {
    new (ptr) T(std::forward<Args>(args)...);
  } catch (...) {
    alloc.deallocate(ptr, 1);
    throw;
  }
  auto deleter = [resource](T* p) {
    ql::pmr::polymorphic_allocator<T> a{resource};
    p->~T();
    a.deallocate(p, 1);
  };
  return std::unique_ptr<T, decltype(deleter)>(ptr, std::move(deleter));
}

// ---------------------------------------------------------------------------
// Conversion helpers — std::string ↔ PMR string
// ---------------------------------------------------------------------------

/// Create a PMR string from a string_view, allocated via `r`.
inline ql::pmr::string to_pmr_string(std::string_view sv,
                                     ql::pmr::memory_resource* const r) {
  return ql::pmr::string(sv.data(), sv.size(),
                         ql::pmr::polymorphic_allocator<char>{r});
}

}  // namespace ql::pmr

#endif  // QLEVER_SRC_UTIL_PMR_UTILS_H
