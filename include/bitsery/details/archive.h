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

#ifndef INCLUDE_BITSERY_DETAILS_ARCHIVE_H_
#define INCLUDE_BITSERY_DETAILS_ARCHIVE_H_

#include <stddef.h>
#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "access.h"
#include "binary.h"
#include "common.h"
#include "polymorphic.h"
#include "registry.h"


namespace bitsery
{

namespace archive
{

/**
 * This is the base archive of the serializer.
 * It enables saving and loading items into/from the archive, via operator().
 */
template <typename ArchiveType>
class archive
{
public:
    /**
     * The derived archive type.
     */
    using archive_type = ArchiveType;

    /**
     * Save/Load the given items into/from the archive.
     */
    template <typename... Items>
    void operator()(Items && ... items)
    {
        // Disallow serialization of pointer types.
        static_assert(detail::all_of<!std::is_pointer<std::remove_reference_t<Items>>::value...>::value,
            "Serialization of pointer types is not allowed");

        // Serialize the items.
        serialize_items(std::forward<Items>(items)...);
    }

protected:
    /**
     * Constructs the archive.
     */
    archive() = default;

    /**
     * Protected destructor to allow safe public inheritance.
     */
    ~archive() = default;

private:
    /**
     * Serialize the given items, one by one.
     */
    template <typename Item, typename... Items>
    void serialize_items(Item && first, Items && ... items)
    {
        // Invoke serialize_item the first item.
        serialize_item(std::forward<Item>(first));

        // Serialize the rest of the items.
        serialize_items(std::forward<Items>(items)...);
    }

    /**
     * Serializes zero items.
     */
    void serialize_items()
    {
    }

    /**
     * Serialize a single item.
     * This overload is for class type items with serialize method.
     */
    template <typename Item, typename...,
        typename = decltype(std::remove_reference_t<Item>::serialize(
            std::declval<archive_type &>(), std::declval<Item &>()))
    >
    void serialize_item(Item && item)
    {
        // Forward as lvalue.
        std::remove_reference_t<Item>::serialize(concrete_archive(), item);
    }

    /**
     * Serialize a single item.
     * This overload is for types with outer serialize method.
     */
    template <typename Item, typename...,
        typename = decltype(serialize(std::declval<archive_type &>(), std::declval<Item &>())),
        typename = void
    >
    void serialize_item(Item && item)
    {
        // Forward as lvalue.
        serialize(concrete_archive(), item);
    }

    /**
     * Serialize a single item.
     * This overload is for fundamental types.
     */
    template <typename Item, typename...,
        typename = std::enable_if_t<std::is_fundamental<std::remove_reference_t<Item>>::value>,
        typename = void, typename = void
    >
    void serialize_item(Item && item)
    {
        // Forward as lvalue.
        concrete_archive().serialize(item);
    }

    /**
     * Serialize a single item.
     * This overload is for enum classes.
     */
    template <typename Item, typename...,
        typename = std::enable_if_t<std::is_enum<std::remove_reference_t<Item>>::value>,
        typename = void, typename = void, typename = void
    >
    void serialize_item(Item && item)
    {
        // If the enum is const, we want the type to be a const type, else non-const.
        using integral_type = std::conditional_t<
            std::is_const<std::remove_reference_t<Item>>::value,
            const std::underlying_type_t<std::remove_reference_t<Item>>,
            std::underlying_type_t<std::remove_reference_t<Item>>
        >;

        // Cast the enum to the underlying type, and forward as lvalue.
        concrete_archive().serialize(reinterpret_cast<std::add_lvalue_reference_t<integral_type>>(item));
    }

    /**
     * Serialize binary data.
     */
    template <typename T>
    void serialize_item(binary<T> && item)
    {
        concrete_archive().serialize(item.data(), item.size_in_bytes());
    }

    /**
     * Returns the concrete archive.
     */
    archive_type & concrete_archive()
    {
        return static_cast<archive_type &>(*this);
    }
}; // archive


/**
 * This archive serves as an output archive, which saves data into memory.
 * Every save operation appends data into the vector.
 * This archive serves as an optimization around vector, use 'memory_output_archive' instead.
 */
class lazy_vector_memory_output_archive : public archive<lazy_vector_memory_output_archive>
{
public:
    /**
     * The base archive.
     */
    using base = archive<lazy_vector_memory_output_archive>;

    /**
     * Declare base as friend.
     */
    friend base;

