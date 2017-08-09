/*! @file
  @author Shin'ichiro Nakaoka
*/

#include "GrxUIPlugin.h"
#include "GrxUIMenuView.h"
#include <cnoid/Py3Util>
#include <cnoid/Python3Plugin>
#include <cnoid/MenuManager>
#include <cnoid/AppConfig>
#include <pybind11/eval.h>
#include "gettext.h"

namespace stdph = std::placeholders;
namespace py = pybind11;
using namespace cnoid;

namespace {

bool isActive_ = false;
Action* importGrxUICheck;

}


bool GrxUIPlugin::isActive()
{
    return isActive_;
}


GrxUIPlugin::GrxUIPlugin()
    : Plugin("GrxUI")
{
    require("Python");
}


bool GrxUIPlugin::initialize()
{
    GrxUIMenuView::initializeClass(this);

    isActive_ = true;
    
    MenuManager& mm = menuManager();
    mm.setPath("/Options").setPath("GrxUI");
    importGrxUICheck = mm.addCheckItem(_("Import the GrxUI module into the main namespace"));
    
    const Mapping& conf = *AppConfig::archive()->findMapping("GrxUI");
    if(conf.isValid()){
        bool on = conf.get("importGrxUI", false);
        importGrxUICheck->setChecked(on);
        onImportGrxUICheckToggled(on, false);
    }
    importGrxUICheck->sigToggled().connect(
        std::bind(&GrxUIPlugin::onImportGrxUICheckToggled, this, stdph::_1, true));

    return true;
}


void GrxUIPlugin::onImportGrxUICheckToggled(bool on, bool doWriteConfig)
{
    if(on){
        py::gil_scoped_acquire lock;
        py::object grxuiModule = py::module::import("cnoid.grxui");
        if(!grxuiModule.is_none()){
            py::eval<py::eval_single_statement>("from cnoid.grxui import *", cnoid::pythonMainNamespace());
        }
    }
    if(doWriteConfig){
        Mapping& conf = *AppConfig::archive()->openMapping("GrxUI");
        conf.write("importGrxUI", importGrxUICheck->isChecked());
    }
}


bool GrxUIPlugin::finalize()
{
    isActive_ = false;
    return true;
}


CNOID_IMPLEMENT_PLUGIN_ENTRY(GrxUIPlugin);