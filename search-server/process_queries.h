#pragma once

#include <string>
#include <vector>
#include <utility>

#include "search_server.h"

std::vector<std::vector<Document>>
ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);


template <typename OuterContainer>
class Iterable2D {

public:
    using outer_container_type = OuterContainer;
    using inner_container_type = typename OuterContainer::value_type;
    using value_type = typename inner_container_type::value_type;
    using reference = value_type&;
    using const_reference = const value_type&;

private:

    template <typename ValueType>
    class BasicIterator {

        friend class Iterable2D; // give access to private constructor

        using outer_iterator_type = typename outer_container_type::iterator;
        using inner_iterator_type = typename inner_container_type::iterator;

        BasicIterator(Iterable2D& iterable,
            outer_iterator_type outer_iterator,
            inner_iterator_type inner_iterator) :
                iterable2d_(iterable),
                outer_iterator_(outer_iterator),
                inner_iterator_(inner_iterator) { }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Iterable2D::value_type;
        using difference_type = typename inner_container_type::difference_type;
        using pointer = ValueType*;
        using reference = ValueType&;

        BasicIterator() = default;
        BasicIterator(const BasicIterator& other) = default;
        BasicIterator& operator=(const BasicIterator& rhs) = default;

        [[nodiscard]] bool operator==(const BasicIterator<const value_type>& rhs) const noexcept {
            return outer_iterator_ == rhs.outer_iterator_
                    && (outer_iterator_ == iterable2d_.outer_.cend() || inner_iterator_ == rhs.inner_iterator_);
        }

        [[nodiscard]] bool operator!=(const BasicIterator<const value_type>& rhs) const noexcept {
            return !(*this == rhs);
        }

        [[nodiscard]] bool operator==(const BasicIterator<value_type>& rhs) const noexcept {
            return outer_iterator_ == rhs.outer_iterator_
                    && (outer_iterator_ == iterable2d_.outer_.cend() || inner_iterator_ == rhs.inner_iterator_);
        }

        [[nodiscard]] bool operator!=(const BasicIterator<value_type>& rhs) const noexcept {
            return !(*this == rhs);
        }

        BasicIterator& operator++() noexcept {
            ++inner_iterator_;
            if (inner_iterator_ == outer_iterator_->end()) {
                ++outer_iterator_;
                inner_iterator_ = outer_iterator_->begin();
            }
            return *this;
        }

        BasicIterator operator++(int) noexcept {
            auto old = *this;
            ++(*this);
            return old;
        }

        [[nodiscard]] reference operator*() const noexcept {
            return *inner_iterator_;
        }

        [[nodiscard]] pointer operator->() const noexcept {
            return &inner_iterator_;
        }

    private:
        
        Iterable2D& iterable2d_;
        outer_iterator_type outer_iterator_;
        inner_iterator_type inner_iterator_;
    };

public:

    Iterable2D() = delete;
    Iterable2D(const Iterable2D&) = delete;
    Iterable2D& operator=(const Iterable2D&) = delete;

    Iterable2D(outer_container_type&& outer) noexcept : outer_(std::move(outer)) {}

    Iterable2D(Iterable2D&& other) noexcept = default;
    Iterable2D& operator=(Iterable2D&& other) noexcept = default;

    outer_container_type& GetOuter() {
        return outer_;
    }

    using Iterator = BasicIterator<value_type>;

    [[nodiscard]]
    Iterator begin() noexcept {
        return Iterator{*this, outer_.begin(), outer_.front().begin()};
    }

    [[nodiscard]]
    Iterator end() noexcept {
        return Iterator{*this, outer_.end(), static_cast<typename inner_container_type::iterator>(nullptr)};
    }

private:

    OuterContainer outer_;
};

Iterable2D<std::vector<std::vector<Document>>>
ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);

