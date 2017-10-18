//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// Contains helper classes used in both defining the CNTKLibrary.h APIs and internal code.
//

#pragma once

#include <vector>
#include <list>
#include <forward_list>
#include <deque>
#include <set>
#include <iterator>
#include <utility> // std::forward

namespace CNTK
{

///
/// Represents a slice view onto a container. Meant for use with std::vector.
/// A future C++ standard may have std::span, which we hope to replace this with in the future.
///
template<typename IteratorType>
class Span
{
protected:
    IteratorType beginIter, endIter;
    typedef typename std::iterator_traits<IteratorType>::value_type T;
    typedef typename std::remove_reference<T>::type TValue;
    typedef typename std::remove_cv<TValue>::type TValueNonConst;
public:
    typedef TValue value_type;
    typedef IteratorType const_iterator; // TODO: some template magic to define the correct one
    typedef IteratorType iterator;
    // can be instantiated from any vector
    // We don't preserve const-ness for this class. Wait for a proper STL version of this :)
    Span(const IteratorType& beginIter, const IteratorType& endIter) : beginIter(beginIter), endIter(endIter) { }
    Span(const IteratorType& beginIter, size_t len) : beginIter(beginIter), endIter(beginIter + len) { }
    // Cannot be copied. Pass this as a reference only, to avoid ambiguity.
    Span(const Span&) = delete; void operator=(const Span&) = delete;
    //Span& operator=(Span&& other) { beginIter = std::move(other.beginIter); endIter = std::move(other.endIter); return *this; }
    Span(Span&& other) : beginIter(std::move(other.beginIter)), endIter(std::move(other.endIter)) { }
    // the collection interface
    const IteratorType& begin()       const { return beginIter; }
    const IteratorType& begin()             { return beginIter; }
    const IteratorType& end()         const { return endIter; }
    const IteratorType& end()               { return endIter; }
    const IteratorType& cbegin()      const { return begin(); }
    const IteratorType& cbegin()            { return begin(); }
    const IteratorType& cend()        const { return end(); }
    const IteratorType& cend()              { return end(); }
    const T* data()                   const { return begin(); }
    T*       data()                         { return begin(); }
    const T& front()                  const { return *begin(); }
    T&       front()                        { return *begin(); }
    const T& back()                   const { return *(end() - 1); }
    T&       back()                         { return *(end() - 1); }
    size_t   size()                   const { return cend() - cbegin(); }
    bool     empty()                  const { return cend() == cbegin(); }
    const T& at(size_t index)         const { return *(beginIter + index); }
    T&       at(size_t index)               { return *(beginIter + index); }
    const T& operator[](size_t index) const { return at(index); }
    T&       operator[](size_t index)       { return at(index); }
    // construct certain collection types directly
    operator std::vector      <TValueNonConst>() const { return std::vector      <TValueNonConst>(cbegin(), cend()); }
    operator std::list        <TValueNonConst>() const { return std::list        <TValueNonConst>(cbegin(), cend()); }
    operator std::forward_list<TValueNonConst>() const { return std::forward_list<TValueNonConst>(cbegin(), cend()); }
    operator std::deque       <TValueNonConst>() const { return std::deque       <TValueNonConst>(cbegin(), cend()); }
    operator std::set         <TValueNonConst>() const { return std::set         <TValueNonConst>(cbegin(), cend()); }
};
// MakeSpan(collection[, beginIndex[, endIndex]])
template<typename CollectionType>
auto MakeSpan(CollectionType& collection, size_t beginIndex = 0) { return Span<typename CollectionType::iterator>(collection.begin() + beginIndex, collection.end()); }
template<typename CollectionType>
auto MakeSpan(const CollectionType& collection, size_t beginIndex = 0) { return Span<typename CollectionType::const_iterator>(collection.cbegin() + beginIndex, collection.cend()); }
// TODO: Decide what end=0 means.
template<typename CollectionType, typename EndIndexType>
auto MakeSpan(CollectionType& collection, size_t beginIndex, EndIndexType endIndex) { return Span<typename CollectionType::iterator>(collection.begin() + beginIndex, (endIndex >= 0 ? collection.begin() : collection.end()) + endIndex); }
template<typename CollectionType, typename EndIndexType>
auto MakeSpan(const CollectionType& collection, size_t beginIndex, EndIndexType endIndex) { return Span<typename CollectionType::const_iterator>(collection.cbegin() + beginIndex, (endIndex >= 0 ? collection.begin() : collection.end()) + endIndex); }

///
/// A collection wrapper class that performs a map ("transform") operation given a lambda.
///
template<typename CollectionType, typename Lambda>
class TransformingSpan
{
    typedef typename CollectionType::value_type T;
    typedef typename std::conditional<std::is_const<CollectionType>::value, typename CollectionType::const_iterator, typename CollectionType::iterator>::type CollectionIterator; // TODO: template magic to make this const only if the input is const, otherwise ::iterator
    typedef typename std::iterator_traits<CollectionIterator>::iterator_category CollectionIteratorCategory;
    typedef decltype(std::declval<Lambda>()(std::forward<T>(std::declval<T>()))) TLambda; // type of result of lambda call
    typedef typename std::remove_reference<TLambda>::type TValue;
    typedef typename std::remove_cv<TValue>::type TValueNonConst;
    CollectionIterator beginIter, endIter;
    const Lambda& lambda;
    // transforming iterator
    class Iterator : public std::iterator<CollectionIteratorCategory, TValue>
    {
        const Lambda& lambda;
        CollectionIterator argIter;
    public:
        Iterator(const CollectionIterator& argIter, const Lambda& lambda) : argIter(argIter), lambda(lambda) { }
        Iterator operator++() { auto cur = *this; argIter++; return cur; }
        Iterator operator++(int) { argIter++; return *this; }
        TLambda operator*() const { return lambda(std::move(*argIter)); }
        auto operator->() const { return &operator*(); }
        bool operator==(const Iterator& other) const { return argIter == other.argIter; }
        bool operator!=(const Iterator& other) const { return argIter != other.argIter; }
        Iterator operator+(difference_type offset) const { return Iterator(argIter + offset, lambda); }
        Iterator operator-(difference_type offset) const { return Iterator(argIter - offset, lambda); }
        difference_type operator-(const Iterator& other) const { return argIter - other.argIter; }
    };
public:
    typedef TLambda value_type;
    // note: constness must be contained in CollectionType
    TransformingSpan(CollectionType& collection, const Lambda& lambda) : beginIter(collection.begin()), endIter(collection.end()), lambda(lambda) { }
    typedef Iterator const_iterator;
    typedef Iterator iterator;
    const_iterator cbegin() const { return const_iterator(beginIter, lambda); }
    const_iterator cend()   const { return const_iterator(endIter  , lambda); }
    const_iterator begin()  const { return cbegin(); }
    const_iterator end()    const { return cend();   }
    iterator       begin()        { return iterator(beginIter, lambda); }
    iterator       end()          { return iterator(endIter,   lambda); }
    size_t         size()   const { return (size_t)(endIter - beginIter); }
    bool           empty()  const { return endIter == beginIter; }
    // construct certain collection types directly
    operator std::vector      <TValueNonConst>() const { return std::vector      <TValueNonConst>(cbegin(), cend()); } // note: don't call as_vector etc., will not be inlined! in VS 2015!
    operator std::list        <TValueNonConst>() const { return std::list        <TValueNonConst>(cbegin(), cend()); }
    operator std::forward_list<TValueNonConst>() const { return std::forward_list<TValueNonConst>(cbegin(), cend()); }
    operator std::deque       <TValueNonConst>() const { return std::deque       <TValueNonConst>(cbegin(), cend()); }
    operator std::set         <TValueNonConst>() const { return std::set         <TValueNonConst>(cbegin(), cend()); }
};
// main entry point
// E.g. call as Transform(collection, lambda1, lambda2, ...).as_vector();
template<typename CollectionType, typename Lambda>
static inline const auto Transform(const CollectionType& collection, const Lambda& lambda) { return TransformingSpan<CollectionType const, Lambda>(collection, lambda); }
template<typename CollectionType, typename Lambda, typename ...MoreLambdas>
static inline const auto Transform(const CollectionType& collection, const Lambda& lambda, MoreLambdas&& ...moreLambdas) { return Transform(TransformingSpan<CollectionType const, Lambda>(collection, lambda), std::forward<MoreLambdas>(moreLambdas)...); }

template<typename CollectionType, typename Lambda>
static inline auto Transform(CollectionType& collection, const Lambda& lambda) { return TransformingSpan<CollectionType, Lambda>(collection, lambda); }
template<typename CollectionType, typename Lambda, typename ...MoreLambdas>
static inline auto Transform(CollectionType& collection, const Lambda& lambda, MoreLambdas&& ...moreLambdas) { return Transform(TransformingSpan<CollectionType, Lambda>(collection, lambda), std::forward<MoreLambdas>(moreLambdas)...); }

///
/// Implement a range like Python's range.
/// Can be used with variable or constant bounds (use IntConstant<val> as the second and third type args).
///
template<int val>
struct IntConstant
{
    static constexpr int x = val;
    constexpr operator int() const { return x; }
};
template<typename T, typename Tbegin = const T, typename Tend = const T>
class NumericRangeSpan
{
    static const T stepValue = (T)1; // for now. TODO: apply the IntConst trick here as well.
    Tbegin beginValue;
    Tend endValue;
    typedef typename std::remove_reference<T>::type TValue;
    typedef typename std::remove_cv<TValue>::type TValueNonConst;
public:
    typedef T value_type;
    NumericRangeSpan(const T& beginValue, const T& endValue/*, const T& stepValue = (const T&)1*/) : beginValue(beginValue), endValue(endValue)/*, stepValue(stepValue)*/ { }
    NumericRangeSpan(const T& endValue) : NumericRangeSpan(0, endValue) { }
    NumericRangeSpan() { }
    // iterator
    class const_iterator : public std::iterator<std::random_access_iterator_tag, TValue>
    {
        T value/*, stepValue*/;
    public:
        const_iterator(const T& value/*, const T& stepValue*/) : value(value)/*,stepValue(stepValue)*/ { }
        const_iterator operator++() { auto cur = *this; value += stepValue; return cur; }
        const_iterator operator++(int) { value += stepValue; return *this; }
        T operator*() const { return value; }
        auto operator->() const { return &operator*(); } // (who knows whether this is defined for the type)
        bool operator==(const const_iterator& other) const { return value == other.value; }
        bool operator!=(const const_iterator& other) const { return value != other.value; }
        const_iterator operator+(difference_type offset) const { return const_iterator(value + offset * stepValue, stepValue); }
        const_iterator operator-(difference_type offset) const { return const_iterator(value - offset * stepValue, stepValue); }
        difference_type operator-(const const_iterator& other) const { return ((difference_type)value - (difference_type)other.value) / stepValue; }
    };
    typedef const_iterator iterator; // in case it gets instantiated non-const. Still cannot modify it of course.
    const_iterator cbegin() const { return const_iterator(beginValue); }
    const_iterator cend()   const { return const_iterator(endValue);   }
    const_iterator begin()  const { return cbegin(); }
    const_iterator end()    const { return cend();   }
    size_t         size()   const { return ((difference_type)endValue - (difference_type)beginValue) / stepValue; }
    bool           empty()  const { return endValue == beginValue; }
    // construct certain collection types directly, to support TransformToVector() etc.
    operator std::vector      <TValueNonConst>() const { return std::vector      <TValueNonConst>(cbegin(), cend()); } // note: don't call as_vector etc., will not be inlined! in VS 2015!
    operator std::list        <TValueNonConst>() const { return std::list        <TValueNonConst>(cbegin(), cend()); }
    operator std::forward_list<TValueNonConst>() const { return std::forward_list<TValueNonConst>(cbegin(), cend()); }
    operator std::deque       <TValueNonConst>() const { return std::deque       <TValueNonConst>(cbegin(), cend()); }
    operator std::set         <TValueNonConst>() const { return std::set         <TValueNonConst>(cbegin(), cend()); }
};

///
/// Assembly-optimized constructors for creating 1- and 2-element std::vector.
/// Note that the embedded iterators only work for std::vector, since operator!= and operator-
/// blindly assume that 'other' is end().
///
template<typename T>
static inline std::vector<T> MakeTwoElementVector(const T& a, const T& b)
{
    class TwoElementSpanIterator : public std::iterator<std::random_access_iterator_tag, T>
    {
        const T* x[2];
    public:
        TwoElementSpanIterator() { } // sentinel
        TwoElementSpanIterator(const T& a, const T& b) { x[0] = &a; x[1] = &b; }
        void operator++() { x[0] = x[1]; x[1] = nullptr; }
        const T& operator*() const { return *x[0]; }
        bool operator!=(const TwoElementSpanIterator&) const { return x[0] != nullptr; }
        constexpr difference_type operator-(const TwoElementSpanIterator&) const { return 2; }
    };
    return vector<T>(TwoElementSpanIterator(a, b), TwoElementSpanIterator());
}
template<typename T>
static inline std::vector<T> MakeOneElementVector(const T& a)
{
    class OneElementSpanIterator : public std::iterator<std::random_access_iterator_tag, T>
    {
        const T* x;
    public:
        OneElementSpanIterator() { } // sentinel
        OneElementSpanIterator(const T& a) : x(&a) { }
        void operator++() { x = nullptr; }
        const T& operator*() const { return *x; }
        bool operator!=(const OneElementSpanIterator&) const { return x != nullptr; }
        constexpr difference_type operator-(const OneElementSpanIterator&) const { return 1; }
    };
    return vector<T>(OneElementSpanIterator(a), OneElementSpanIterator());
}

///
/// Helpers to construct the standard STL from the above.
///
template<typename Container>
static inline auto MakeVector(const Container& container) { return std::vector<Container::value_type>(container.cbegin(), container.cend()); }
template<typename Container>
static inline auto MakeList(const Container& container) { return std::list<Container::value_type>(container.cbegin(), container.cend()); }
template<typename Container>
static inline auto MakeFowardList(const Container& container) { return std::forward_list<Container::value_type>(container.cbegin(), container.cend()); }
template<typename Container>
static inline auto MakeDeque(const Container& container) { return std::deque<Container::value_type>(container.cbegin(), container.cend()); }
template<typename Container>
static inline auto MakeSet(const Container& container) { return std::set<Container::value_type>(container.cbegin(), container.cend()); }

///
/// Class that stores a vector with "small-vector optimization," that is, if it has N or less elements,
/// they are stored in the object itself, without malloc(), while more elements use a std::vector.
///
template<typename T, size_t N>
class FixedVectorWithBuffer : public Span<T*>
{
    typedef Span<T*> Base;
    union U
    {
        T fixedBuffer[N]; // stored inside a union so that we get away without automatic construction yet correct alignment
        U() {}  // C++ requires these in order to be happy
        ~U() {}
    } u;
public:
    typedef T value_type;
    // not copyable
    FixedVectorWithBuffer(const FixedVectorWithBuffer&) = delete; void operator=(const FixedVectorWithBuffer&) = delete;
    // short-circuit constructors that construct from up to 2 arguments which are taken ownership of
    FixedVectorWithBuffer()             : Base(u.fixedBuffer, (size_t)0) {}
    FixedVectorWithBuffer(T&& a)        : Base(u.fixedBuffer, 1) { new (&u.fixedBuffer[0]) T(std::move(a)); }
    FixedVectorWithBuffer(T&& a, T&& b) : Base(u.fixedBuffer, 2) { new (&u.fixedBuffer[0]) T(std::move(a)); new (&u.fixedBuffer[1]) T(std::move(b)); } // BUGBUG: This version should only be defined if N > 1.
    FixedVectorWithBuffer(const T& a)   : Base(u.fixedBuffer, 1) { new (&u.fixedBuffer[0]) T(a); }
    FixedVectorWithBuffer(const T& a, const T& b) : Base(u.fixedBuffer, 2)
    {
        new (&u.fixedBuffer[0]) T(a);
        try { new (&u.fixedBuffer[1]) T(b); } // if second one fails, we must clean up the first one
        catch (...) { u.fixedBuffer[0].~T(); throw; }
    } // BUGBUG: This version should only be defined if N > 1.
    // constructor from a vector
    // This constructor steals all elements out from the passed vector, but not the vector's fixedBuffer itself.
    // This is meant for the use case where we want to avoid reallocation of the vector, while its members
    // are small movable objects that get created upon each use.
    template<typename Collection, typename = std::enable_if_t<!std::is_lvalue_reference_v<Collection&&>>> // [thanks to Billy O'Neal for the tip]
    FixedVectorWithBuffer(Collection&& other) :
        Base(other.size() > N ? (T*)malloc(other.size() * sizeof(T)) : &u.fixedBuffer[0], other.size())
    {
        auto* b = begin();
        const auto* e = end();
        auto otherIter = other.begin();
        for (auto* p = b; p != e; p++)
        {
            new (p) T(std::move(*otherIter)); // nothrow
            otherIter++;
        }
    }
    ~FixedVectorWithBuffer()
    {
        auto* b = begin();
        const auto* e = end();
        for (auto* p = b; p != e; p++)
            p->~T();
        if (e > b + N) // (write it this way to avoid the division)
            free(b);   // Span::beginIter holds the result of malloc()
    }
    FixedVectorWithBuffer& operator=(FixedVectorWithBuffer&& other)
    {
        this->~FixedVectorWithBuffer();
        new (this) FixedVectorWithBuffer(std::move(other));
        other.~FixedVectorWithBuffer(); // this destructs the freshly moved-out objects right away --TODO: do it inside the loop?
        new (&other) FixedVectorWithBuffer(); // this writes two pointers
        return *this;
    }
    // this is a common use case
    void assign(T&& a)
    {
        this->~FixedVectorWithBuffer();
        new (this) FixedVectorWithBuffer(std::move(a));
    }
};

} // namespace
