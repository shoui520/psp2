/*
 * The draw objects' factories (DEBUGGER 0x810d8a7c..0x810d8b10): the plane and text objects and
 * their parameters made for Plugin by their parameters' ParamName(), which the Framework
 * registers. No record or string names the file or its functions: those names are ours, the
 * tables' are named as Sony named the widgets' exported ones.
 */
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <widget/factory.h>

using namespace paf;
using namespace paf::graph;

static DrawObj *NewPlaneObj()
{
	return new PlaneObj(PlaneObjStyle());
}

static VersatileParam *NewPlaneObjParam()
{
	return new PlaneObjParam;
}

static DrawObj *NewTextObj()
{
	return new TextObj(TextObjStyle());
}

static VersatileParam *NewTextObjParam()
{
	return new TextObjParam;
}

ui::FactoryFuncDrawObj g_factoryPlaneObj = {
	PlaneObjParam::ParamName(), NewPlaneObj, NewPlaneObjParam
};

ui::FactoryFuncDrawObj g_factoryTextObj = {
	TextObjParam::ParamName(), NewTextObj, NewTextObjParam
};
