#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

/*
from xyz/openbmc_project/Inventory/Manager.interface.yaml
parameters:
    - name: object
      type: dict[path,dict[string,dict[string,variant[boolean,int64,string,array[byte]]]]]
      description: >
          A dictionary of fully enumerated items to be managed.
*/
/** @brief Inventory manager supported property types. */
using InterfaceVariantType =
    std::variant<bool, int64_t, std::string, std::vector<uint8_t>>;

template <typename T>
using InterfaceType = std::map<std::string, T>;

template <typename T>
using ObjectType = std::map<std::string, InterfaceType<T>>;

using Interface = InterfaceType<InterfaceVariantType>;
using Object = ObjectType<InterfaceVariantType>;