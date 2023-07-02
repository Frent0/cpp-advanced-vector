#pragma once

#include <cassert>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <new>
#include <utility>
#include <iterator>

template <typename T>
class RawMemory {
public:

    RawMemory() = default;

    explicit RawMemory(size_t capacity) :
        buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(RawMemory&& other) noexcept :
        buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0)) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;


    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            buffer_ = rhs.buffer_;
            capacity_ = rhs.capacity_;
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }


    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_); return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:

    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

};

template <typename T>
class Vector {
public:

    Vector() = default;

    explicit Vector(size_t size) :
        data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) :
        data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept :
        data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0)) {
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return size_ + data_.GetAddress();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return size_ + data_.GetAddress();
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator end() const noexcept {
        return cend();
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {

        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            if (new_size > data_.Capacity()) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args);

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args &&... args) {
        auto distance = std::distance(cbegin(), pos);
        if (size_ == Capacity()) {

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data.GetAddress() + distance) T(std::forward<Args>(args)...);

            if (size_ > 0) {
                if (cbegin() != pos) {
                    auto head_size = std::distance(cbegin(), pos);
                    try {
                        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                            std::uninitialized_move_n(begin(), head_size, new_data.GetAddress());
                        }
                        else {
                            std::uninitialized_copy_n(begin(), head_size, new_data.GetAddress());
                        }
                    }
                    catch (...) {
                        std::destroy_at(new_data.GetAddress() + distance);
                        throw;
                    }
                }

                if (cend() != pos) {
                    auto tail_size = std::distance(pos, cend());
                    try {
                        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                            std::uninitialized_move_n(begin() + distance, tail_size, new_data.GetAddress() + distance + 1);
                        }
                        else {
                            std::uninitialized_copy_n(begin() + distance, tail_size, new_data.GetAddress() + distance + 1);
                        }
                    }
                    catch (...) {
                        std::destroy_n(new_data.GetAddress(), distance);
                        throw;
                    }
                }
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else {
            auto pos_ = data_.GetAddress() + distance;
            if (size_ > 0) {
                T copy(std::forward<Args>(args)...);
                new(end()) T(std::move(*(end() - 1)));
                std::move_backward(pos_, end() - 1, end());
                *pos_ = std::forward<T>(copy);
            }
            else {
                new(pos_) T(std::forward<Args>(args)...);
            }
        }
        ++size_;

        return data_.GetAddress() + distance;
    }

    iterator Insert(const_iterator pos, const T& item) {
        return Emplace(pos, item);
    }

    iterator Insert(const_iterator pos, T&& item) {
        return Emplace(pos, std::move(item));
    }

    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());

        size_t position = pos - begin();
        std::move(begin() + position + 1, end(), begin() + position);
        PopBack();

        return (begin() + position);
    }

    template <typename Arg>
    void PushBack(Arg&& arg);

    void PopBack() {
        assert(size_);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    Vector& operator=(const Vector& other) {

        if (this != &other) {

            if (other.size_ <= data_.Capacity()) {

                if (size_ <= other.size_) {

                    std::copy(other.data_.GetAddress(),
                        other.data_.GetAddress() + size_,
                        data_.GetAddress());

                    std::uninitialized_copy_n(other.data_.GetAddress() + size_,
                        other.size_ - size_,
                        data_.GetAddress() + size_);
                }
                else {

                    std::copy(other.data_.GetAddress(),
                        other.data_.GetAddress() + other.size_,
                        data_.GetAddress());

                    std::destroy_n(data_.GetAddress() + other.size_,
                        size_ - other.size_);
                }

                size_ = other.size_;

            }
            else {
                Vector other_copy(other);
                Swap(other_copy);
            }
        }

        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        Swap(other); return *this;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_); return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};

template <typename T>
template <typename Arg>
void Vector<T>::PushBack(Arg&& arg) {
    EmplaceBack(std::forward<Arg>(arg));
}

template <typename T>
template <typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    if (size_ == data_.Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new(new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            try {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_at(new_data.GetAddress() + size_);
            }
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }
    else {
        new(data_.GetAddress() + size_) T(std::forward<Args>(args)...);
    }
    return data_[size_++];
}
