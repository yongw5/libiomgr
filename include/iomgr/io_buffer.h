#ifndef LIBIOMGR_INCLUDE_IO_BUFFER_H_
#define LIBIOMGR_INCLUDE_IO_BUFFER_H_

#include <memory>
#include <string>

#include "iomgr/export.h"
#include "iomgr/ref_counted.h"

namespace iomgr {

class IOMGR_EXPORT IOBuffer : public RefCounted<IOBuffer> {
 public:
  IOBuffer();
  explicit IOBuffer(int buffer_size);

  IOBuffer(const IOBuffer&) = delete;
  IOBuffer& operator=(const IOBuffer&) = delete;

  char* data() const { return data_; }

 protected:
  friend class RefCounted<IOBuffer>;

  explicit IOBuffer(char* data);
  virtual ~IOBuffer();

  char* data_;
};

class IOMGR_EXPORT IOBufferWithSize : public IOBuffer {
 public:
  explicit IOBufferWithSize(size_t size);
  size_t size() const { return size_; }

 protected:
  IOBufferWithSize(char* data, size_t size);
  ~IOBufferWithSize() override;

  size_t size_;
};

class IOMGR_EXPORT StringIOBuffer : public IOBuffer {
 public:
  explicit StringIOBuffer(const std::string& s);
  explicit StringIOBuffer(std::unique_ptr<std::string> s);
  size_t size() const { return string_data_.size(); }

 private:
  ~StringIOBuffer() override;

  std::string string_data_;
};

class IOMGR_EXPORT DrainableIOBuffer : public IOBuffer {
 public:
  DrainableIOBuffer(RefPtr<IOBuffer> base, size_t size);
  void DidConsume(int bytes);
  int BytesConsumed() const;
  int BytesRemaining() const;
  void SetOffset(int bytes);
  size_t size() const { return size_; }

 private:
  ~DrainableIOBuffer() override;

  RefPtr<IOBuffer> base_;
  size_t size_;
  int used_;
};

class IOMGR_EXPORT GrowableIOBuffer : public IOBuffer {
 public:
  GrowableIOBuffer();
  void SetCapacity(int capacity);
  int capacity() const { return capacity_; }
  void set_offset(int offset);
  int RemainingCapacity();
  char* StartOfBuffer();

 private:
  struct FreeDeleter {
    inline void operator()(void* ptr) const { free(ptr); }
  };

  ~GrowableIOBuffer() override;

  std::unique_ptr<char, FreeDeleter> real_data_;
  int capacity_;
  int offset_;
};

class IOMGR_EXPORT WrappedIOBuffer : public IOBuffer {
 public:
  explicit WrappedIOBuffer(const char* data);

 protected:
  ~WrappedIOBuffer() override;
};

}  // namespace iomgr

#endif  // LIBIOMGR_INCLUDE_IO_BUFFER_H_