    /**
     * saving archive.
     */
    using saving = void;

protected:
    /**
     * Constructs a memory output archive, that outputs to the given vector.
     */
    explicit lazy_vector_memory_output_archive(std::vector<unsigned char> & output) noexcept :
        m_output(std::addressof(output)),
        m_size(output.size())
    {
    }

    /**
     * Serialize a single item - save it to the vector.
     */
    template <typename Item>
    void serialize(Item && item)
    {
        // Increase vector size.
        if (m_size + sizeof(item) > m_output->size()) {
             m_output->resize((m_size + sizeof(item)) * 3 / 2);
        }

        // Copy the data to the end of the vector.
        std::copy_n(reinterpret_cast<const unsigned char *>(std::addressof(item)),
            sizeof(item),
            m_output->data() + m_size);

        // Increase the size.
        m_size += sizeof(item);
    }

    /**
     * Serialize binary data - save it to the vector.
     */
    void serialize(const void * data, size_type size)
    {
        // Increase vector size.
        if (m_size + size > m_output->size()) {
             m_output->resize((m_size + size) * 3 / 2);
        }

        // Copy the data to the end of the vector.
        std::copy_n(static_cast<const unsigned char *>(data),
            size,
            m_output->data() + m_size);

        // Increase the size.
        m_size += size;
    }

     /**
      * Resizes the vector to the desired size.
      */
     void fit_vector()
     {
          m_output->resize(m_size);
     }

private:
    /**
     * The output vector.
     */
    std::vector<unsigned char> * m_output{};

     /**
      * The vector size.
      */
     std::size_t m_size{};
}; // lazy_vector_memory_output_archive

/**
 * This archive serves as an output archive, which saves data into memory.
 * Every save operation appends data into the vector.
 */
class memory_output_archive : private lazy_vector_memory_output_archive
{
public:
    /**
     * The base archive.
     */
    using base = lazy_vector_memory_output_archive;

    /**
     * Constructs a memory output archive, that outputs to the given vector.
     */
    explicit memory_output_archive(std::vector<unsigned char> & output) noexcept :
          lazy_vector_memory_output_archive(output)
    {
    }

    /**
     * Saves items into the archive.
     */
    template <typename... Items>
    void operator()(Items && ... items)
     {
          try {
               // Serialize the items.
               base::operator()(std::forward<Items>(items)...);

               // Fit the vector.
               fit_vector();
          } catch (...) {
               // Fit the vector.
               fit_vector();
               throw;
          }
     }
};

/**
 * This archive serves as the memory view input archive, which loads data from non owning memory.
 * Every load operation advances an offset to that the next data may be loaded on the next iteration.
 */
class memory_view_input_archive : public archive<memory_view_input_archive>
{
public:
    /**
     * The base archive.
     */
    using base = archive<memory_view_input_archive>;

    /**
     * Declare base as friend.
     */
    friend base;

    /**
     * Loading archive.
     */
    using loading = void;

    /**
     * Construct a memory view input archive, that loads data from an array of given pointer and size.
     */
    memory_view_input_archive(const unsigned char * input, std::size_t size) noexcept :
        m_input(input),
        m_size(size)
    {
    }

    /**
     * Construct a memory view input archive, that loads data from an array of given pointer and size.
     */
    memory_view_input_archive(const char * input, std::size_t size) noexcept :
        m_input(reinterpret_cast<const unsigned char *>(input)),
        m_size(size)
    {
    }

protected:
    /**
     * Resets the serialization to offset zero.
     */
    void reset() noexcept
    {
        m_offset = 0;
    }

    /**
     * Returns the offset of the serialization.
     */
    std::size_t get_offset() const noexcept
    {
        return m_offset;
    }

    /**
     * Serialize a single item - load it from the vector.
     */
    template <typename Item>
    void serialize(Item && item)
    {
        // Verify that the vector is large enough to contain the item.
        if (m_size < (sizeof(item) + m_offset)) {
            throw out_of_range("Input vector was not large enough to contain the requested item");
        }

        // Fetch the item from the vector.
        std::copy_n(m_input + m_offset, sizeof(item), reinterpret_cast<unsigned char *>(std::addressof(item)));

        // Increase the offset according to item size.
        m_offset += sizeof(item);
    }

