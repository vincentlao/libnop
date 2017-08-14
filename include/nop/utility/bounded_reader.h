#ifndef LIBNOP_INCLUDE_NOP_UTILITY_BOUNDED_READER_H_
#define LIBNOP_INCLUDE_NOP_UTILITY_BOUNDED_READER_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include <nop/base/encoding.h>
#include <nop/base/handle.h>
#include <nop/base/utility.h>

namespace nop {

// BoundedReader is a reader type that wraps another reader pointer and tracks
// the number of bytes read. Reader operations are transparently passed to the
// underlying reader unless the requested operation would exceed the size limit
// set at construction. BufferReader can also skip padding remaining in the
// input up to the size limit in situations that require specific input payload
// size.
template <typename Reader>
class BoundedReader {
 public:
  BoundedReader() = default;
  BoundedReader(const BoundedReader&) = default;
  BoundedReader(Reader* reader, std::size_t size)
      : reader_{reader}, size_{size} {}

  BoundedReader& operator=(const BoundedReader&) = default;

  Status<void> Ensure(std::size_t size) {
    if (size_ - index_ < size)
      return ErrorStatus(ENOBUFS);
    else
      return reader_->Ensure(size);
  }

  Status<void> Read(EncodingByte* prefix) {
    if (index_ < size_) {
      auto status = reader_->Read(prefix);
      if (!status)
        return status;

      index_ += 1;
      return {};
    } else {
      return ErrorStatus(ENOBUFS);
    }
  }

  template <typename IterBegin, typename IterEnd>
  Status<void> ReadRaw(IterBegin begin, IterEnd end) {
    const std::size_t length_bytes =
        std::distance(begin, end) *
        sizeof(typename std::iterator_traits<IterBegin>::value_type);

    if (length_bytes > (size_ - index_))
      return ErrorStatus(ENOBUFS);

    auto status = reader_->ReadRaw(begin, end);
    if (!status)
      return status;

    index_ += length_bytes;
    return {};
  }

  Status<void> Skip(std::size_t padding_bytes) {
    if (padding_bytes > (size_ - index_))
      return ErrorStatus(ENOBUFS);

    auto status = reader_->Skip(padding_bytes);
    if (!status)
      return status;

    index_ += padding_bytes;
    return {};
  }

  // Skips any bytes remaining in the limit set at construction.
  Status<void> ReadPadding() {
    const std::size_t padding_bytes = size_ - index_;
    auto status = reader_->Skip(padding_bytes);
    if (!status)
      return status;

    index_ += padding_bytes;
    return {};
  }

  template <typename HandleType>
  Status<HandleType> GetHandle(HandleReference handle_reference) {
    return reader_->GetHandle(handle_reference);
  }

  bool empty() const { return index_ == size_; }

  std::size_t size() const { return index_; }
  std::size_t capacity() const { return size_; }

 private:
  Reader* reader_{nullptr};
  std::size_t size_{0};
  std::size_t index_{0};
};

}  // namespace nop

#endif  // LIBNOP_INCLUDE_NOP_UTILITY_BOUNDED_READER_H_