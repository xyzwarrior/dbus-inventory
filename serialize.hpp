#pragma once

#include <cereal/archives/json.hpp>
#include <filesystem>
#include <fstream>

#ifndef PIM_PERSIST_PATH
//#define PIM_PERSIST_PATH "/var/lib/phosphor-inventory-manager"
#define PIM_PERSIST_PATH "./phosphor-inventory-manager"
#endif

namespace fs = std::filesystem;

namespace detail
{
inline fs::path getStoragePath(const std::string& path,
                               const std::string& iface)
{
    auto p = fs::path(PIM_PERSIST_PATH);
    p /= fs::path(path).relative_path();
    p /= fs::path(iface).relative_path();
    return p;
}
} // namespace detail

struct SerialOps
{
    /** @brief Serialize inventory item path
     *  Serializing only path for an empty interface to be consistent
     *  interfaces.
     *  @param[in] path - DBus object path
     *  @param[in] iface - Inventory interface name
     */
    static void serialize(const std::string& path, const std::string& iface)
    {
        auto p = detail::getStoragePath(path, iface);
        fs::create_directories(p.parent_path());
        std::ofstream os(p, std::ios::binary);
    }

    /** @brief Serialize inventory item
     *
     *  @param[in] path - DBus object path
     *  @param[in] iface - Inventory interface name
     *  @param[in] object - Object to be serialized
     */
    template <typename T>
    static void serialize(const std::string& path, const std::string& iface,
                          const T& object)
    {
        auto p = detail::getStoragePath(path, iface);
        fs::create_directories(p.parent_path());
        std::ofstream os(p, std::ios::binary);
        cereal::JSONOutputArchive oarchive(os);
        oarchive(object);
    }

    static void deserialize(const std::string&, const std::string&)
    {
        // This is intentionally a noop.
    }

    /** @brief Deserialize inventory item
     *
     *  @param[in] path - DBus object path
     *  @param[in] iface - Inventory interface name
     *  @param[in] object - Object to be serialized
     */
    template <typename T>
    static void deserialize(const std::string& path, const std::string& iface,
                            T& object)
    {
        auto p = detail::getStoragePath(path, iface);
        try
        {
            std::cout << "    deserialize: " << p << std::endl;
            if (fs::exists(p))
            {
                std::ifstream is(p, std::ios::in | std::ios::binary);
                //if (is) {
                //    std::cout << is.rdbuf() << std::endl; // Dumps entire file buffer to stdout
                //}
                cereal::JSONInputArchive iarchive(is);
                iarchive(object);
            }
        }
        catch (cereal::Exception& e)
        {
            //phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
	        printf("Exception: %s\n", e.what());
            //fs::remove(p);
        }
    }
};