    /**
     * Serializes binary data.
     */
    void serialize(void * data, size_type size)
    {
        // Verify that the vector is large enough to contain the data.
        if (m_size < (size + m_offset)) {
            throw out_of_range("Input vector was not large enough to contain the requested item");
        }

        // Fetch the binary data from the vector.
        std::copy_n(m_input + m_offset, size, static_cast<unsigned char *>(data));

        // Increase the offset according to data size.
        m_offset += size;
    }

private:
    /**
     * The input data.
     */
    const unsigned char * m_input{};

    /**
     * The input size.
     */
    std::size_t m_size{};

    /**
     * The next input.
     */
    std::size_t m_offset{};
}; // memory_view_input_archive

/**
 * This archive serves as the memory input archive, which loads data from owning memory.
 * Every load operation erases data from the beginning of the vector.
 */
class memory_input_archive : private memory_view_input_archive
{
public:
    /**
     * Construct a memory input archive from a vector.
     */
    memory_input_archive(std::vector<unsigned char> & input) :
        memory_view_input_archive(input.data(), input.size()),
        m_input(std::addressof(input))
    {
    }

    /**
     * Load items from the archive.
     */
    template <typename... Items>
    void operator()(Items && ... items)
    {
        try {
            // Update the input archive.
            static_cast<memory_view_input_archive &>(*this) = { m_input->data(), m_input->size() };

            // Load the items.
            memory_view_input_archive::operator()(std::forward<Items>(items)...);
        } catch (...) {
            // Erase the loaded elements.
            m_input->erase(m_input->begin(), m_input->begin() + get_offset());

            // Reset to offset zero.
            reset();
            throw;
        }

        // Erase the loaded elements.
        m_input->erase(m_input->begin(), m_input->begin() + get_offset());

        // Reset to offset zero.
        reset();
    }

private:
    /**
     * The input data.
     */
    std::vector<unsigned char> * m_input{};
};


/**
 * Serialize resizable containers, operates on loading (input) archives.
 */
template <typename Archive, typename Container, typename...,
    typename = decltype(std::declval<Container &>().size()),
    typename = decltype(std::declval<Container &>().begin()),
    typename = decltype(std::declval<Container &>().end()),
    typename = decltype(std::declval<Container &>().resize(std::size_t())),
    typename = std::enable_if_t<
        std::is_class<
            typename Container::value_type
        >::value || !std::is_base_of<
            std::random_access_iterator_tag,
            typename std::iterator_traits<typename Container::iterator>::iterator_category
        >::value
    >,
    typename = typename Archive::loading,
    typename = void, typename = void, typename = void, typename = void
>
void serialize(Archive & archive, Container & container)
{
    size_type size = 0;

    // Fetch the number of items to load.
    archive(size);

    // Resize the container to match the size.
    container.resize(size);

    // Serialize all the items.
    for (auto & item : container) {
        archive(item);
    }
};

/**
 * Serialize resizable containers, operates on saving (output) archives.
 */
template <typename Archive, typename Container, typename...,
    typename = decltype(std::declval<Container &>().size()),
    typename = decltype(std::declval<Container &>().begin()),
    typename = decltype(std::declval<Container &>().end()),
    typename = decltype(std::declval<Container &>().resize(std::size_t())),
    typename = std::enable_if_t<
        std::is_class<
            typename Container::value_type
        >::value || !std::is_base_of<
            std::random_access_iterator_tag,
            typename std::iterator_traits<typename Container::iterator>::iterator_category
        >::value || !detail::has_data_member_function<Container>::value
    >,
    typename = typename Archive::saving,
    typename = void, typename = void, typename = void, typename = void
>
void serialize(Archive & archive, const Container & container)
{
    // Save the container size.
    archive(static_cast<size_type>(container.size()));

    // Serialize all the items.
    for (auto & item : container) {
        archive(item);
    }
}

/**
 * Serialize resizable, continuous containers, of fundamental or enumeration types.
 * Operates on loading (input) archives.
 */
template <typename Archive, typename Container, typename...,
    typename = decltype(std::declval<Container &>().size()),
    typename = decltype(std::declval<Container &>().begin()),
    typename = decltype(std::declval<Container &>().end()),
    typename = decltype(std::declval<Container &>().resize(std::size_t())),
    typename = decltype(std::declval<Container &>().data()),
    typename = std::enable_if_t<
        std::is_fundamental<typename Container::value_type>::value ||
        std::is_enum<typename Container::value_type>::value
    >,
    typename = std::enable_if_t<std::is_base_of<
        std::random_access_iterator_tag,
        typename std::iterator_traits<typename Container::iterator>::iterator_category>::value
    >,
    typename = typename Archive::loading,
    typename = void, typename = void, typename = void, typename = void
>
void serialize(Archive & archive, Container & container)
{
    size_type size = 0;

    // Fetch the number of items to load.
    archive(size);

    // Resize the container to match the size.
    container.resize(size);

    // If the size is zero, return.
    if (!size) {
        return;
    }

    // Serialize the binary data.
    archive(as_binary(std::addressof(container[0]), static_cast<size_type>(container.size())));
};

/**
 * Serialize resizable, continuous containers, of fundamental or enumeration types.
 * Operates on saving (output) archives.
 */
template <typename Archive, typename Container, typename...,
    typename = decltype(std::declval<Container &>().size()),
    typename = decltype(std::declval<Container &>().begin()),
    typename = decltype(std::declval<Container &>().end()),
    typename = decltype(std::declval<Container &>().resize(std::size_t())),
    typename = decltype(std::declval<Container &>().data()),
    typename = std::enable_if_t<
        std::is_fundamental<typename Container::value_type>::value ||
        std::is_enum<typename Container::value_type>::value
    >,
    typename = std::enable_if_t<std::is_base_of<
        std::random_access_iterator_tag,
        typename std::iterator_traits<typename Container::iterator>::iterator_category>::value
    >,
    typename = typename Archive::saving,
    typename = void, typename = void, typename = void, typename = void
>
void serialize(Archive & archive, const Container & container)
{
    // The container size.
    auto size = static_cast<size_type>(container.size());

    // Save the container size.
    archive(size);

    // If the size is zero, return.
    if (!size) {
        return;
    }

    // Serialize the binary data.
    archive(as_binary(std::addressof(container[0]), static_cast<size_type>(container.size())));
}

/**
 * Serialize Associative and UnorderedAssociative containers, operates on loading (input) archives.
 */
template <typename Archive, typename Container, typename...,
    typename = decltype(std::declval<Container &>().size()),
    typename = decltype(std::declval<Container &>().begin()),
    typename = decltype(std::declval<Container &>().end()),
    typename = typename Container::value_type,
    typename = typename Container::key_type,
    typename = typename Archive::loading
>
void serialize(Archive & archive, Container & container)
{
    size_type size = 0;

    // Fetch the number of items to load.
    archive(size);

    // Serialize all the items.
    for (size_type i = 0; i < size; ++i) {
        // Deduce the container item type.
        using item_type = detail::container_nonconst_value_type_t<Container>;

        // Create just enough storage properly aligned for one item.
        std::aligned_storage_t<sizeof(item_type), alignof(item_type)> storage;

        // Default construct the item in the storage.
        auto item = access::placement_new<item_type>(std::addressof(storage));

        try {
            // Serialize the item.
            archive(*item);

            // Insert the item to the container.
            container.insert(std::move(*item));
        } catch (...) {
            // Destruct the item.
            access::destruct(*item);
            throw;
        }

        // Destruct the item.
        access::destruct(*item);
    }
}

/**
 * Serialize Associative and UnorderedAssociative containers, operates on saving (output) archives.
 */
template <typename Archive, typename Container, typename...,
    typename = decltype(std::declval<Container &>().size()),
    typename = decltype(std::declval<Container &>().begin()),
    typename = decltype(std::declval<Container &>().end()),
    typename = typename Container::value_type,
    typename = typename Container::key_type,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const Container & container)
{
    // Save the container size.
    archive(static_cast<size_type>(container.size()));

    // Serialize every item.
    for (auto & item : container) {
        archive(item);
    }
}

/**
 * Serialize arrays, operates on loading (input) archives.
 */
template <typename Archive, typename Item, std::size_t size, typename...,
    typename = typename Archive::loading
>
void serialize(Archive & archive, Item(&array)[size])
{
    // Serialize every item.
    for (auto & item: array) {
        archive(item);
    }
}

/**
 * Serialize arrays, operates on saving (output) archives.
 */
template <typename Archive, typename Item, std::size_t size, typename...,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const Item(&array)[size])
{
    // Serialize every item.
    for (auto & item: array) {
        archive(item);
    }
}

