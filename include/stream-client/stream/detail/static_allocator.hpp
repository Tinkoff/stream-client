#pragma once

#include <cstddef>
#include <new>

namespace stream_client {
namespace stream {
namespace detail {

class static_pool
{
public:
    static static_pool& construct(std::size_t size)
    {
        auto p = new char[sizeof(static_pool) + size];
        return *(::new (p) static_pool{size});
    }

    static_pool& share()
    {
        ++refs_;
        return *this;
    }

    void destroy()
    {
        if (refs_--) {
            return;
        }
        this->~static_pool();
        delete[] reinterpret_cast<char*>(this);
    }

    void* alloc(std::size_t n)
    {
        auto last = p_ + n;
        if (last >= end()) {
            throw std::bad_alloc{};
        }

        ++count_;
        auto p = p_;
        p_ = last;
        return p;
    }

    void dealloc()
    {
        if (--count_) {
            return;
        }

        p_ = reinterpret_cast<char*>(this + 1);
    }

private:
    char* end()
    {
        return reinterpret_cast<char*>(this + 1) + size_;
    }

    explicit static_pool(std::size_t size)
        : size_(size)
        , p_(reinterpret_cast<char*>(this + 1))
    {
    }

    std::size_t size_;
    std::size_t refs_ = 1;
    std::size_t count_ = 0;
    char* p_;
};

/** A non-thread-safe static allocator.

    This allocator obtains memory from a pre-allocated memory block
    of a given size. It does nothing in deallocate until all
    previously allocated blocks are deallocated, upon which it
    resets the internal memory block for re-use.

    To use this allocator declare an instance persistent to the
    connection or session, and construct with the block size.
    A good rule of thumb is 20% more than the maximum allowed
    header size. For example if the application only allows up
    to an 8,000 byte header, the block size could be 9,600.

    Then, for every instance of `message` construct the header
    with a copy of the previously declared allocator instance.
*/
template <typename T>
class static_allocator
{
public:
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using const_pointer = const T*;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    static_allocator() = default;

    explicit static_allocator(std::size_t size)
        : pool_(&static_pool::construct(size))
    {
    }

    static_allocator(const static_allocator& other)
        : pool_(&other.pool_->share())
    {
    }

    template <typename U>
    static_allocator(const static_allocator<U>& other)
        : pool_(&other.pool_->share())
    {
    }

    ~static_allocator()
    {
        pool_->destroy();
    }

    value_type* allocate(size_type n)
    {
        return static_cast<value_type*>(pool_->alloc(n * sizeof(T)));
    }

    void deallocate(value_type*, size_type)
    {
        pool_->dealloc();
    }

    void deallocate(value_type*)
    {
        pool_->dealloc();
    }

    template <typename U>
    friend bool operator==(const static_allocator& lhs, const static_allocator<U>& rhs)
    {
        return &lhs.pool_ == &rhs.pool_;
    }

    template <typename U>
    friend bool operator!=(const static_allocator& lhs, const static_allocator<U>& rhs)
    {
        return !(lhs == rhs);
    }

private:
    template <typename U>
    friend class static_allocator;

    static_pool* pool_;
};

} // namespace detail
} // namespace stream
} // namespace stream_client
