<%
    interfaces = interfacelist.split(';')

    def interface_namespace(i):
        a = i.split('.')
        return '::'.join(a[:-1] + ['server', a[-1]])
%>
#include <manager.hpp>
% for i in interfaces:
#include <${i}.hpp>
% endfor

using namespace std::literals::string_literals;

const Manager::Makers Manager::_makers{
% for i in interfaces:
    {
        "${str(i)}",
        std::make_tuple(
            MakeInterface<
                ServerObject<
                    ${interface_namespace(i)}>>::op,
            AssignInterface<
                ServerObject<
                    ${interface_namespace(i)}>>::op,
            SerializeInterface<
                ServerObject<
                    ${interface_namespace(i)}>, SerialOps>::op,
            DeserializeInterface<
                ServerObject<
                    ${interface_namespace(i)}>, SerialOps>::op
        )
    },
% endfor
};