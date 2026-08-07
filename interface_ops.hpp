#include "types.hpp"
#include "utils.hpp"

template <typename T>
struct HasProperties
{
  private:
    using yes = char;
    struct no
    {
        char array[2];
    };

    template <typename U>
    static constexpr yes test(typename U::PropertiesVariant*);
    template <typename U>
    static constexpr no test(...);

  public:
    static constexpr auto value = sizeof(test<T>(0)) == sizeof(yes);
};

template <typename T, typename Enable = void>
struct MakeInterface
{
    static std::any op(sdbus::IObject* obj, const Interface&)
    {
        return std::any(std::make_shared<T>(obj));
    }
};

template <typename T>
struct MakeInterface<T, std::enable_if_t<HasProperties<T>::value>>
{
    static std::any op(sdbus::IObject* obj, const Interface& props)
    {
        using InterfaceVariant =
            std::map<std::string, typename T::PropertiesVariant>;

        InterfaceVariant v;
        for (const auto& p : props)
        {
            v.emplace(p.first,
                      convertVariant<typename T::PropertiesVariant>(p.second));
        }

        return std::any(std::make_shared<T>(obj, v));
    }
};

template <typename T, typename Enable = void>
struct AssignInterface
{
    static void op(const Interface&, std::any&)
    {
    }
};

template <typename T>
struct AssignInterface<T, std::enable_if_t<HasProperties<T>::value>>
{
    static void op(const Interface& props, std::any& holder)
    {
        auto& iface = *std::any_cast<std::shared_ptr<T>&>(holder);
        for (const auto& p : props)
        {
            iface.setPropertyByName(
                p.first,
                convertVariant<typename T::PropertiesVariant>(p.second));
        }
    }
};

template <typename T, typename Ops, typename Enable = void>
struct SerializeInterface
{
    static void op(const std::string& path, const std::string& iface,
                   const std::any&)
    {
        Ops::serialize(path, iface);
    }
};

template <typename T, typename Ops>
struct SerializeInterface<T, Ops, std::enable_if_t<HasProperties<T>::value>>
{
    static void op(const std::string& path, const std::string& iface,
                   const std::any& holder)
    {
        const auto& object = *std::any_cast<const std::shared_ptr<T>&>(holder);
        Ops::serialize(path, iface, object);
    }
};

template <typename T, typename Ops, typename Enable = void>
struct DeserializeInterface
{
    static void op(const std::string& path, const std::string& iface, std::any&)
    {
        Ops::deserialize(path, iface);
    }
};

template <typename T, typename Ops>
struct DeserializeInterface<T, Ops, std::enable_if_t<HasProperties<T>::value>>
{
    static void op(const std::string& path, const std::string& iface,
                   std::any& holder)
    {
        //printf("DeserializeInterface::op %s %s\n", path.c_str(), iface.c_str());
        auto& object = *std::any_cast<std::shared_ptr<T>&>(holder);
        Ops::deserialize(path, iface, object);
    }
};

struct DummyInterface
{
};
using MakeInterfaceType =
    std::add_pointer_t<decltype(MakeInterface<DummyInterface>::op)>;
using AssignInterfaceType =
    std::add_pointer_t<decltype(AssignInterface<DummyInterface>::op)>;
template <typename Ops>
using SerializeInterfaceType =
    std::add_pointer_t<decltype(SerializeInterface<DummyInterface, Ops>::op)>;
template <typename Ops>
using DeserializeInterfaceType =
    std::add_pointer_t<decltype(DeserializeInterface<DummyInterface, Ops>::op)>;
