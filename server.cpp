#include <manager.hpp>
#include <iostream>
#include <variant>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

// supported interfaces
/*
xyz.openbmc_project.Common.UUID
xyz.openbmc_project.Inventory.Item
xyz.openbmc_project.Inventory.Item.System
xyz.openbmc_project.Inventory.Item.Chassis
xyz.openbmc_project.Inventory.Item.Bmc
xyz.openbmc_project.Inventory.Item.Board
xyz.openbmc_project.Inventory.Item.Ethernet
xyz.openbmc_project.Inventory.Item.NetworkInterface
xyz.openbmc_project.Inventory.Decorator.Asset
xyz.openbmc_project.Inventory.Decorator.AssetTag
xyz.openbmc_project.Inventory.Decorator.Cacheable
xyz.openbmc_project.Inventory.Decorator.Replaceable
xyz.openbmc_project.Inventory.Decorator.Revision
*/
#include <xyz.openbmc_project.Common.UUID.hpp>
#include <xyz.openbmc_project.Inventory.Item.hpp>
#include <xyz.openbmc_project.Inventory.Item.System.hpp>
#include <xyz.openbmc_project.Inventory.Item.Chassis.hpp>
#include <xyz.openbmc_project.Inventory.Item.Bmc.hpp>
#include <xyz.openbmc_project.Inventory.Item.Board.hpp>
#include <xyz.openbmc_project.Inventory.Item.Ethernet.hpp>
#include <xyz.openbmc_project.Inventory.Item.NetworkInterface.hpp>
#include <xyz.openbmc_project.Inventory.Decorator.Asset.hpp>
#include <xyz.openbmc_project.Inventory.Decorator.AssetTag.hpp>
#include <xyz.openbmc_project.Inventory.Decorator.Cacheable.hpp>
#include <xyz.openbmc_project.Inventory.Decorator.Replaceable.hpp>
#include <xyz.openbmc_project.Inventory.Decorator.Revision.hpp>

/* Path of directory housing persisted inventory */
#ifndef PIM_PERSIST_PATH
//#define PIM_PERSIST_PATH "/var/lib/phosphor-inventory-manager"
#define PIM_PERSIST_PATH "./phosphor-inventory-manager"
#endif

/* The DBus inventory namespace root. */
#define INVENTORY_ROOT "/xyz/openbmc_project/inventory"

/*
busctl introspect xyz.openbmc_project.Inventory.Manager /xyz/openbmc_project/inventory
NAME                                  TYPE      SIGNATURE     RESULT/VALUE  FLAGS
org.freedesktop.DBus.Introspectable   interface -             -             -
.Introspect                           method    -             s             -
org.freedesktop.DBus.ObjectManager    interface -             -             -
.GetManagedObjects                    method    -             a{oa{sa{sv}}} -
.InterfacesAdded                      signal    oa{sa{sv}}    -             -
.InterfacesRemoved                    signal    oas           -             -
org.freedesktop.DBus.Peer             interface -             -             -
.GetMachineId                         method    -             s             -
.Ping                                 method    -             -             -
org.freedesktop.DBus.Properties       interface -             -             -
.Get                                  method    ss            v             -
.GetAll                               method    s             a{sv}         -
.Set                                  method    ssv           -             -
.PropertiesChanged                    signal    sa{sv}as      -             -
xyz.openbmc_project.Inventory.Manager interface -             -             -
.Notify                               method    a{oa{sa{sv}}} -             -
*/



//using namespace std::literals::string_literals;

/** @class InterfaceError
 *  @brief Exception class for unrecognized interfaces.
 */
class InterfaceError final : public std::invalid_argument
{
  public:
    ~InterfaceError() = default;
    InterfaceError() = delete;
    InterfaceError(const InterfaceError&) = delete;
    InterfaceError(InterfaceError&&) = default;
    InterfaceError& operator=(const InterfaceError&) = delete;
    InterfaceError& operator=(InterfaceError&&) = default;

    /** @brief Construct an interface error.
     *
     *  @param[in] msg - The message to be returned by what().
     *  @param[in] iface - The failing interface name.
     */
    InterfaceError(const char* msg, const std::string& iface) :
        std::invalid_argument(msg), interface(iface)
    {
    }

    /** @brief Log the exception message to the systemd journal. */
    void log() const;

  private:
    std::string interface;
};

void InterfaceError::log() const
{
    //logging::log<logging::level::ERR>(
    //    what(), phosphor::logging::entry("INTERFACE=%s", interface.c_str()));
    std::cout << "-- " << what() << " INTERFACE=" << interface << std::endl;
}

Manager::Manager(sdbus::IConnection* conn) : _connection(conn)
{

}

