/*
 * registry.h
 *
 *  Created on: Oct 8, 2018
 *      Author: simon
 */

#ifndef INCLUDE_BITSERY_DETAILS_REGISTRY_H_
#define INCLUDE_BITSERY_DETAILS_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>
#include <__config>
#include <__mutex_base>
#include <iostream>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

#include "archive.h"
#include "common.h"
#include "polymorphic.h"

namespace bitsery
{

namespace archive
{

/**
 * This class manages polymorphic type registration for serialization process.
 */
template <typename Archive>
class registry
{
public:
    static_assert(!std::is_reference<Archive>::value,
        "Disallows reference type for archive in registry");

    /**
     * Returns the global instance of the registry.
     */
    static registry & get_instance() noexcept
    {
        static registry registry;
        return registry;
    }

    /**
     * Add a serialization method for a given polymorphic type and id.
     */
    template <typename Type, id_type id>
    void add()
    {
        add<Type>(id);
    }

    /**
     * Adds a serialization method for a given polymorphic type and id.
     */
    template <typename Type>
    void add(id_type id)
    {
        add(id, typeid(Type).name(), make_serialization_method<Archive, Type>());
    }

    /**
     * Add a serialization method for a given polymorphic type information string and id.
     * The behavior is undefined if the type isn't derived from polymorphic.
     */
    void add(id_type id, std::string type_information_string, serialization_method_t<Archive> serialization_method)
    {
        // Lock the serialization method maps for write access.
        std::lock_guard<shared_mutex> lock(m_shared_mutex);

        // Add the serialization id to serialization method mapping.
        m_serialization_id_to_method.emplace(id, std::move(serialization_method));

        // Add the type information to to serialization id mapping.
        m_type_information_to_serialization_id.emplace(std::move(type_information_string), id);
    }

    /**
     * Serialize a polymorphic type, in case of a loading (input) archive.
     */
    template <typename...,
        typename ArchiveType = Archive,
        typename = typename ArchiveType::loading
    >
    void serialize(Archive & archive, std::unique_ptr<polymorphic> & object)
    {
        id_type id = 0;

        // Load the serialization id.
        archive(id);

        // Lock the serialization method maps for read access.
        std::shared_lock<shared_mutex> lock(m_shared_mutex);

        // Find the serialization method.
        auto serialization_id_to_method_pair = m_serialization_id_to_method.find(id);
        if (m_serialization_id_to_method.end() == serialization_id_to_method_pair) {
            throw undeclared_polymorphic_type_error();
        }

        // Fetch the serialization method.
        auto serialization_method = serialization_id_to_method_pair->second;

        // Unlock the serialization method maps.
        lock.unlock();

        // Serialize (load) the given object.
        serialization_method(archive, object);
    }

    /**
     * Serialize a polymorphic type, in case of a saving (output) archive.
     */
    template <typename...,
        typename ArchiveType = Archive,
        typename = typename ArchiveType::saving
    >
    void serialize(Archive & archive, const polymorphic & object)
    {
        // Lock the serialization method maps for read access.
        std::shared_lock<shared_mutex> lock(m_shared_mutex);

        // Find the serialization id.
        auto type_information_to_serialization_id_pair = m_type_information_to_serialization_id.find(
            typeid(object).name());
        if (m_type_information_to_serialization_id.end() == type_information_to_serialization_id_pair) {
            throw undeclared_polymorphic_type_error();
        }

        // Fetch the serialization id.
        auto id = type_information_to_serialization_id_pair->second;

        // Find the serialization method.
        auto serialization_id_to_method_pair = m_serialization_id_to_method.find(id);
        if (m_serialization_id_to_method.end() == serialization_id_to_method_pair) {
            throw undeclared_polymorphic_type_error();
        }

        // Fetch the serialization method.
        auto serialization_method = serialization_id_to_method_pair->second;

        // Unlock the serialization method maps.
        lock.unlock();

        // Serialize (save) the serialization id.
        archive(id);

        // Serialize (save) the given object.
        serialization_method(archive, object);
    }

private:
    /**
     * Default constructor, defaulted.
     */
    registry() = default;

private:
    /**
     * The shared mutex that protects the maps below.
     */
    shared_mutex m_shared_mutex;

    /**
     * A map between serialization id to method.
     */
    std::unordered_map<id_type, serialization_method_t<Archive>> m_serialization_id_to_method;

    /**
     * A map between type information string to serialization id.
     */
    std::unordered_map<std::string, id_type> m_type_information_to_serialization_id;
}; // registry


/**
 * Makes a meta pair of type and id.
 */
template <typename Type, id_type id>
struct make_type;

/**
 * Registers user defined polymorphic types to serialization registry.
 */
template <typename... ExtraTypes>
class register_types;

/**
 * A no operation class, registers an empty list of types.
 */
template <>
class register_types<>
{
};

/**
 * Registers user defined polymorphic types to serialization registry.
 */
template <typename Type, id_type id, typename... ExtraTypes>
class register_types<make_type<Type, id>, ExtraTypes...> : private register_types<ExtraTypes...>
{
public:
    /**
     * Registers the type to the built in archives of the serializer.
     */
    register_types() noexcept :
        register_types(builtin_archives())
    {
    }

