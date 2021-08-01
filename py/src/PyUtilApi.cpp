#include <sihd/py/PyUtilApi.hpp>

namespace sihd::py
{

using namespace sihd::util;

PyUtilApi::PyUtilApi()
{
}

PyUtilApi::~PyUtilApi()
{
}

PYBIND11_MODULE(sihd_util, m)
{
    m.doc() = "hello";

    pybind11::class_<Named> named(m, "Named");
    named.def(pybind11::init<const std::string &, Node *>());
    named.def("name", &Named::get_name);

    pybind11::class_<Node> node(m, "Node", named);
    node.def(pybind11::init<const std::string &, Node *>());
    node.def("is_link", &Node::is_link);
}

}
