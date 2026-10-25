#include <initguid.h>
#include <windows.h>

// mingw-w64 declares IReference<boolean> beside IReference<BYTE>, the same type in C++
#define ____FIReference_1_boolean_INTERFACE_DEFINED__

#include <roapi.h>
#include <shlwapi.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <sihd/sys/DesktopNotifier.hpp>
#include <sihd/sys/Uuid.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/windows/ComInit.hpp>
#include <sihd/sys/windows/registry.hpp>
#include <sihd/util/str.hpp>

#include "../internal/desktop_notifier.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::UI::Notifications;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

namespace registry = sihd::sys::windows_::registry;

namespace
{

constexpr std::string_view app_ids_key = "Software\\Classes\\AppUserModelId\\";

Error hresult_error(const std::string & what, HRESULT hr)
{
    const std::string text = os::error_str(static_cast<int>(hr));
    // FormatMessage knows no text for many winrt codes (class not registered under wine)
    return Error(io_error, "{}: {}(0x{:08x})", what, text.empty() ? "" : text + " ", static_cast<ULONG>(hr));
}

HStringReference reference(const std::wstring & wide)
{
    return HStringReference(wide.c_str(), static_cast<UINT32>(wide.size()));
}

Result<std::string> file_url(const std::string & path)
{
    const std::wstring wide_path = str::to_wstr(path);
    // percent-encoding at most triples the path, plus the file:/// scheme
    std::wstring url(wide_path.size() * 3 + 16, L'\0');
    DWORD size = static_cast<DWORD>(url.size());
    const HRESULT hr = UrlCreateFromPathW(wide_path.c_str(), url.data(), &size, 0);
    if (FAILED(hr))
        return std::unexpected(hresult_error(fmt::format("could not make an url of '{}'", path), hr));
    url.resize(size);
    return str::to_str(url);
}

Result<std::string> toast_xml(const DesktopNotifier::Notification & notification)
{
    std::string image;
    if (!notification.icon.empty())
    {
        auto url = file_url(notification.icon);
        SIHD_UNEXPECTED_RETURN(url);
        image = fmt::format(R"(<image placement="appLogoOverride" src="{}"/>)", str::escape_xml(*url));
    }
    return fmt::format(R"(<toast duration="{}"><visual><binding template="ToastGeneric"><text>{}</text>)"
                       R"(<text>{}</text>{}</binding></visual>{}</toast>)",
                       notification.urgency == DesktopNotifier::Urgency::critical ? "long" : "short",
                       str::escape_xml(notification.summary),
                       str::escape_xml(notification.body),
                       image,
                       notification.urgency == DesktopNotifier::Urgency::low ? R"(<audio silent="true"/>)" : "");
}

Result<ComPtr<IToastNotificationManagerStatics>> toast_manager()
{
    ComPtr<IToastNotificationManagerStatics> manager;
    const HRESULT hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
        IID_PPV_ARGS(&manager));
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not get the toast notification manager", hr));
    return manager;
}

Result<ComPtr<IXmlDocument>> load_xml(const std::string & xml)
{
    ComPtr<IInspectable> inspectable;
    HRESULT hr = RoActivateInstance(HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(),
                                    &inspectable);
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not create the toast xml document", hr));
    ComPtr<IXmlDocument> document;
    ComPtr<IXmlDocumentIO> document_io;
    hr = inspectable.As(&document);
    if (SUCCEEDED(hr))
        hr = document.As(&document_io);
    if (FAILED(hr))
        return std::unexpected(hresult_error("not a toast xml document", hr));

    const std::wstring wide = str::to_wstr(xml);
    hr = document_io->LoadXml(reference(wide).Get());
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not load the toast xml", hr));
    return document;
}

Result<ComPtr<IToastNotification>>
    create_toast(IXmlDocument *document, const std::wstring & tag, const std::wstring & group)
{
    ComPtr<IToastNotificationFactory> factory;
    HRESULT hr = RoGetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
                                        IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not get the toast notification factory", hr));
    ComPtr<IToastNotification> toast;
    hr = factory->CreateToastNotification(document, &toast);
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not create the toast notification", hr));

    ComPtr<IToastNotification2> tagged;
    hr = toast.As(&tagged);
    if (SUCCEEDED(hr))
        hr = tagged->put_Tag(reference(tag).Get());
    if (SUCCEEDED(hr))
        hr = tagged->put_Group(reference(group).Get());
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not tag the toast notification", hr));
    return toast;
}

} // namespace

