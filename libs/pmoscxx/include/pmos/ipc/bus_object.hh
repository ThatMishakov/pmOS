#pragma once

#include <cstdint>
#include <optional>
#include <pmos/ipc.h>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace pmos::ipc
{

class BUSObject
{
public:
    using property = std::variant<std::string, uint64_t>;

    /// Sets the property to the desired value (replacing the previous value if it had already been set)
    void set_property(std::string_view name, std::variant<std::string_view, uint64_t> p);

    /// Sets the name of this busobject
    void set_name(std::string name);

    const std::string &get_name() const;

    std::optional<property> get_property(std::string_view name);

    /// Serializes the BUSObject into IPC_BUS_Publish_Object message
    std::vector<uint8_t> serialize();
    std::vector<uint8_t> serialize_into_ipc();

    static std::pair<BUSObject, uint64_t> deserialize(const IPC_BUS_Request_Object_Reply *reply, uint64_t message_length);
private:
    std::string name;
    std::unordered_map<std::string, property> properties;

    template<class... Ts>
    struct overloads : Ts... { using Ts::operator()...; };

    std::vector<uint8_t> serialize_properties();
};

class EqualsFilter;
class Conjunction;
class Disjunction;
class NoFilter;

using AnyFilter = std::variant<
    EqualsFilter,
    Conjunction,
    Disjunction,
    NoFilter>;

class NoFilter {};

class EqualsFilter
{
private:
    std::string name_;
    std::string value_;
public:
    EqualsFilter(std::string name, std::string value): name_(std::move(name)), value_(std::move(value)) {}

    const std::string &name() const { return name_; }
    const std::string &value() const { return value_; }
};

class Conjunction
{
private:
    std::vector<AnyFilter> operands_;
public:
    Conjunction(std::vector<AnyFilter> operands);
    
    std::vector<AnyFilter> &operands() { return operands_; }
    const std::vector<AnyFilter> &operands() const { return operands_; }
};

class Disjunction
{
private:
    std::vector<AnyFilter> operands_;
public:
    Disjunction(std::vector<AnyFilter> operands);

    std::vector<AnyFilter> &operands() { return operands_; }
    const std::vector<AnyFilter> &operands() const { return operands_; }
};

std::vector<uint8_t> serialize_filter(const AnyFilter &filter);
std::vector<uint8_t> serialize_filter_ipc(const AnyFilter &filter, uint64_t from_sequence_number);

size_t filter_serialized_size(const AnyFilter &filter);

} // namespace pmos::ipc