//MIT License
//
//Copyright (c) 2017 Mindaugas Vinkelis
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#ifndef INCLUDE_BITSERY_DETAILS_COMMON_H_
#define INCLUDE_BITSERY_DETAILS_COMMON_H_


#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace bitsery
{

namespace archive
{
namespace detail
{

/**
 * Map any sequence of types to void.
 */
template <typename...>
using void_t = void;

/**
 * Tests if all conditions are true, empty means true.
 * Example:
 * ~~~
 * all_of<true, false, true>::value == false
 * all_of<true, true>::value == true
 * all_of<false, false>::value == false
 * all_of<>::value == true
 * ~~~
 */
template <bool... bConditions>
struct all_of : std::true_type {};

template <bool... bConditions>
struct all_of<false, bConditions...> : std::false_type {};

template <bool... bConditions>
struct all_of<true, bConditions...> : all_of<bConditions...> {};

template <>
struct all_of<true> : std::true_type {};

/**
 * Remove const of container value_type
 */
template <typename Container, typename = void>
struct container_nonconst_value_type
{
    using type = std::remove_const_t<typename Container::value_type>;
};

/**
 * Same as above, except in case of std::map and std::unordered_map, and similar,
 * we also need to remove the const of the key type.
 */
template <template <typename...> class Container, typename KeyType, typename MappedType, typename... ExtraTypes>
struct container_nonconst_value_type<Container<KeyType, MappedType, ExtraTypes...>,
    void_t<
        // Require existence of key_type.
        typename Container<KeyType, MappedType, ExtraTypes...>::key_type,

        // Require existence of mapped_type.
        typename Container<KeyType, MappedType, ExtraTypes...>::mapped_type,

        // Require that the value type is a pair of const KeyType and MappedType.
        std::enable_if_t<std::is_same<
            std::pair<const KeyType, MappedType>,
            typename Container<KeyType, MappedType, ExtraTypes...>::value_type
        >::value>
    >
>
{
    using type = std::pair<KeyType, MappedType>;
};

/**
 * Alias to the above.
 */
template <typename Container>
using container_nonconst_value_type_t = typename container_nonconst_value_type<Container>::type;

/**
 * The serializer exception template.
 */
template <typename Base, std::size_t Id>
class exception : public Base
{
public:
    /**
     * Use the constructors from the base class.
     */
    using Base::Base;

    /**
     * Constructs an exception object with empty message.
     */
    exception() :
        Base(std::string{})
    {
    }
};

/**
 * A no operation, single byte has same representation in little/big endian.
 */
inline constexpr std::uint8_t swap_byte_order(std::uint8_t value) noexcept
{
    return value;
}

/**
 * Swaps the byte order of a given integer.
 */
inline constexpr std::uint16_t swap_byte_order(std::uint16_t value) noexcept
{
    return (std::uint16_t(swap_byte_order(std::uint8_t(value))) << 8) |
        (swap_byte_order(std::uint8_t(value >> 8)));
}

/**
 * Swaps the byte order of a given integer.
 */
inline constexpr std::uint32_t swap_byte_order(std::uint32_t value) noexcept
{
    return (std::uint32_t(swap_byte_order(std::uint16_t(value))) << 16) |
        (swap_byte_order(std::uint16_t(value >> 16)));
}

/**
 * Swaps the byte order of a given integer.
 */
inline constexpr std::uint64_t swap_byte_order(std::uint64_t value) noexcept
{
    return (std::uint64_t(swap_byte_order(std::uint32_t(value))) << 32) |
        (swap_byte_order(std::uint32_t(value >> 32)));
}

/**
 * Rotates the given number left by count bits.
 */
template <typename Integer>
constexpr auto rotate_left(Integer number, std::size_t count)
{
    return (number << count) | (number >> ((sizeof(number) * 8) - count));
}

/**
 * Checks if has 'data()' member function.
 */
template <typename Type, typename = void>
struct has_data_member_function : std::false_type {};

/**
 * Checks if has 'data()' member function.
 */
template <typename Type>
struct has_data_member_function<Type, void_t<decltype(std::declval<Type &>().data())>> : std::true_type {};

} // detail

/**
 * @name Exceptions
 * @{
 */
using out_of_range = detail::exception<std::out_of_range, 0>;
using undeclared_polymorphic_type_error = detail::exception<std::runtime_error, 1>;
using attempt_to_serialize_null_pointer_error = detail::exception<std::logic_error, 2>;
using polymorphic_type_mismatch_error = detail::exception<std::runtime_error, 3>;
/**
 * @}
 */

/**
 * If C++17 or greater, use shared mutex, else, use shared timed mutex.
 */
#if __cplusplus > 201402L
/**
 * The shared mutex type, defined to shared mutex when available.
 */
using shared_mutex = std::shared_mutex;
#else
/**
 * The shared mutex type, defined to shared timed mutex when shared mutex is not available.
 */
using shared_mutex = std::shared_timed_mutex;
#endif

/**
 * The size type of the serializer.
 * It is used to indicate the size for containers.
 */
using size_type = std::uint32_t;

/**
 * The serialization id type,
 */
using id_type = std::uint64_t;

} // serializer
} //bitsery

#endif /* INCLUDE_BITSERY_DETAILS_COMMON_H_ */