void Manager::destroyObjects(const std::vector<const char*>& paths)
{
    std::string p;

    for (const auto& path : paths)
    {
        p.assign(_root);
        p.append(path);
        //_bus.emit_object_removed(p.c_str());

        _objs[p]->emitInterfacesRemovedSignal();
        _refs.erase(p);
        _objs.erase(p);
    }
}

void Manager::updateInterfaces(const sdbus::ObjectPath& path,
                               const Object& interfaces,
                               ObjectReferences::iterator pos, bool newObject,
                               bool restoreFromCache)
{
    auto& refaces = pos->second;
    auto ifaceit = interfaces.cbegin();
    auto opsit = _makers.cbegin();
    auto refaceit = refaces.begin();
    std::vector</*std::string*/sdbus::InterfaceName> signals;

    while (ifaceit != interfaces.cend())
    {
        try
        {
            // Find the binding ops for this interface.
            opsit = std::lower_bound(opsit, _makers.cend(), ifaceit->first,
                                     compareFirst(_makers.key_comp()));

            if (opsit == _makers.cend() || opsit->first != ifaceit->first)
            {
                // This interface is not supported.
                throw InterfaceError("Encountered unsupported interface.",
                                     ifaceit->first);
            }

            // Find the binding insertion point or the binding to update.
            refaceit = std::lower_bound(refaceit, refaces.end(), ifaceit->first,
                                        compareFirst(refaces.key_comp()));

            if (refaceit == refaces.end() || refaceit->first != ifaceit->first)
            {
                // Add the new interface.
                auto& ctor = std::get<MakeInterfaceType>(opsit->second);
                refaceit = refaces.insert(
                    refaceit,
                    std::make_pair(ifaceit->first, ctor(_objs[path].get(), ifaceit->second)));

                signals.push_back(sdbus::InterfaceName(ifaceit->first));
            }
            else
            {
                // Set the new property values.
                auto& assign = std::get<AssignInterfaceType>(opsit->second);
                assign(ifaceit->second, refaceit->second);
            }
            if (!restoreFromCache)
            {
                auto& serialize =
                    std::get<SerializeInterfaceType<SerialOps>>(opsit->second);
                serialize(path, ifaceit->first, refaceit->second);
            }
            else
            {
                auto& deserialize =
                    std::get<DeserializeInterfaceType<SerialOps>>(
                        opsit->second);
                deserialize(path, ifaceit->first, refaceit->second);
            }
        }
        catch (const InterfaceError& e)
        {
            // Reset the binding ops iterator since we are
            // at the end.
            opsit = _makers.cbegin();
            e.log();
        }

        ++ifaceit;
    }

    if (newObject)
    {
        //_bus.emit_object_added(path.str.c_str());
        _objs[path]->emitInterfacesAddedSignal();

    }
    else if (!signals.empty())
    {
        //_bus.emit_interfaces_added(path.str.c_str(), signals);
        _objs[path]->emitInterfacesAddedSignal(signals);
    }
}


void Manager::updateObjects(
    const std::map<sdbus::ObjectPath, Object>& objs,
    bool restoreFromCache)
{
    auto objit = objs.cbegin();
    auto refit = _refs.begin();
    std::string absPath;
    bool newObj;

    while (objit != objs.cend())
    {
        // Find the insertion point or the object to update.
        refit = std::lower_bound(refit, _refs.end(), objit->first,
                                 compareFirst(RelPathCompare(_root)));

        absPath.assign(_root);
        absPath.append(objit->first);

        newObj = false;
        if (refit == _refs.end() || refit->first != absPath)
        {
            refit = _refs.insert(
                refit, std::make_pair(absPath, decltype(_refs)::mapped_type()));
            newObj = true;
            _objs[absPath] = sdbus::createObject(*_connection, sdbus::ObjectPath(absPath));
        }

        updateInterfaces(sdbus::ObjectPath(absPath), objit->second, refit, newObj,
                         restoreFromCache);
#ifdef CREATE_ASSOCIATIONS
        if (newObj)
        {
            _associations.createAssociations(absPath);
        }
#endif
        ++objit;
    }
}

std::any& Manager::getInterfaceHolder(const char* path, const char* interface)
{
    return const_cast<std::any&>(
        const_cast<const Manager*>(this)->getInterfaceHolder(path, interface));
}

const std::any& Manager::getInterfaceHolder(const char* path,
                                            const char* interface) const
{
    std::string p{path};
    auto oit = _refs.find(_root + p);
    if (oit == _refs.end())
        throw std::runtime_error(_root + p + " was not found");

    auto& obj = oit->second;
    auto iit = obj.find(interface);
    if (iit == obj.end())
        throw std::runtime_error("interface was not found");

    return iit->second;
}

