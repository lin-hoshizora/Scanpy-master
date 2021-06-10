#include "x50.h"

BOOST_PYTHON_MODULE(scanpy){
	class_<X50>("X50")
		.def("findDev", &X50::findDev)
		.def("openDev", &X50::openDev)
        .def("init", &X50::init)
        .def("ledOn", &X50::ledOn)
        .def("ledOff", &X50::ledOff)
		.def("captureSetup", &X50::captureSetup)
		.def("detectPaperL", &X50::detectPaperL)
		.def("detectPaperR", &X50::detectPaperR)
		.def("autoDetect", &X50::autoDetect)
        .def("forceStart", &X50::forceStart)
        .def("retrieveBuff", &X50::retrieveBuff)
		.def("clean", &X50::clean);
}