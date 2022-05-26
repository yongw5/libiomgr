#ifndef LIBIOMGR_UTIL_POINTER_HASH_H_
#define LIBIOMGR_UTIL_POINTER_HASH_H_

namespace iomgr {

template <typename T>
struct PointerHash;

template <typename T>
struct PointerHash<T*> {
  size_t operator()(T* p) const {
    return (((reinterpret_cast<size_t>(p)) >> 4) ^
            ((reinterpret_cast<size_t>(p)) >> 9) ^
            ((reinterpret_cast<size_t>(p)) >> 14));
  }
};

}  // namespace iomgr

#endif // LIBIOMGR_UTIL_POINTER_HASH_H_