void Manager::restore()
{
    if (!fs::exists(fs::path(PIM_PERSIST_PATH)))
    {
        return;
    }

    static const std::string remove =
        std::string(PIM_PERSIST_PATH) + INVENTORY_ROOT;

    std::map<sdbus::ObjectPath, Object> objects;
    for (const auto& dirent :
         fs::recursive_directory_iterator(PIM_PERSIST_PATH))
    {
        const auto& path = dirent.path();
        if (fs::is_regular_file(path))
        {
            auto ifaceName = path.filename().string();
            auto objPath = path.parent_path().string();
            objPath.erase(0, remove.length());
            auto objit = objects.find(sdbus::ObjectPath(objPath));
            Interface propertyMap{};
            if (objects.end() != objit)
            {
                auto& object = objit->second;
                object.emplace(std::move(ifaceName), std::move(propertyMap));
            }
            else
            {
                Object object;
                object.emplace(std::move(ifaceName), std::move(propertyMap));
                objects.emplace(std::move(objPath), std::move(object));
            }
        }
    }
    if (!objects.empty())
    {
        auto restoreFromCache = true;
        updateObjects(objects, restoreFromCache);
#if 0
	    //nlohmann::json jsonObj = objects;
        ////Serialize to a string (argument 4 enables pretty printing)
	    //std::string jsonString = jsonObj.dump(4);

	    //std::cout << jsonString << std::endl;

        auto objit = objects.cbegin();
        auto refit = _refs.begin();
        std::string absPath;
        //bool newObj;

        while (objit != objects.cend()) {

            // Find the insertion point or the object to update.
            refit = std::lower_bound(refit, _refs.end(), objit->first,
                                 compareFirst(RelPathCompare(INVENTORY_ROOT)));

            absPath.assign(INVENTORY_ROOT);
            absPath.append(objit->first);

            //newObj = false;
            if (refit == _refs.end() || refit->first != absPath)
            {
                refit = _refs.insert(
                    refit, std::make_pair(absPath, decltype(_refs)::mapped_type()));
                //newObj = true;
                std::cout << "createObject: " << absPath << std::endl;
                _objs[absPath] = sdbus::createObject(*_connection, sdbus::ObjectPath(absPath));
            }

            printf(" Object Path: %s\n", absPath.c_str());

            // updateInterfaces
            const Object& interfaces = objit->second;
            auto& refaces = refit->second;
            auto ifaceit = interfaces.cbegin();
            auto opsit = _makers.cbegin();
            auto refaceit = refaces.begin();
            while (ifaceit != interfaces.cend())
            {
                std::cout << "  Interface: " << ifaceit->first << std::endl;

                //try 
                {
                    // Find the binding ops for this interface.
                    opsit = std::lower_bound(opsit, _makers.cend(), ifaceit->first,
                                        compareFirst(_makers.key_comp()));

                    if (opsit == _makers.cend() || opsit->first != ifaceit->first)
                    {
                        // This interface is not supported.
                        //throw InterfaceError("Encountered unsupported interface.",
                        //             ifaceit->first);
                        // Reset the binding ops iterator since we are
                        // at the end.
                        opsit = _makers.cbegin();
                        ifaceit++;
                        continue;
                    }

                    // Find the binding insertion point or the binding to update.
                    refaceit = std::lower_bound(refaceit, refaces.end(), ifaceit->first,
                                            compareFirst(refaces.key_comp()));

                    if (refaceit == refaces.end() || refaceit->first != ifaceit->first)
                    {
                        // Add the new interface.
                        auto& ctor = std::get<MakeInterfaceType>(opsit->second);
                        refaceit = refaces.insert(
                            refaceit,
                            std::make_pair(ifaceit->first, ctor(_objs[absPath].get(), ifaceit->second)));
                        //signals.push_back(ifaceit->first);
                    }
                    else
                    {
                        // Set the new property values.
                        auto& assign = std::get<AssignInterfaceType>(opsit->second);
                        assign(ifaceit->second, refaceit->second);
                    }
                    std::cout << "    " << opsit->first << "\n";

                    if (opsit->first == ifaceit->first) {
                        //std::cout << "   " << opsit->first << " " << typeid(opsit->second).name() << std::endl;
                        try {
                            
                            auto& deserialize =
                                std::get<DeserializeInterfaceType<SerialOps>>(                            
                                    opsit->second);
                            
                            //std::cout << "      *** " << deserialize << std::endl;
                            //std::cout << "      --- " << typeid(deserialize).name() << std::endl;
                            
                            //std::cout << "      deserialize: " << absPath << " " << ifaceit->first << std::endl;
                            deserialize(absPath, ifaceit->first, refaceit->second);

                        }
                        catch (const std::bad_variant_access& ex) {
                            std::cout << "Error: " << ex.what() << '\n';
                        }
                    }
                }
                //catch (const InterfaceError& e)
                //{
                //    // Reset the binding ops iterator since we are
                //    // at the end.
                //    opsit = _makers.cbegin();
                //    e.log();
                //}

                ++ifaceit;
                //std::cout << "*****************\n";
            }
            ++objit;
        }
#endif                
    }
}

