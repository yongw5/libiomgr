#ifndef LIBIOMGR_INCLUDE_REF_COUNTED_H_
#define LIBIOMGR_INCLUDE_REF_COUNTED_H_

#include <atomic>

#include "iomgr/export.h"

namespace iomgr {

template <class T, typename Deleter>
class RefCounted;

template <class T>
struct IOMGR_EXPORT DefaultDeleter {
  static void Destruct(const T* x) {
    RefCounted<T, DefaultDeleter>::DeleteInternal(x);
  }
};

template <class T, typename Deleter = DefaultDeleter<T>>
class IOMGR_EXPORT RefCounted {
 public:
  RefCounted(const RefCounted&) = delete;
  RefCounted& operator=(const RefCounted&) = delete;

  bool HasOneRef() const {
    return ref_count_.load(std::memory_order_acquire) == 1;
  }
  bool HasAtLeastOneRef() const {
    return ref_count_.fetch_add(1, std::memory_order_relaxed) >= 1;
  }

  void AddRef() { ref_count_.fetch_add(1, std::memory_order_relaxed); }
  void Release() {
    if (ref_count_.fetch_sub(1, std::memory_order_relaxed) == 1) {
      Deleter::Destruct(static_cast<const T*>(this));
    }
  }

 protected:
  explicit RefCounted() = default;
  ~RefCounted() = default;

 private:
  friend struct DefaultDeleter<T>;

  template <typename U>
  static void DeleteInternal(const U* x) {
    delete x;
  }

  mutable std::atomic<int> ref_count_{0};
};

template <typename T>
struct RefPtr {
 public:
  RefPtr() = default;

  // Allow implicit construction from  nullptr
  RefPtr(std::nullptr_t) {}

  RefPtr(T* p) : ptr_(p) {
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  RefPtr(const RefPtr& r) : RefPtr(r.ptr_) {}
  RefPtr(RefPtr&& r) : ptr_(r.ptr_) { r.ptr_ = nullptr; }
  RefPtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }
  RefPtr& operator=(T* p) { return *this = RefPtr(p); }
  RefPtr& operator=(RefPtr r) {
    swap(r);
    return *this;
  }

  template <class U, typename = typename std::enable_if<
                         std::is_convertible<U*, T*>::value>::type>
  RefPtr(const RefPtr<U>& r) : RefPtr(r.ptr_) {}

  template <class U, typename = typename std::enable_if<
                         std::is_convertible<U*, T*>::value>::type>
  RefPtr(RefPtr<U>&& r) : ptr_(r.ptr_) {
    r.ptr_ = nullptr;
  }

  ~RefPtr() {
    if (ptr_) {
      ptr_->Release();
    }
  }

  T* get() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
  void reset() { RefPtr().swap(*this); }
  T* release() {
    T* ptr = ptr_;
    ptr_ = nullptr;
    return ptr;
  }
  void swap(RefPtr& r) { std::swap(ptr_, r.ptr_); }
  explicit operator bool() const { return ptr_ != nullptr; }

 protected:
  T* ptr_{nullptr};

 private:
  template <class U>
  friend class RefPtr;
};

template <class T, typename... Args>
RefPtr<T> MakeRefCounted(Args&&... args) {
  T* obj = new T(std::forward<Args>(args)...);
  return RefPtr<T>(obj);
}

// template <class T>
// RefPtr<T> AdoptRef(T* obj) {
//   DCHECK(obj);
//   DCHECK(obj->HasOneRef());
//   scoped_refptr<T> ret;
//   ret.ptr_ = obj;
//   return ret;
// }

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_REF_COUNTED_H_