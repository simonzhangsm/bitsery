/*
 * access.h
 *
 *  Created on: Oct 8, 2018
 *      Author: simon
 */

#ifndef INCLUDE_BITSERY_DETAILS_ACCESS_H_
#define INCLUDE_BITSERY_DETAILS_ACCESS_H_

#include <memory>
#include <type_traits>
#include <polymorphic.h>

namespace bitsery
{

namespace archive
{

/**
 * This class grants the serializer access to the serialized types.
 */
class access
{
public:
    /**
     * Allows placement construction of types.
     */
    template <typename Item, typename... Arguments>
    static auto placement_new(void * pAddress, Arguments && ... arguments) noexcept(
        noexcept(Item(std::forward<Arguments>(arguments)...)))
    {
        return ::new (pAddress) Item(std::forward<Arguments>(arguments)...);
    }

    /**
     * Allows dynamic construction of types.
     * This overload is for non polymorphic serialization.
     */
    template <typename Item, typename... Arguments,
        typename = std::enable_if_t<!std::is_base_of<polymorphic, Item>::value>
    >
    static auto make_unique(Arguments && ... arguments)
    {
        // Construct the requested type, using new since constructor might be private.
        return std::unique_ptr<Item>(new Item(std::forward<Arguments>(arguments)...));
    }

    /**
     * Allows dynamic construction of types.
     * This overload is for polymorphic serialization.
     */
    template <typename Item, typename... Arguments,
        typename = std::enable_if_t<std::is_base_of<polymorphic, Item>::value>,
        typename = void
    >
    static auto make_unique(Arguments && ... arguments)
    {
        // We create a deleter that will delete using the base class polymorphic which, as we declared,
        // has a public virtual destructor.
        struct deleter
        {
            void operator()(Item * item) noexcept
            {
                delete static_cast<polymorphic *>(item);
            }
        };

        // Construct the requested type, using new since constructor might be private.
        return std::unique_ptr<Item, deleter>(new Item(std::forward<Arguments>(arguments)...));
    }

    /**
     * Allows destruction of types.
     */
    template <typename Item>
    static void destruct(Item & item) noexcept
    {
        item.~Item();
    }
}; // access

} // archive
} // bitsery

#endif /* INCLUDE_BITSERY_DETAILS_ACCESS_H_ */
