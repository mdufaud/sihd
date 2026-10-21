#include <windows.h>

#include <array>
#include <cstring>
#include <string>

namespace
{

struct Service
{
        SERVICE_STATUS_HANDLE status_handle = nullptr;
        HANDLE stop_event = nullptr;
        DWORD stop_delay_ms = 0;
};

Service service;

void report_state(DWORD state)
{
    SERVICE_STATUS status {};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP : 0;
    SetServiceStatus(service.status_handle, &status);
}

DWORD WINAPI on_control(DWORD control, DWORD, void *, void *)
{
    if (control == SERVICE_CONTROL_STOP)
    {
        report_state(SERVICE_STOP_PENDING);
        SetEvent(service.stop_event);
    }
    return NO_ERROR;
}

void WINAPI service_main(DWORD, LPSTR *argv)
{
    service.status_handle = RegisterServiceCtrlHandlerExA(argv[0], on_control, nullptr);
    if (service.status_handle == nullptr)
        return;
    service.stop_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    report_state(SERVICE_RUNNING);
    WaitForSingleObject(service.stop_event, INFINITE);
    CloseHandle(service.stop_event);
    Sleep(service.stop_delay_ms);
    report_state(SERVICE_STOPPED);
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    if (argc >= 3 && strcmp(argv[2], "slow-stop") == 0)
        service.stop_delay_ms = 2000;
    std::string name = argv[1];
    std::array<SERVICE_TABLE_ENTRYA, 2> table {{{name.data(), service_main}, {nullptr, nullptr}}};
    return StartServiceCtrlDispatcherA(table.data()) == 0 ? 1 : 0;
}