/**
 * Serialize std::array, operates on loading (input) archives.
 */
template <typename Archive, typename Item, std::size_t size, typename...,
    typename = typename Archive::loading
>
void serialize(Archive & archive, std::array<Item, size> & array)
{
    // Serialize every item.
    for (auto & item: array) {
        archive(item);
    }
}

/**
 * Serialize std::array, operates on saving (output) archives.
 */
template <typename Archive, typename Item, std::size_t size, typename...,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const std::array<Item, size> & array)
{
    // Serialize every item.
    for (auto & item: array) {
        archive(item);
    }
}

/**
 * Serialize std::pair, operates on loading (input) archives.
 */
template <typename Archive, typename First, typename Second, typename...,
    typename = typename Archive::loading
>
void serialize(Archive & archive, std::pair<First, Second> & pair)
{
    // Serialize first, then second.
    archive(pair.first, pair.second);
}

/**
 * Serialize std::pair, operates on saving (output) archives.
 */
template <typename Archive, typename First, typename Second, typename...,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const std::pair<First, Second> & pair)
{
    // Serialize first, then second.
    archive(pair.first, pair.second);
}

/**
 * Serialize std::tuple, operates on loading (input) archives.
 */
template <typename Archive, typename... TupleItems,
    typename = typename Archive::loading
