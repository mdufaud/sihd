#ifndef __SIHD_NET_OUIREGISTRY_HPP__
#define __SIHD_NET_OUIREGISTRY_HPP__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

// the IEEE MA-L registry: the vendor owning the first 3 octets of a universally administered address
class OuiRegistry
{
    public:
        OuiRegistry();
        explicit OuiRegistry(const std::string & path);
        ~OuiRegistry() = default;

        // reads an IEEE oui.txt, like /usr/share/hwdata/oui.txt; a failure keeps the registry
        sihd::util::Result<void> load(const std::string & path);

        // nullopt for a locally administered or group address
        std::optional<std::string> vendor(sihd::util::ArrUByteView address) const;
        // aa:bb:cc:dd:ee:ff
        std::optional<std::string> vendor(std::string_view text) const;

    private:
        std::unordered_map<uint32_t, std::string> _vendors;
};

} // namespace sihd::net

#endif
