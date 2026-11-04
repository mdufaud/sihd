#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include <fmt/format.h>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/core/Channel.hpp>
#include <sihd/tui/TreeMonitorComponent.hpp>
#include <sihd/tui/channel_view.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/ServiceController.hpp>

namespace sihd::tui
{

using namespace ftxui;
using namespace sihd::util;
using namespace sihd::core;

namespace
{

struct Row
{
        std::string path;
        // the full name events carry: under a link alias it differs from path
        std::string source;
        std::string key;
        size_t depth = 0;
};

Color state_color(ServiceController::State state)
{
    switch (state)
    {
        case ServiceController::Running:
            return Color::Green;
        case ServiceController::Stopped:
            return Color::Yellow;
        case ServiceController::StartFailure:
        case ServiceController::StopFailure:
        case ServiceController::Failure:
            return Color::Red;
        default:
            return Color::GrayLight;
    }
}

class TreeView: public ComponentBase,
                public IHandler<TreeMonitorEvent *>
{
    public:
        TreeView(TreeMonitor & monitor, Refresher & refresher): _monitor(monitor), _refresher(refresher)
        {
            // subscribed first: what attaches during the walk is merged, never missed
            _monitor.add_observer(this);
            this->_build();
        }

        ~TreeView() override { _monitor.remove_observer(this); }

        void handle(TreeMonitorEvent *event) override
        {
            {
                std::lock_guard lock(_mutex);
                const std::string & path = *event->path;
                switch (event->kind)
                {
                    case TreeMonitorEvent::channel_attached:
                        this->_insert(path);
                        if (event->value != nullptr)
                            _values[path] = array_str(*event->value);
                        break;
                    case TreeMonitorEvent::service_attached:
                        this->_insert(path);
                        _states[path] = *event->state;
                        break;
                    case TreeMonitorEvent::channel_detached:
                    case TreeMonitorEvent::service_detached:
                        this->_erase(path);
                        break;
                    case TreeMonitorEvent::write_begin:
                        if (event->value != nullptr)
                            _values[path] = array_str(*event->value);
                        _writes[path] += 1;
                        break;
                    case TreeMonitorEvent::op_begin:
                    case TreeMonitorEvent::op_end:
                        _states[path] = *event->state;
                        break;
                    default:
                        return;
                }
            }
            _refresher.request();
        }

        Element OnRender() override
        {
            std::lock_guard lock(_mutex);
            this->_update_visible();
            Elements lines;
            for (size_t i = 0; i < _visible.size(); ++i)
            {
                const size_t idx = _visible[i];
                const Row & row = _rows[idx];
                const char *marker = "  ";
                if (this->_has_children(idx))
                    marker = _folded.contains(row.path) ? "▸ " : "▾ ";

                Elements cells {text(std::string(row.depth * 2, ' ') + marker + row.key)};
                if (auto state = _states.find(row.source); state != _states.end())
                    cells.push_back(text(fmt::format(" [{}]", ServiceController::state_str(state->second)))
                                    | color(state_color(state->second)));
                if (auto value = _values.find(row.source); value != _values.end())
                    cells.push_back(text(" = " + value->second));
                if (auto writes = _writes.find(row.source); writes != _writes.end())
                    cells.push_back(text(fmt::format(" (writes: {})", writes->second)) | dim);

                Element line = hbox(std::move(cells));
                if (i == _selected)
                    line = this->Focused() ? (line | inverted | ftxui::focus) : (line | inverted | ftxui::select);
                lines.push_back(std::move(line));
            }
            return vbox(std::move(lines)) | yframe | vscroll_indicator;
        }

        bool OnEvent(Event event) override
        {
            std::lock_guard lock(_mutex);
            this->_update_visible();
            if (_visible.empty())
                return false;
            const size_t idx = _visible[_selected];
            const Row & row = _rows[idx];
            if (event == Event::ArrowUp)
                _selected = _selected == 0 ? 0 : _selected - 1;
            else if (event == Event::ArrowDown)
                _selected = std::min(_selected + 1, _visible.size() - 1);
            else if (event == Event::ArrowRight)
                _folded.erase(row.path);
            else if (event == Event::ArrowLeft && this->_has_children(idx) && _folded.contains(row.path) == false)
                _folded.insert(row.path);
            else if (event == Event::ArrowLeft && row.depth > 0)
                _selected = this->_visible_parent(idx);
            else
                return false;
            return true;
        }

        bool Focusable() const override { return true; }

    private:
        void _build()
        {
            std::vector<Row> rows;
            std::map<std::string, std::string> values;
            std::map<std::string, ServiceController::State> states;
            std::vector<std::string> parents;
            _monitor.walk(
                [&](Named *named, [[maybe_unused]] const Node *parent, const std::string & key, size_t depth) {
                    parents.resize(depth);
                    const std::string path = depth == 0 ? named->full_name() : parents.back() + Named::separator + key;
                    parents.push_back(path);
                    rows.push_back({path, named->full_name(), key, depth});

                    if (const Channel *channel = dynamic_cast<const Channel *>(named))
                        values[named->full_name()] = channel_str(*channel);

                    AService *service = dynamic_cast<AService *>(named);
                    auto *ctrl = service != nullptr ? dynamic_cast<ServiceController *>(service->service_ctrl())
                                                    : nullptr;
                    if (ctrl != nullptr)
                        states[named->full_name()] = ctrl->state();
                });
            std::lock_guard lock(_mutex);
            std::vector<Row> attached = std::move(_rows);
            _rows = std::move(rows);
            for (const Row & row : attached)
                this->_insert(row.path);
            // values from events are newer than the walk
            _values.merge(values);
            _states.merge(states);
        }

        size_t _insert(const std::string & path)
        {
            auto it = std::find_if(_rows.begin(), _rows.end(), [&path](const Row & row) {
                return row.path == path || row.source == path;
            });
            if (it != _rows.end())
                return it - _rows.begin();

            const size_t sep = path.rfind(Named::separator);
            if (sep == std::string::npos)
            {
                _rows.push_back({path, path, path, 0});
                return _rows.size() - 1;
            }
            const size_t parent = this->_insert(path.substr(0, sep));
            size_t pos = parent + 1;
            while (pos < _rows.size() && _rows[pos].depth > _rows[parent].depth)
                ++pos;
            _rows.insert(_rows.begin() + pos, {path, path, path.substr(sep + 1), _rows[parent].depth + 1});
            return pos;
        }

        void _erase(const std::string & path)
        {
            for (;;)
            {
                auto it = std::find_if(_rows.begin(), _rows.end(), [&path](const Row & row) {
                    return row.path == path || row.source == path;
                });
                if (it == _rows.end())
                    break;
                auto last = std::find_if(it + 1, _rows.end(), [&it](const Row & row) {
                    return row.depth <= it->depth;
                });
                _rows.erase(it, last);
            }
            _values.erase(path);
            _states.erase(path);
            _writes.erase(path);
        }

        bool _has_children(size_t idx) const
        {
            return idx + 1 < _rows.size() && _rows[idx + 1].depth > _rows[idx].depth;
        }

        void _update_visible()
        {
            _visible.clear();
            size_t folded_depth = 0;
            bool folding = false;
            for (size_t i = 0; i < _rows.size(); ++i)
            {
                if (folding && _rows[i].depth > folded_depth)
                    continue;
                folding = _folded.contains(_rows[i].path);
                folded_depth = _rows[i].depth;
                _visible.push_back(i);
            }
            _selected = std::min(_selected, _visible.empty() ? 0 : _visible.size() - 1);
        }

        size_t _visible_parent(size_t idx) const
        {
            for (size_t i = _selected; i > 0; --i)
            {
                if (_rows[_visible[i - 1]].depth < _rows[idx].depth)
                    return i - 1;
            }
            return 0;
        }

        TreeMonitor & _monitor;
        Refresher & _refresher;
        std::mutex _mutex;
        std::vector<Row> _rows;
        std::map<std::string, std::string> _values;
        std::map<std::string, ServiceController::State> _states;
        std::map<std::string, size_t> _writes;
        std::set<std::string> _folded;
        std::vector<size_t> _visible;
        size_t _selected = 0;
};

} // namespace

Component TreeMonitorComponent(TreeMonitor & monitor, Refresher & refresher)
{
    return Make<TreeView>(monitor, refresher);
}

} // namespace sihd::tui
