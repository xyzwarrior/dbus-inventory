#pragma once
#include <map>
#include <string>
#include <sdbus-c++/sdbus-c++.h>
#include <tuple>
#include <variant>
#include <cereal/types/map.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>
//#include <cereal/archives/json.hpp>

// Emitting signals prior to claiming a well known DBus service name causes
// un-necessary DBus traffic and wakeups.  De-serialization only happens prior
// to claiming a well known name, so don't emit signals.
#ifndef _SKIP_SIGNALS
#define _SKIP_SIGNALS
static constexpr auto skipSignals = true;
#endif

% for m in interface.methods + interface.properties + interface.signals:
${ m.cpp_prototype(loader, interface=interface, ptype='callback-hpp-includes') }
% endfor
<%
    namespaces = interface.name.split('.')
    classname = namespaces.pop()

    def setOfPropertyTypes():
        return set(p.cppTypeParam(interface.name) for p in
                   interface.properties);

    def cppNamespace():
        return "::".join(namespaces) + "::server::" + classname
%>

    % for s in namespaces:
namespace ${s}
{
    % endfor
namespace server
{

class ${classname}
{
    public:
        /* Define all of the basic class operations:
         *     Not allowed:
         *         - Default constructor to avoid nullptrs.
         *         - Copy operations due to internal unique_ptr.
         *         - Move operations due to 'this' being registered as the
         *           'context' with sdbus.
         *     Allowed:
         *         - Destructor.
         */
        ${classname}() = delete;
        ${classname}(const ${classname}&) = delete;
        ${classname}& operator=(const ${classname}&) = delete;
        ${classname}(${classname}&&) = delete;
        ${classname}& operator=(${classname}&&) = delete;
        virtual ~${classname}() = default;

        /** @brief Constructor to put object onto bus at a dbus path.
         *  @param[in] bus - Bus to attach to.
         *  @param[in] path - Path to attach at.
         */
        ${classname}(sdbus::IObject* obj);

    % for e in interface.enums:
        enum class ${e.name}
        {
        % for v in e.values:
            ${v.name},
        % endfor
        };
    % endfor

    % if interface.properties:
        using PropertiesVariant = std::variant<
                ${",\n                ".join(setOfPropertyTypes())}>;

        /** @brief Constructor to initialize the object from a map of
         *         properties.
         *
         *  @param[in] bus - Bus to attach to.
         *  @param[in] path - Path to attach at.
         *  @param[in] vals - Map of property name to value for initialization.
         */
        ${classname}(sdbus::IObject* obj,
                     const std::map<std::string, PropertiesVariant>& vals,
                     bool skipSignal = false);

    % endif
    % for m in interface.methods:
${ m.cpp_prototype(loader, interface=interface, ptype='header') }
    % endfor

    % for s in interface.signals:
${ s.cpp_prototype(loader, interface=interface, ptype='header') }
    % endfor

    % for p in interface.properties:
        /** Get value of ${p.name} */
        virtual ${p.cppTypeParam(interface.name)} ${p.camelCase}() const;
        /** Set value of ${p.name} with option to skip sending signal */
        virtual ${p.cppTypeParam(interface.name)} \
${p.camelCase}(${p.cppTypeParam(interface.name)} value,
               bool skipSignal);
        /** Set value of ${p.name} */
        virtual ${p.cppTypeParam(interface.name)} \
${p.camelCase}(${p.cppTypeParam(interface.name)} value);
    % endfor

    % if interface.properties:
        /** @brief Sets a property by name.
         *  @param[in] _name - A string representation of the property name.
         *  @param[in] val - A variant containing the value to set.
         */
        void setPropertyByName(const std::string& _name,
                               const PropertiesVariant& val,
                               bool skipSignal = false);

        /** @brief Gets a property by name.
         *  @param[in] _name - A string representation of the property name.
         *  @return - A variant containing the value of the property.
         */
        PropertiesVariant getPropertyByName(const std::string& _name);

    % endif
    % for e in interface.enums:
        /** @brief Convert a string to an appropriate enum value.
         *  @param[in] s - The string to convert in the form of
         *                 "${interface.name}.<value name>"
         *  @return - The enum value.
         */
        static ${e.name} convert${e.name}FromString(const std::string& s);

        /** @brief Convert an enum value to a string.
         *  @param[in] e - The enum to convert to a string.
         *  @return - The string conversion in the form of
         *            "${interface.name}.<value name>"
         */
        static std::string convert${e.name}ToString(${e.name} e);
    % endfor        

        static constexpr auto interface = "${interface.name}";