>
void serialize(Archive & archive, std::tuple<TupleItems...> & tuple)
{
    // Delegate to a helper function with an index sequence.
    serialize(archive, tuple, std::make_index_sequence<sizeof...(TupleItems)>());
}

/**
 * Serialize std::tuple, operates on saving (output) archives.
 */
template <typename Archive, typename... TupleItems,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const std::tuple<TupleItems...> & tuple)
{
    // Delegate to a helper function with an index sequence.
    serialize(archive, tuple, std::make_index_sequence<sizeof...(TupleItems)>());
}

/**
 * Serialize std::tuple, operates on loading (input) archives.
 * This overload serves as a helper function that accepts an index sequence.
 */
template <typename Archive, typename... TupleItems, std::size_t... Indices,
    typename = typename Archive::loading
>
void serialize(Archive & archive, std::tuple<TupleItems...> & tuple, std::index_sequence<Indices...>)
{
    archive(std::get<Indices>(tuple)...);
}

/**
 * Serialize std::tuple, operates on saving (output) archives.
 * This overload serves as a helper function that accepts an index sequence.
 */
template <typename Archive, typename... TupleItems, std::size_t... Indices,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const std::tuple<TupleItems...> & tuple, std::index_sequence<Indices...>)
{
    archive(std::get<Indices>(tuple)...);
}

/**
 * Serialize std::unique_ptr of non polymorphic, in case of a loading (input) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<!std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::loading
>
void serialize(Archive & archive, std::unique_ptr<Type> & object)
{
    // Construct a new object.
    auto loaded_object = access::make_unique<Type>();

    // Serialize the object.
    archive(*loaded_object);

    // Transfer the object.
    object.reset(loaded_object.release());
}

/**
 * Serialize std::unique_ptr of non polymorphic, in case of a saving (output) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<!std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const std::unique_ptr<Type> & object)
{
    // Prevent serialization of null pointers.
    if (nullptr == object) {
        throw attempt_to_serialize_null_pointer_error();
    }

    // Serialize the object.
    archive(*object);
}

/**
 * Serialize std::unique_ptr of polymorphic, in case of a loading (input) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::loading,
    typename = void
>
void serialize(Archive & archive, std::unique_ptr<Type> & object)
{
    std::unique_ptr<polymorphic> loaded_type;

    // Get the instance of the polymorphic registry.
    auto & registry_instance = registry<Archive>::get_instance();

    // Serialize the object using the registry.
    registry_instance.serialize(archive, loaded_type);

    try {
        // Check if the loaded type is convertible to Type.
        object.reset(&dynamic_cast<Type &>(*loaded_type));

        // Release the object.
        loaded_type.release();
    } catch (const std::bad_cast &) {
        // The loaded type was not convertible to Type.
        throw polymorphic_type_mismatch_error();
    }
}

/**
 * Serialize std::unique_ptr of polymorphic, in case of a saving (output) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::saving,
    typename = void
>
void serialize(Archive & archive, const std::unique_ptr<Type> & object)
{
    // Prevent serialization of null pointers.
    if (nullptr == object) {
        throw attempt_to_serialize_null_pointer_error();
    }

    // Get the instance of the polymorphic registry.
    auto & registry_instance = registry<Archive>::get_instance();

    // Serialize the object using the registry.
    registry_instance.serialize(archive, *object);
}

/**
 * Serialize std::shared_ptr of non polymorphic, in case of a loading (input) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<!std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::loading
>
void serialize(Archive & archive, std::shared_ptr<Type> & object)
{
    // Construct a new object.
    auto loaded_object = access::make_unique<Type>();

    // Serialize the object.
    archive(*loaded_object);

    // Transfer the object.
    object.reset(loaded_object.release());
}

/**
 * Serialize std::shared_ptr of non polymorphic, in case of a saving (output) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<!std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const std::shared_ptr<Type> & object)
{
    // Prevent serialization of null pointers.
    if (nullptr == object) {
        throw attempt_to_serialize_null_pointer_error();
    }

    // Serialize the object.
    archive(*object);
}

/**
 * Serialize std::shared_ptr of polymorphic, in case of a loading (input) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::loading,
    typename = void
>
void serialize(Archive & archive, std::shared_ptr<Type> & object)
{
    std::unique_ptr<polymorphic> loaded_type;

    // Get the instance of the polymorphic registry.
    auto & registry_instance = registry<Archive>::get_instance();

    // Serialize the object using the registry.
    registry_instance.serialize(archive, loaded_type);

    try {
        // Check if the loaded type is convertible to Type.
        object.reset(&dynamic_cast<Type &>(*loaded_type));

        // Release the object.
        loaded_type.release();
    } catch (const std::bad_cast &) {
        // The loaded type was not convertible to Type.
        throw polymorphic_type_mismatch_error();
    }
}

/**
 * Serialize std::shared_ptr of polymorphic, in case of a saving (output) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = std::enable_if_t<std::is_base_of<polymorphic, Type>::value>,
    typename = typename Archive::saving,
    typename = void
>
void serialize(Archive & archive, const std::shared_ptr<Type> & object)
{
    // Prevent serialization of null pointers.
    if (nullptr == object) {
        throw attempt_to_serialize_null_pointer_error();
    }

    // Get the instance of the polymorphic registry.
    auto & registry_instance = registry<Archive>::get_instance();

    // Serialize the object using the registry.
    registry_instance.serialize(archive, *object);
}

/**
 * Serialize types wrapped with polymorphic_wrapper,
 * which is supported only for saving (output) archives.
 * Usually used with the as_polymorphic facility.
 */
