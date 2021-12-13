#ifndef __SIHD_UTIL_LINEREADER_HPP__
# define __SIHD_UTIL_LINEREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/IReader.hpp>
# include <sihd/util/File.hpp>
# include <queue>

namespace sihd::util
{

class LineReader: public sihd::util::Named, public sihd::util::IReader
{
    public:
        LineReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~LineReader();

        bool set_read_buffsize(size_t buff);

        bool open(const std::string & path);
        bool is_open() const;
        bool close();

        bool read_next();
        bool get_read_data(char **data, size_t *size) const;

        size_t buffsize() const { return _read_buff_size; }
        const sihd::util::File & file() const { return _file; }

    protected:

    private:
        bool _find_in_last_read();
        bool _find_in_read();
        ssize_t _add_new_read();
        ssize_t _search_line_feed(const char *str);

        void _build_line();
        void _concat_read_queue();
        void _process_last_read_queue();

        void _clear_read_data();
        void _clear_line();
        void _reset();

        sihd::util::File _file;
        size_t _read_buff_size;

        struct LastRead
        {
            ssize_t remaining;
            ssize_t total;
            size_t line_feed_pos;
            size_t last_index;
        };
        LastRead _last_read;

        std::queue<char *> _read_queue;
        char *_line_ptr;
        ssize_t _total_line_size;

};

}

#endif