        // save archive (must be const)
        template<class Archive>
        void save([[maybe_unused]] Archive& a) const
        {
        % for p in interface.properties:
<% t = "cereal::make_nvp(\"" + p.CamelCase + "\", " + p.camelCase + "())"%>\
            a(${t});
        % endfor
        }
        // load archive (non-const)
        template<class Archive>
        void load(Archive& a)
        {
        % for p in interface.properties:
<% t = p.camelCase + "()" %>\
            decltype(${t}) ${p.CamelCase}{};
        % endfor
    
        % for p in interface.properties:
<% t = "cereal::make_nvp(\"" + p.CamelCase + "\", " + p.CamelCase + ")" %>\
            try
            {
                a(${t});
            }
            catch (const cereal::Exception &e)
            {
                // Ignore any exceptions, property value stays default
            }
        % endfor
    
        % for p in interface.properties:
<% t = p.camelCase + "(" + p.CamelCase + ", skipSignals)" %>\
            ${t};
        % endfor
        }

    private:
        sdbus::IObject* _obj;
    % for m in interface.methods:
${ m.cpp_prototype(loader, interface=interface, ptype='callback-header') }
    % endfor
        
    % for p in interface.properties:
        % if p.defaultValue is not None:
        ${p.cppTypeParam(interface.name)} _${p.camelCase} = \
            % if p.is_enum():
${p.cppTypeParam(interface.name)}::\
            % endif
${p.defaultValue};
        % else:
        ${p.cppTypeParam(interface.name)} _${p.camelCase}{};
        % endif
    % endfor

};

    % for e in interface.enums:
/* Specialization of sdbusplus::server::bindings::details::convertForMessage
 * for enum-type ${classname}::${e.name}.
 *
 * This converts from the enum to a constant c-string representing the enum.
 *
 * @param[in] e - Enum value to convert.
 * @return C-string representing the name for the enum value.
 */
inline std::string convertForMessage(${classname}::${e.name} e)
{
    return ${classname}::convert${e.name}ToString(e);
}
    % endfor

} // namespace server    
    % for s in reversed(namespaces):
} // namespace ${s}
    % endfor

namespace message
{
namespace details
{
    % for e in interface.enums:
template <>
inline auto convert_from_string<${cppNamespace()}::${e.name}>(
        const std::string& value)
{
    return ${cppNamespace()}::convert${e.name}FromString(value);
}

template <>
inline std::string convert_to_string<${cppNamespace()}::${e.name}>(
        ${cppNamespace()}::${e.name} value)
{
    return ${cppNamespace()}::convert${e.name}ToString(value);
}
    % endfor
} // namespace details
} // namespace message
#if 0
#ifndef CLASS_VERSION
//#define CLASS_VERSION 1
#define CLASS_VERSION 2
#endif
CEREAL_CLASS_VERSION(${'::'.join(namespaces)}::server::${classname}, CLASS_VERSION);

namespace cereal
{
#ifndef _CLASS_VERSION_WITH_NVP
#define _CLASS_VERSION_WITH_NVP
// The version we started using cereal NVP from
static constexpr size_t CLASS_VERSION_WITH_NVP = 2;
#endif

template<class Archive>
void _save([[maybe_unused]] Archive& a,
          [[maybe_unused]] const ${'::'.join(namespaces)}::server::${classname}& object,
          const std::uint32_t /* version */)
{
% for p in interface.properties:
<% t = "cereal::make_nvp(\"" + p.CamelCase + "\", object." + p.camelCase + "())"
%>\
    a(${t});
% endfor    
}

template<class Archive>
void _load(Archive& a,
          [[maybe_unused]] ${'::'.join(namespaces)}::server::${classname}& object,
          const std::uint32_t version)
{
% for p in interface.properties:
<% t = "object." + p.camelCase + "()" %>\
    decltype(${t}) ${p.CamelCase}{};
% endfor
    if (version < CLASS_VERSION_WITH_NVP)
    {
<%
    props = ', '.join([p.CamelCase for p in interface.properties])
%>\
        a(${props});
    }
    else
    {
% for p in interface.properties:
<% t = "cereal::make_nvp(\"" + p.CamelCase + "\", " + p.CamelCase + ")" %>\
        try
        {
            a(${t});
        }
        catch (const Exception &e)
        {
            // Ignore any exceptions, property value stays default
        }
% endfor
    }
% for p in interface.properties:
<% t = "object." + p.camelCase + "(" + p.CamelCase + ", skipSignals)" %>\
    ${t};
% endfor
}

} // namespace cereal
#endif