void Manager::notify(std::map<sdbus::ObjectPath, Object> objs)
{
    updateObjects(objs);
}

int main(int argc, char* argv[])
{
    std::cout << "Starting Inventory Manager...\n";
    try 
    {    
        //create Dbus connection to the system bus
        const char *serviceName = "xyz.openbmc_project.Inventory.Manager";
        //auto connection = sdbus::createSessionBusConnection(sdbus::ServiceName{serviceName});
        /*
        To connect system bus, add /etc/dbus-1/system.d/xyz.openbmc_project.Inventory.Manager.conf
        <!DOCTYPE busconfig PUBLIC
            "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
            "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
        <busconfig>
            <policy user="root">
                <allow own="xyz.openbmc_project.Inventory.Manager"/>
                <allow send_destination="xyz.openbmc_project.Inventory.Manager"/>
            </policy>
        </busconfig>
        */
        auto connection = sdbus::createSystemBusConnection(sdbus::ServiceName{serviceName});

        //create inventory object
        const char *objPath = "/xyz/openbmc_project/inventory";
        auto inventory = sdbus::createObject(*connection, sdbus::ObjectPath{objPath});
        inventory->addObjectManager();

        Manager manager(connection.get());

        auto notify = [&](std::map<sdbus::ObjectPath, Object/*ObjectType<sdbus::Variant>*/> objs) {
            manager.notify(objs);
        };

        inventory->addVTable(sdbus::registerMethod("Notify").implementedAs(std::move(notify))).forInterface("xyz.openbmc_project.Inventory.Manager");
        
        manager.restore();
        
#if 0 
        // system object
        auto system = sdbus::createObject(*connection, sdbus::ObjectPath{"/xyz/openbmc_project/inventory/system"});
    
        // addVtable requires sdbus-c++ version > v2.0.0
        system->addVTable().forInterface(sdbus::InterfaceName{"xyz.openbmc_project.Inventory.Item.System"});
        //system->setInterfaceFlags("xyz.openbmc_project.Inventory.Item.System", sdbus::Flags());

        std::string assetTag = "testAssetType";
        system->addVTable(sdbus::registerProperty("AssetTag")
            .withGetter([&]() {
                return assetTag;
            })
            .withSetter([&](const std::string& value) {
                assetTag = value;
                //system->emitPropertiesChangedSignal("xyz.openbmc_project.Inventory.Decorator.AssetTag", {"AssetTag"});
                system->emitPropertiesChangedSignal(sdbus::InterfaceName{"xyz.openbmc_project.Inventory.Decorator.AssetTag"}, {sdbus::PropertyName{"AssetTag"}});
            })).forInterface("xyz.openbmc_project.Inventory.Decorator.AssetTag");
        
        system->addVTable(sdbus::registerProperty("PrettyName")
            .withGetter([&]() {
                try {
                    const auto& interfaceComposite = _refs.at("/xyz/openbmc_project/inventory/system");
                    const auto& interface = interfaceComposite.at("xyz.openbmc_project.Inventory.Item");
                    // cast std::any back to interface object
                    const auto& object = *std::any_cast<const std::shared_ptr<xyz::openbmc_project::Inventory::Item>&>(interface);
                    return object.prettyName();
                } catch (const std::out_of_range& e) {
                    std::cout << "Error: Key does not exist.\n";
                    return std::string{};
                }
            })
            //.withSetter([&](const std::string& value) {

            //})
            ).forInterface("xyz.openbmc_project.Inventory.Item")
            ;
#endif

             
        
        auto prettyName = manager.invokeMethod<
            xyz::openbmc_project::Inventory::server::Item, 
            decltype(&xyz::openbmc_project::Inventory::server::Item::getPropertyByName)
        >("/system", "xyz.openbmc_project.Inventory.Item", &xyz::openbmc_project::Inventory::server::Item::getPropertyByName, "PrettyName");

        std::cout << "Test invokeMethod getPropertyByName(\"PrettyName\") for /system : " << std::get<std::string>(prettyName) << std::endl;

        std::cout << "Starting Event Loop...\n";
        connection->enterEventLoop();

        std::cout << "Exiting Event Loop...\n";
    }
    catch (const sdbus::Error& error)
    {
        std::cerr << "D-Bus error: "
                  << error.getName()
                  << " - "
                  << error.getMessage()
                  << '\n';
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
