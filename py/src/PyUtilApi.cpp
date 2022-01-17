#include <sihd/py/PyUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::py
{

NEW_LOGGER("sihd::py");

using namespace sihd::util;

PYBIND11_MODULE(sihd_util, m)
{
    m.doc() = "hello";

    pybind11::class_<Named, SmartNodePtr<Named> > named(m, "Named");
    // keep_alive 1 -> this | 2 -> first arg | 3 -> parent
    // keeps alive parent while named object is alive
    named.def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>());
    named.def(pybind11::init<const std::string &>());
    named.def("get_name", &Named::get_name);
    named.def("get_parent", &Named::get_parent);

    pybind11::class_<Node, SmartNodePtr<Node> > node(m, "Node", named);
    // keeps alive parent while node object is alive
    node.def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>());
    node.def(pybind11::init<const std::string &>());
    node.def("is_link", &Node::is_link);

}

}
