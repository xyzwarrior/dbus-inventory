#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <any>
#include "interface_ops.hpp"
#include "serialize.hpp"

template <typename T>
using ServerObject = T;

class Manager
{
public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = default;
    Manager& operator=(Manager&&) = default;
    ~Manager() = default;

    Manager(sdbus::IConnection* connection);

    /** @brief Start processing DBus messages. */
    void run() noexcept;

    /** @brief Provided for testing only. */
    void shutdown() noexcept;

    /** @brief sd_bus Notify method implementation callback. */
    void notify(std::map<sdbus::ObjectPath, Object> objs);

    /** @brief Drop one or more objects from DBus. */
    void destroyObjects(const std::vector<const char*>& paths);

    /** @brief Add objects to DBus. */
    void createObjects(
        const std::map<sdbus::ObjectPath, Object>& objs);

    /** @brief Add or update objects on DBus. */
    void updateObjects(
        const std::map<sdbus::ObjectPath, Object>& objs,
        bool restoreFromCache = false);

    /** @brief Restore persistent inventory items */
    void restore();

    /** @brief Invoke an sdbus server binding method.
     *
     *  Invoke the requested method with a reference to the requested
     *  sdbus server binding interface as a parameter.
     *
     *  @tparam T - The sdbus server binding interface type.
     *  @tparam U - The type of the sdbus server binding member.
     *  @tparam Args - Argument types of the binding member.
     *
     *  @param[in] path - The DBus path on which the method should
     *      be invoked.
     *  @param[in] interface - The DBus interface hosting the method.
     *  @param[in] member - Pointer to sdbusplus server binding member.
     *  @param[in] args - Arguments to forward to the binding member.
     *
     *  @returns - The return/value type of the binding method being
     *      called.
     */
    template <typename T, typename U, typename... Args>
    decltype(auto) invokeMethod(const char* path, const char* interface,
                                U&& member, Args&&... args)
    {
        auto& iface = getInterface<T>(path, interface);
        return (iface.*member)(std::forward<Args>(args)...);
    }

private:
    using InterfaceComposite = std::map<std::string, std::any>;
    using ObjectReferences = std::map<std::string, InterfaceComposite>;

    // The int instantiations are safe since the signature of these
    // functions don't change from one instantiation to the next.
    using Makers =
        std::map<std::string, std::tuple<MakeInterfaceType, AssignInterfaceType,
                                         SerializeInterfaceType<SerialOps>,
                                         DeserializeInterfaceType<SerialOps>>>;

    /** @brief Provides weak references to interface holders.
     *
     *  Common code for all types for the templated getInterface
     *  methods.
     *
     *  @param[in] path - The DBus path for which the interface
     *      holder instance should be provided.
     *  @param[in] interface - The DBus interface for which the
     *      holder instance should be provided.
     *
     *  @returns A weak reference to the holder instance.
     */
    const std::any& getInterfaceHolder(const char*, const char*) const;
    std::any& getInterfaceHolder(const char*, const char*);

    /** @brief Provides weak references to interface holders.
     *
     *  @tparam T - The sdbusplus server binding interface type.
     *
     *  @param[in] path - The DBus path for which the interface
     *      should be provided.
     *  @param[in] interface - The DBus interface to obtain.
     *
     *  @returns A weak reference to the interface holder.
     */
    template <typename T>
    auto& getInterface(const char* path, const char* interface)
    {
        auto& holder = getInterfaceHolder(path, interface);
        return *std::any_cast<std::shared_ptr<T>&>(holder);
    }
    template <typename T>
    auto& getInterface(const char* path, const char* interface) const
    {
        auto& holder = getInterfaceHolder(path, interface);
        return *std::any_cast<T>(holder);
    }
                                             
    /** @brief Add or update interfaces on DBus. */
    void updateInterfaces(const sdbus::ObjectPath& path,
                          const Object& interfaces,
                          ObjectReferences::iterator pos,
                          bool emitSignals = true,
                          bool restoreFromCache = false);
    /** @brief A container of sdbusplus server interface references. */
    ObjectReferences _refs;
    /** @brief A container of pimgen generated factory methods.  */
    static const Makers _makers;
    sdbus::IConnection* _connection;
    std::map<std::string, std::unique_ptr<sdbus::IObject>> _objs;
    const char* _root = "/xyz/openbmc_project/inventory";
};