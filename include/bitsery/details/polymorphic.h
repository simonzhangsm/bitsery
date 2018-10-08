/*
 * polymorphic.h
 *
 *  Created on: Oct 8, 2018
 *      Author: simon
 */

#ifndef INCLUDE_BITSERY_DETAILS_POLYMORPHIC_H_
#define INCLUDE_BITSERY_DETAILS_POLYMORPHIC_H_

#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "common.h"

namespace bitsery
{

namespace archive
{

/**
 * The base class for polymorphic serialization.
 */
class polymorphic
{
public:
    /**
     * Pure virtual destructor, in order to become abstract
     * and make derived classes polymorphic.
     */
    virtual ~polymorphic() = 0;
};

/**
 * Default implementation for the destructor.
 */
inline polymorphic::~polymorphic() = default;

/**
 * Allow serialization with saving (output) archives, of objects held by reference,
 * that will be serialized as polymorphic, meaning, with leading polymorphic serialization id.
 */
template <typename Type>
class polymorphic_wrapper
{
public:
    static_assert(std::is_base_of<polymorphic, Type>::value,
            "The given type is not derived from polymorphic");

    /**
     * Constructs from the given object to be serialized as polymorphic.
     */
    explicit polymorphic_wrapper(const Type & object) noexcept :
        m_object(object)
    {
    }

    /**
     * Returns the object to be serialized as polymorphic.
     */
    const Type & operator*() const noexcept
    {
        return m_object;
    }

private:
    /**
     * The object to be serialized as polymorphic.
     */
    const Type & m_object;
}; // polymorphic_wrapper

/**
 * A facility to save object with leading polymorphic serialization id.
 */
template <typename Type>
auto as_polymorphic(const Type & object) noexcept
{
    return polymorphic_wrapper<Type>(object);
}


} // archive
} // bitsery

#endif /* INCLUDE_BITSERY_DETAILS_POLYMORPHIC_H_ */
