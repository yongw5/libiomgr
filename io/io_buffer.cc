#include "iomgr/io_buffer.h"

#include "glog/logging.h"

namespace iomgr {

IOBuffer::IOBuffer() : data_(nullptr) {}

IOBuffer::IOBuffer(int buffer_size) { data_ = new char[buffer_size]; }

IOBuffer::IOBuffer(char* data) : data_(data) {}

IOBuffer::~IOBuffer() {
  delete[] data_;
  data_ = nullptr;
}

IOBufferWithSize::IOBufferWithSize(size_t size) : IOBuffer(size), size_(size) {}

IOBufferWithSize::IOBufferWithSize(char* data, size_t size)
    : IOBuffer(data), size_(size) {}

IOBufferWithSize::~IOBufferWithSize() = default;

StringIOBuffer::StringIOBuffer(const std::string& s)
    : IOBuffer(nullptr), string_data_(s) {
  // TODO
  data_ = const_cast<char*>(string_data_.data());
}

StringIOBuffer::StringIOBuffer(std::unique_ptr<std::string> s)
    : IOBuffer(static_cast<char*>(nullptr)) {
  // TODO AssertValidBufferSize
  string_data_.swap(*s.get());
  data_ = const_cast<char*>(string_data_.data());
}

StringIOBuffer::~StringIOBuffer() { data_ = nullptr; }

DrainableIOBuffer::DrainableIOBuffer(RefPtr<IOBuffer> base, size_t size)
    : IOBuffer(base->data()), base_(base), size_(size), used_(0) {
  // AssertValidBufferSize(size);
}

void DrainableIOBuffer::DidConsume(int bytes) { SetOffset(used_ + bytes); }

int DrainableIOBuffer::BytesRemaining() const { return size_ - used_; }

// Returns the number of consumed bytes.
int DrainableIOBuffer::BytesConsumed() const { return used_; }

void DrainableIOBuffer::SetOffset(int bytes) {
  DCHECK_GE(bytes, 0);
  DCHECK_LE(bytes, size_);
  used_ = bytes;
  data_ = base_->data() + used_;
}

DrainableIOBuffer::~DrainableIOBuffer() {
  // The buffer is owned by the |base_| instance.
  data_ = nullptr;
}

GrowableIOBuffer::GrowableIOBuffer() : IOBuffer(), capacity_(0), offset_(0) {}

void GrowableIOBuffer::SetCapacity(int capacity) {
  DCHECK_GE(capacity, 0);
  // realloc will crash if it fails.
  real_data_.reset(static_cast<char*>(realloc(real_data_.release(), capacity)));
  capacity_ = capacity;
  if (offset_ > capacity)
    set_offset(capacity);
  else
    set_offset(offset_);  // The pointer may have changed.
}

void GrowableIOBuffer::set_offset(int offset) {
  DCHECK_GE(offset, 0);
  DCHECK_LE(offset, capacity_);
  offset_ = offset;
  data_ = real_data_.get() + offset;
}

int GrowableIOBuffer::RemainingCapacity() { return capacity_ - offset_; }

char* GrowableIOBuffer::StartOfBuffer() { return real_data_.get(); }

GrowableIOBuffer::~GrowableIOBuffer() { data_ = nullptr; }

WrappedIOBuffer::WrappedIOBuffer(const char* data)
    : IOBuffer(const_cast<char*>(data)) {}

WrappedIOBuffer::~WrappedIOBuffer() { data_ = nullptr; }

}  // namespace iomgr