/*
 * binary.h
 *
 *  Created on: Oct 8, 2018
 *      Author: simon
 */

#ifndef INCLUDE_BITSERY_DETAILS_BINARY_H_
#define INCLUDE_BITSERY_DETAILS_BINARY_H_

#include <__config>
#include <type_traits>

#include "polymorphic.h"

namespace bitsery
{

namespace archive
{

/**
 * Enables serialization of arbitrary binary data.
 * Use only with care.
 */
template <typename Item>
class binary
{
public:
    /**
     * Constructs the binary wrapper from pointer and count of items.
     */
    binary(Item * items, size_type count) :
        m_items(items),
        m_count(count)
    {
    }

    /**
     * Returns a pointer to the first item.
     */
    Item * data() const noexcept
    {
        return m_items;
    }

    /**
     * Returns the size in bytes of the binary data.
     */
    size_type size_in_bytes() const noexcept
    {
        return m_count * sizeof(Item);
    }

    /**
     * Returns the count of items in the binary wrapper.
     */
    size_type count() const noexcept
    {
        return m_count;
    }

private:
    /**
     * Pointer to the items.
     */
    Item * m_items = nullptr;

    /**
     * The number of items.
     */
    size_type m_count = 0;
};

/**
 * Allows serialization as binary data.
 * Use only with care.
 */
template <typename Item>
binary<Item> as_binary(Item * item, size_type count)
{
    static_assert(std::is_trivially_copyable<Item>::value,
           "Must be trivially copyable");

    return { item, count };
}

/**
 * Allows serialization as binary data.
 * Use only with care.
 */
inline binary<unsigned char> as_binary(void * data, size_type size)
{
    return { static_cast<unsigned char *>(data), size };
}

/**
 * Allows serialization as binary data.
 */
inline binary<const unsigned char> as_binary(const void * data, size_type size)
{
    return { static_cast<const unsigned char *>(data), size };
}

} // archive
} // bitsery

#endif /* INCLUDE_BITSERY_DETAILS_BINARY_H_ */
