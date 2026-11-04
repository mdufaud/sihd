#include <sihd/pcap/CapturedPacket.hpp>
#include <sihd/pcap/DevCapture.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/time.hpp>

namespace sihd::pcap
{

SIHD_LOGGER;

using namespace sihd::util;
using namespace sihd::core;

SIHD_REGISTER_FACTORY(DevCapture)

DevCapture::DevCapture(const std::string & name, Node *parent):
    Device(name, parent),
    _realtime(false),
    _promiscuous(false),
    _monitor(false),
    _immediate(true),
    _timeout_ms(0),
    _datalink(0),
    _active(false),
    _stopping(false),
    _last_stats_time(0),
    _activate(nullptr),
    _packet(nullptr),
    _datalink_out(nullptr),
    _status(nullptr),
    _kernel_received(nullptr),
    _kernel_dropped(nullptr)
{
    _worker.set_method([this] { return this->_capture(); });
    this->set_realtime(false);
    this->set_promiscuous(false);
    this->set_monitor(false);
    this->set_immediate(true);
    this->set_timeout(100);
    this->set_stats_period(1000);

    this->add_conf("interface", &DevCapture::set_interface);
    this->add_conf("file", &DevCapture::set_file);
    this->add_conf("realtime", &DevCapture::set_realtime);
    this->add_conf("promiscuous", &DevCapture::set_promiscuous);
    this->add_conf("monitor", &DevCapture::set_monitor);
    this->add_conf("immediate", &DevCapture::set_immediate);
    this->add_conf("timeout", &DevCapture::set_timeout);
    this->add_conf("stats_period", &DevCapture::set_stats_period);
}

DevCapture::~DevCapture()
{
    if (this->is_running())
        this->stop();
}

bool DevCapture::is_running() const
{
    return _worker.is_worker_running();
}

bool DevCapture::set_interface(const std::string & interface_name)
{
    if (interface_name.empty() || _file.empty() == false)
        return false;
    _interface = interface_name;
    return true;
}

bool DevCapture::set_file(const std::string & path)
{
    if (path.empty() || _interface.empty() == false)
        return false;
    _file = path;
    return true;
}

bool DevCapture::set_realtime(bool active)
{
    _realtime = active;
    return true;
}

bool DevCapture::set_promiscuous(bool active)
{
    _promiscuous = active;
    return true;
}

bool DevCapture::set_monitor(bool active)
{
    _monitor = active;
    return true;
}

bool DevCapture::set_immediate(bool active)
{
    _immediate = active;
    return true;
}

bool DevCapture::set_timeout(int ms)
{
    if (ms < 0)
        return false;
    _timeout_ms = ms;
    return true;
}

bool DevCapture::set_stats_period(int ms)
{
    if (ms <= 0)
        return false;
    _stats_period = Duration(time::milli(ms));
    return true;
}

bool DevCapture::on_init()
{
    this->add_unlinked_channel("activate", TYPE_BOOL, 1);
    _packet = this->add_channel_resizable("packet", TYPE_UBYTE, 0, CapturedPacket::header_size + max_packet_size);
    _datalink_out = this->add_channel("datalink", TYPE_INT, 1);
    _status = this->add_channel("status", TYPE_UBYTE, 1);
    _kernel_received = this->add_channel("kernel_received", TYPE_UINT, 1);
    _kernel_dropped = this->add_channel("kernel_dropped", TYPE_UINT, 1);
    if (_packet == nullptr || _datalink_out == nullptr || _status == nullptr || _kernel_received == nullptr
        || _kernel_dropped == nullptr)
        return false;
    // two identical packets are two packets
    _packet->set_write_on_change(false);
    return true;
}

bool DevCapture::_open_source()
{
    if (_file.empty() == false)
    {
        _reader.emplace(this->name() + "_reader");
        Result<void> opened = _reader->open(_file);
        if (SIHD_UNEXPECTED_LOG(opened))
            return false;
        _datalink = _reader->datalink();
        return true;
    }

    if (_interface.empty())
    {
        SIHD_LOG(error, "DevCapture: {} has neither an interface nor a file", this->name());
        return false;
    }
    _sniffer.emplace(this->name() + "_sniffer");
    Result<void> opened = _sniffer->open(_interface);
    if (SIHD_UNEXPECTED_LOG(opened))
        return false;
    if (_sniffer->set_timeout(_timeout_ms) == false || _sniffer->set_immediate(_immediate) == false
        || _sniffer->set_promiscuous(_promiscuous) == false || _sniffer->set_monitor(_monitor) == false)
        return false;
    Result<void> activated = _sniffer->activate();
    if (SIHD_UNEXPECTED_LOG(activated))
        return false;
    _datalink = _sniffer->datalink();
    return true;
}

bool DevCapture::on_start()
{
    _activate = this->get_channel("activate").value_or(nullptr);
    if (_activate == nullptr || this->_open_source() == false)
    {
        _sniffer.reset();
        _reader.reset();
        return false;
    }

    {
        auto guard = _waitable.guard();
        _active = _activate->read<bool>(0);
        _stopping = false;
    }
    _last_capture_time.reset();
    _datalink_out->write<int32_t>(0, _datalink);
    this->_set_status(Status::idle);
    this->observe_channel(_activate);

    if (_worker.start_sync_worker(this->name()) == false)
    {
        SIHD_LOG(error, "DevCapture: {} could not start its thread", this->name());
        return false;
    }
    return true;
}

bool DevCapture::on_stop()
{
    this->remove_channels_observation();
    {
        auto guard = _waitable.guard();
        _stopping = true;
    }
    _waitable.notify_all();
    if (_sniffer)
        _sniffer->interrupt();
    const bool stopped = _worker.stop_worker();
    _sniffer.reset();
    _reader.reset();
    return stopped;
}

void DevCapture::handle(Channel *channel)
{
    if (channel != _activate)
        return;
    {
        auto guard = _waitable.guard();
        _active = channel->read<bool>(0);
    }
    _waitable.notify_all();
}

bool DevCapture::_capture()
{
    while (true)
    {
        bool active;
        {
            auto guard = _waitable.guard();
            active = _active;
            if (_stopping)
                break;
        }
        if (active == false)
        {
            this->_set_status(Status::paused);
            _waitable.wait([this] { return _active || _stopping; });
            continue;
        }
        this->_set_status(Status::running);

        Result<bool> read = this->_source().read_next();
        if (SIHD_UNEXPECTED_LOG(read))
        {
            this->_set_status(Status::failed);
            break;
        }
        if (*read)
            this->_publish_packet();
        else if (_reader)
        {
            this->_set_status(Status::end_of_file);
            break;
        }
        if (_sniffer && Timestamp::now() - _last_stats_time >= _stats_period)
            this->_publish_kernel_stats();
    }
    return true;
}

IReaderTimestamp & DevCapture::_source()
{
    if (_reader)
        return *_reader;
    return *_sniffer;
}

bool DevCapture::_wait_capture_pace(Timestamp timestamp)
{
    if (_reader.has_value() == false || _realtime == false)
        return true;
    if (_last_capture_time.has_value() && timestamp > *_last_capture_time)
        _waitable.wait_for(Duration(timestamp - *_last_capture_time), [this] { return _stopping; });
    _last_capture_time = timestamp;
    auto guard = _waitable.guard();
    return _stopping == false;
}

void DevCapture::_publish_packet()
{
    IReaderTimestamp & source = this->_source();
    ArrCharView data;
    source.get_read_data(data);
    const Timestamp capture_time = source.get_read_timestamp().value_or(Timestamp(0));
    const size_t original_length = _reader ? _reader->packet_len() : _sniffer->packet_len();
    if (this->_wait_capture_pace(capture_time) == false)
        return;

    const CapturedPacket packet(capture_time,
                                static_cast<uint32_t>(original_length),
                                _datalink,
                                ArrUByteView(data.data(), data.byte_size()));
    if (_packet->write(ArrByteView(packet.encode())) == false)
        SIHD_LOG(warning, "DevCapture: {} could not write a packet of {} bytes", this->name(), data.size());
}

void DevCapture::_publish_kernel_stats()
{
    _last_stats_time = Timestamp::now();
    std::optional<Sniffer::Stats> stats = _sniffer->stats();
    if (!stats)
        return;
    _kernel_received->write<uint32_t>(0, stats->received);
    _kernel_dropped->write<uint32_t>(0, stats->dropped + stats->if_dropped);
}

void DevCapture::_set_status(Status status)
{
    _status->write<uint8_t>(0, static_cast<uint8_t>(status));
}

} // namespace sihd::pcap
