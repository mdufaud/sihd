#ifndef __SIHD_BT_ASSIGNEDNUMBERS_HPP__
#define __SIHD_BT_ASSIGNEDNUMBERS_HPP__

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::bt
{

// the names of the bluetooth sig assigned numbers, from the user's copy of its files; a failed load keeps the table
class AssignedNumbers
{
    public:
        AssignedNumbers();
        ~AssignedNumbers() = default;

        // assigned_numbers/company_identifiers/company_identifiers.yaml
        sihd::util::Result<void> load_company_identifiers(const std::string & path);
        // assigned_numbers/uuids/member_uuids.yaml
        sihd::util::Result<void> load_member_uuids(const std::string & path);
        // assigned_numbers/uuids/service_uuids.yaml
        sihd::util::Result<void> load_service_uuids(const std::string & path);

        std::optional<std::string> company(uint16_t id) const;
        std::optional<std::string> member(uint16_t uuid) const;
        std::optional<std::string> service(uint16_t uuid) const;

        // the manufacturer data company, the owner of an advertised member uuid, then the owner of a known beacon
        std::optional<std::string> vendor(const ScanResult & result) const;

    private:
        std::unordered_map<uint16_t, std::string> _companies;
        std::unordered_map<uint16_t, std::string> _members;
        std::unordered_map<uint16_t, std::string> _services;
};

} // namespace sihd::bt

#endif