template <typename Archive, typename Type, typename...,
    typename = typename Archive::saving
>
void serialize(Archive & archive, const polymorphic_wrapper<Type> & object)
{
    // Get the instance of the polymorphic registry.
    auto & registry_instance = registry<Archive>::get_instance();

    // Serialize using the registry.
    registry_instance.serialize(archive, *object);
}

/**
 * A meta container that holds a sequence of archives.
 */
template <typename... Archives>
struct archive_sequence {};

/**
 * The built in archives.
 */
using builtin_archives = archive_sequence<
    memory_view_input_archive,
    lazy_vector_memory_output_archive
>;


/**
 * The serialization method type.
 */
template <typename Archive, typename = void>
struct serialization_method;

/**
 * The serialization method type exporter, for loading (input) archives.
 */
template <typename Archive>
struct serialization_method<Archive, typename Archive::loading>
{
    /**
     * Disabled default constructor.
     */
    serialization_method() = delete;

    /**
     * The exported type.
     */
    using type = void(*)(Archive &, std::unique_ptr<polymorphic> &);
}; // serialization_method

/**
 * The serialization method type exporter, for saving (output) archives.
 */
template <typename Archive>
struct serialization_method<Archive, typename Archive::saving>
{
    /**
     * Disabled default constructor.
     */
    serialization_method() = delete;

    /**
     * The exported type.
     */
    using type = void(*)(Archive &, const polymorphic &);
}; // serialization_method

/**
 * The serialization method type.
 */
template <typename Archive>
using serialization_method_t = typename serialization_method<Archive>::type;

/**
 * Make a serialization method from type and a loading (input) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = typename Archive::loading
>
serialization_method_t<Archive> make_serialization_method() noexcept
{
    return [](Archive & archive, std::unique_ptr<polymorphic> & object) {
        auto concrete_type = access::make_unique<Type>();
        archive(*concrete_type);
        object.reset(concrete_type.release());
    };
}

/**
 * Make a serialization method from type and a saving (output) archive.
 */
template <typename Archive, typename Type, typename...,
    typename = typename Archive::saving,
    typename = void
>
serialization_method_t<Archive> make_serialization_method() noexcept
{
    return [](Archive & archive, const polymorphic & object) {
        archive(dynamic_cast<const Type &>(object));
    };
}

} // archive
} // bitsery
#endif /* INCLUDE_BITSERY_DETAILS_ARCHIVE_H_ */
