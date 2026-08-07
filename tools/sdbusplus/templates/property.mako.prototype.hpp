<%
    namespaces = interface.name.split('.')
    classname = namespaces.pop()

    def interface_instance():
        return "_".join(interface.name.split('.') + ['interface'])

    def error_namespace(e):
        n = e.split('.');
        n.pop(); # Remove error name.

        n = map((lambda x: interface.name if x == "self" else x), n);
        return '::'.join('.'.join(n).split('.'));

    def error_name(e):
        return e.split('.').pop();

    def error_include(e):
        l = error_namespace(e).split('::')
        l.pop() # Remove "Error"
        return '/'.join(l) + '/error.hpp';
%>
% if ptype == 'callback-cpp':
auto ${classname}::${property.camelCase}() const ->
        ${property.cppTypeParam(interface.name)}
{
    return _${property.camelCase};
}

auto ${classname}::${property.camelCase}(${property.cppTypeParam(interface.name)} value,
                                         bool skipSignal) ->
        ${property.cppTypeParam(interface.name)}
{
    if (_${property.camelCase} != value)
    {
        _${property.camelCase} = value;
        if (!skipSignal)
        {
            //_${interface_instance()}.property_changed("${property.name}");
            _obj->emitPropertiesChangedSignal(interface, {sdbus::PropertyName("${property.name}")});
        }
    }

    return _${property.camelCase};
}

auto ${classname}::${property.camelCase}(${property.cppTypeParam(interface.name)} val) ->
        ${property.cppTypeParam(interface.name)}
{
    return ${property.camelCase}(val, false);
}

% elif ptype == 'callback-cpp-includes':
        % for e in property.errors:
#include <${error_include(e)}>
        % endfor
% elif ptype == 'callback-hpp-includes':
        % for i in interface.enum_includes([property]):
#include <${i}>
        % endfor
% endif
