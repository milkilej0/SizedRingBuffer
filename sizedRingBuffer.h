#ifndef SIZED_RING_BUFFER_H
#define SIZED_RING_BUFFER_H

#if defined(__GNUC__) || defined(__clang__)
#  define SRB_ALWAYS_INLINE [[gnu::always_inline]] inline
#  define SRB_HOT [[gnu::hot]]
#  define SRB_COLD [[gnu::cold]]
#elif defined(_MSC_VER)
#  define SRB_ATTRIBUTE __forceinline
#  define SRB_HOT
#  define SRB_COLD
#else
#  define SRB_ATTRIBUTE inline
#  define SRB_HOT
#  define SRB_COLD
#endif

// A ring buffer with dynamic size (0 <= size <= N)
// Significantly faster (~2.5/1.2 times faster push/pop respectively)
template<typename T, uint8_t B>
struct alignas(1 << (std::min<uint64_t>(B, 12) / 2)) sizedRingBuffer
{
    static constexpr uint64_t N = 1ull << B;
    static constexpr uint64_t modMask = N - 1;
    #define mod(i) (i & modMask)

private:
    T data_[N] = {}; // The ring buffer
    uint64_t start_; // Index of the start of the list (0 <= start_ < N)
    uint64_t size_; // Number of valid elements after start_ (0 <= size_ <= N)

public:
    constexpr sizedRingBuffer() : start_(0), size_(0) {}
    explicit constexpr sizedRingBuffer(const uint32_t s) : start_(0), size_(s) {}
    SRB_COLD constexpr sizedRingBuffer(const std::initializer_list<T> init) noexcept : start_(0), size_(0) { for (auto&& e : init) push(e); }

    SRB_ALWAYS_INLINE constexpr T& operator[](const uint64_t i) { return data_[mod(start_ + i)]; }
    SRB_ALWAYS_INLINE constexpr const T& operator[](const uint64_t i) const { return data_[mod(start_ + i)]; }

    [[nodiscard]] SRB_ALWAYS_INLINE constexpr uint32_t size() const noexcept { return size_; }

    SRB_ALWAYS_INLINE constexpr void push(const T& t) noexcept
    {
        if (size_ == N) [[unlikely]]
        {
            data_[start_] = t;
            start_ = mod(start_ + 1ull);
        }
        else  [[likely]]
            data_[mod(start_ + size_++)] = t;
    }
    SRB_COLD constexpr void push(const std::initializer_list<T> init) noexcept { for (auto&& e : init) push(e); }

    SRB_HOT constexpr void pop(const uint64_t i) noexcept
    {
        assert(i < size_);

        if (--size_) [[likely]]
        {
            if ((size_ + 1) | i)
            {
                const int64_t tail = N - (start_ + 1ull), head = i - (tail + 1);
                T* startPtr = data_ + start_;

                if (head < 0) [[likely]]
                    std::memmove( startPtr + 1ull, startPtr, i * sizeof(T) );
                else [[unlikely]]
                {
                    if (head > 0)
                        std::memmove( data_ + 1ull, data_, head * sizeof(T) );
                    else
                        std::memmove( startPtr + 1ull,  startPtr, tail * sizeof(T) );

                    data_[0] = data_[N - 1ull];
                }

                start_ = mod(start_ + 1ull);
            }
            else
            {
                const uint64_t phys  = mod(start_ + i);    // physical index of element i
                const int64_t temp = size_ - i, head = temp + phys - N;
                T* physPtr = data_ + phys;

                if (head < 0) [[likely]]
                    std::memmove( physPtr, physPtr + 1ull, temp * sizeof(T) );
                else [[unlikely]]
                {
                    if (const int64_t tail = N - (phys + 1ull); tail > 0)
                        std::memmove( physPtr, physPtr + 1ull, tail * sizeof(T) );

                    T tmp = data_[0];
                    if (head > 0)
                        std::memmove( data_, data_ + 1ull, head * sizeof(T) );
                    data_[N - 1ull] = tmp;
                }
            }
        }
    }

    SRB_ALWAYS_INLINE constexpr void popFirst() { assert(size_ != 0); --size_; start_ = mod(start_ + 1ull); }
    SRB_ALWAYS_INLINE constexpr void popLast() { assert(size_ != 0); --size_; }

    [[nodiscard]] SRB_ALWAYS_INLINE constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] SRB_ALWAYS_INLINE constexpr bool full() const noexcept { return size_ == N; }

    SRB_ALWAYS_INLINE constexpr void clear() noexcept { size_ = 0; }

    SRB_COLD friend std::ostream& operator<<(std::ostream& os, const sizedRingBuffer<T, N>& b)
    {
        os << "SRB { ";
        for (auto el : b)
            os << el << ", ";
        return os << "}";
    }

    // Extracts the raw data to a byte string
    [[nodiscard]] SRB_COLD constexpr std::string extract() const noexcept
    {
        std::string result(8 + sizeof(data_), '\0');
        *reinterpret_cast<uint64_t*>(result.data()) = size_;

        const uint64_t stride = std::min(size_, N - start_) * sizeof(T);
        memcpy(result.data() + 8ull, data_ + start_, stride);
        memcpy(result.data() + 8ull + stride, data_, size_ * sizeof(T) - stride);

        return result;
    }

    // Reconstructs the sizedRingBuffer from a byte string
    [[nodiscard]] SRB_COLD static constexpr sizedRingBuffer reconstruct(std::string s)
    {
        sizedRingBuffer result;
        result.size_ = *reinterpret_cast<uint32_t*>(s.data());
        memcpy(result.data_, s.data() + 4, result.size_);
        return result;
    }

    [[nodiscard]] SRB_COLD constexpr sizedRingBuffer optimise() noexcept
    {
        sizedRingBuffer result;
        result.start_ = 0;

        const uint64_t stride = std::min(size_, N - start_) * sizeof(T);
        memcpy(result.data_, data_ + start_, stride);
        memcpy(static_cast<uint8_t *>(result.data_) + stride, data_, size_ * sizeof(T) - stride);

        return result;
    }

    #undef mod
};

#endif // SIZED_RING_BUFFER_H