    /**
     * Registers the type to every archive in the given archive sequence.
     */
    template <typename... Archives>
    register_types(archive_sequence<Archives...> archives) noexcept
    {
        register_type_to_archives(archives);
    }

private:
    /**
     * Registers the type to every archive in the given archive sequence.
     */
    template <typename Archive, typename... Archives>
    void register_type_to_archives(archive_sequence<Archive, Archives...>) noexcept
    {
        // Register the type to the first archive.
        register_type_to_archive<Archive>();

        // Register the type to the other archives.
        register_type_to_archives(archive_sequence<Archives...>());
    }

    /**
     * Registers the type to an empty archive sequence - does nothing.
     */
    void register_type_to_archives(archive_sequence<>) noexcept
    {
    }

    /**
     * Registers the type to the given archive.
     * Must throw no exceptions since this will most likely execute
     * during static construction.
     * The effect of failure is that the type will not be registered,
     * it will be detected during runtime.
     */
    template <typename Archive>
    void register_type_to_archive() noexcept
    {
        try {
            registry<Archive>::get_instance().template add<Type, id>();
        } catch (...) {
        }
    }
}; // register_types


/**
 * Accepts a name and returns its serialization id.
 * We return the first 8 bytes of the sha1 on the given name.
 */
template <std::size_t size>
constexpr id_type make_id(const char (&name)[size])
{
    // Initialize constants.
    std::uint32_t h0 = 0x67452301u;
    std::uint32_t h1 = 0xEFCDAB89u;
    std::uint32_t h2 = 0x98BADCFEu;
    std::uint32_t h3 = 0x10325476u;
    std::uint32_t h4 = 0xC3D2E1F0u;

    // Initialize the message size in bits.
    std::uint64_t message_size = (size - 1) * 8;

    // Calculate the size aligned to 64 bytes (512 bits).
    constexpr std::size_t aligned_message_size = (((size + sizeof(std::uint64_t)) + 63) / 64) * 64;

    // Construct the pre-processed message.
    std::uint32_t preprocessed_message[aligned_message_size / sizeof(std::uint32_t)] = {};
    for (std::size_t i = 0; i < size - 1; ++i) {
        preprocessed_message[i / 4] |= detail::swap_byte_order(std::uint32_t(name[i])
            << ((sizeof(std::uint32_t) - 1 - (i % 4)) * 8));
    }

    // Append the byte 0x80.
    preprocessed_message[(size - 1) / 4] |= detail::swap_byte_order(std::uint32_t(0x80)
        << ((sizeof(std::uint32_t) - 1 - ((size - 1) % 4)) * 8));

    // Append the length in bits, in 64 bit, big endian.
    preprocessed_message[(aligned_message_size / sizeof(std::uint32_t)) - 2] =
        detail::swap_byte_order(std::uint32_t(message_size >> 32));
    preprocessed_message[(aligned_message_size / sizeof(std::uint32_t)) - 1] =
        detail::swap_byte_order(std::uint32_t(message_size));

    // Process the message in successive 512-bit chunks.
    for (std::size_t i = 0; i < (aligned_message_size / sizeof(std::uint32_t)); i += 16) {
        std::uint32_t w[80] = {};

        // Set the value of w.
        for (std::size_t j = 0; j < 16; ++j) {
            w[j] = preprocessed_message[i + j];
        }

        // Extend the sixteen 32-bit words into eighty 32-bit words.
        for (std::size_t j = 16; j < 80; ++j) {
            w[j] = detail::swap_byte_order(detail::rotate_left(detail::swap_byte_order(
                w[j - 3] ^ w[j - 8] ^
                   w[j - 14] ^ w[j - 16]), 1));
        }

        // Initialize hash values for this chunk.
        auto a = h0;
        auto b = h1;
        auto c = h2;
        auto d = h3;
        auto e = h4;

        // Main loop.
        for (std::size_t j = 0; j < 80; ++j) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (j <= 19) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (j <= 39) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (j <= 59) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }

            auto temp = detail::rotate_left(a, 5) + f + e + k +
                detail::swap_byte_order(w[j]);
            e = d;
            d = c;
            c = detail::rotate_left(b, 30);
            b = a;
            a = temp;
        }

        // Add this chunk's hash to result so far.
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    // Produce the first 8 bytes of the hash in little endian.
    return detail::swap_byte_order((std::uint64_t(h0) << 32) | h1);
} // make_id

} // archive
} // bitsery

#endif /* INCLUDE_BITSERY_DETAILS_REGISTRY_H_ */