struct DesktopNotifier::Impl
{
        std::string app_name;
        std::string app_id;
        // toast ids are per notifier: its own group keeps close() off the toasts of another notifier
        std::wstring group = str::to_wstr(Uuid().str());
        bool registered = false;
        uint32_t next_id = 1;

        Result<void> start_runtime(std::optional<windows_::ComInit> & com) const
        {
            if (app_id.empty())
                return std::unexpected(Error(invalid_argument, "a windows desktop notification needs an app id"));
            try
            {
                com.emplace(windows_::ComInit::Threading::multithreaded);
            }
            catch (const std::runtime_error & error)
            {
                return std::unexpected(Error(io_error, "{}", error.what()));
            }
            return {};
        }

        Result<void> register_app_id()
        {
            if (registered)
                return {};
            registry::Key key;
            auto created = key.create(registry::Hive::current_user, std::string(app_ids_key) + app_id);
            SIHD_UNEXPECTED_RETURN(created);
            if (!app_name.empty())
            {
                auto named = key.set_string("DisplayName", app_name);
                SIHD_UNEXPECTED_RETURN(named);
            }
            registered = true;
            return {};
        }
};

Result<uint32_t> DesktopNotifier::notify(const Notification & notification)
{
    auto valid_timeout = internal::check_timeout(notification.timeout);
    SIHD_UNEXPECTED_RETURN(valid_timeout);
    std::optional<windows_::ComInit> com;
    auto started = _impl->start_runtime(com);
    SIHD_UNEXPECTED_RETURN(started);
    auto registered = _impl->register_app_id();
    SIHD_UNEXPECTED_RETURN(registered);

    auto xml = toast_xml(notification);
    SIHD_UNEXPECTED_RETURN(xml);
    auto document = load_xml(*xml);
    SIHD_UNEXPECTED_RETURN(document);
    const uint32_t id = _impl->next_id;
    auto toast = create_toast(document->Get(), std::to_wstring(id), _impl->group);
    SIHD_UNEXPECTED_RETURN(toast);
    auto manager = toast_manager();
    SIHD_UNEXPECTED_RETURN(manager);

    const std::wstring app_id = str::to_wstr(_impl->app_id);
    ComPtr<IToastNotifier> notifier;
    HRESULT hr = (*manager)->CreateToastNotifierWithId(reference(app_id).Get(), &notifier);
    if (SUCCEEDED(hr))
        hr = notifier->Show(toast->Get());
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not show the toast notification", hr));
    ++_impl->next_id;
    return id;
}

Result<void> DesktopNotifier::close(uint32_t id)
{
    std::optional<windows_::ComInit> com;
    auto started = _impl->start_runtime(com);
    SIHD_UNEXPECTED_RETURN(started);
    auto manager = toast_manager();
    SIHD_UNEXPECTED_RETURN(manager);

    const std::wstring app_id = str::to_wstr(_impl->app_id);
    const std::wstring tag = std::to_wstring(id);
    ComPtr<IToastNotificationManagerStatics2> manager_with_history;
    ComPtr<IToastNotificationHistory> history;
    HRESULT hr = manager->As(&manager_with_history);
    if (SUCCEEDED(hr))
        hr = manager_with_history->get_History(&history);
    if (SUCCEEDED(hr))
        hr = history->RemoveGroupedTagWithId(reference(tag).Get(),
                                             reference(_impl->group).Get(),
                                             reference(app_id).Get());
    if (FAILED(hr))
        return std::unexpected(hresult_error("could not remove the toast notification", hr));
    return {};
}

DesktopNotifier::DesktopNotifier(): _impl(std::make_unique<Impl>()) {}

DesktopNotifier::DesktopNotifier(DesktopNotifier && other) = default;

DesktopNotifier & DesktopNotifier::operator=(DesktopNotifier && other) = default;

DesktopNotifier::~DesktopNotifier() = default;

void DesktopNotifier::set_app_name(const std::string & name)
{
    _impl->app_name = name;
    _impl->registered = false;
}

void DesktopNotifier::set_app_id(const std::string & id)
{
    _impl->app_id = id;
    _impl->registered = false;
}

} // namespace sihd::sys
