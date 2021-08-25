#include <sihd/py/PyUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>

namespace sihd::py
{

NEW_LOGGER("sihd::py");

using namespace sihd::util;

PYBIND11_MODULE(sihd_util, m)
{
    m.doc() = "hello";

    pybind11::class_<Named, SmartNodePtr<Named> > named(m, "Named");
    named.def(pybind11::init<const std::string &, Node *>());
    named.def("get_name", &Named::get_name);
    named.def("get_parent", &Named::get_parent);

    pybind11::class_<Node, SmartNodePtr<Node> > node(m, "Node", named);
    node.def(pybind11::init<const std::string &, Node *>());
    node.def("is_link", &Node::is_link);

}

}
