#include <PreCompiled.h>

#include<QImage>
#include<QString>
#include<QWindow>
#include<QScreen>

#include "NavlibInterface.h"
#include <Inventor/SoRenderManager.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoResetTransform.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoDepthBuffer.h>

#include <Gui/Application.h>
#include <Gui/Selection.h>
#include <Gui/NavigationStyle.h>
#include <Gui/ViewProvider.h>
#include <Gui/BitmapFactory.h>

#include <Inventor/nodes/SoCamera.h>

#include <Gui/View3DInventor.h>
#include <Gui/View3DInventorViewer.h>


#include <Gui/Document.h>

constexpr float MAX_FLOAT = std::numeric_limits<float>::max();

long NavlibInterface::GetPointerPosition(navlib::point_t &position) const
{
	if (is2DView())
	{
	    QPoint viewPoint = currentView.pView2d->mapFromGlobal(QCursor::pos());
		auto const& data2d = data2dMap.at(currentView.pView2d);
		QPointF size(data2d.modelExtents.width(), (data2d.modelExtents.height()));
		auto sceneVP = (currentView.pView2d->mapToScene(viewPoint) - size / 2.0) / data2d.scale;
		QVector4D pos = data2d.data * QVector4D(sceneVP.x(), -sceneVP.y(), 1, 1);
		position = { pos.x(), pos.y(), 0 };
		return 0;
	}
	if (auto viewer = getViewer())
	{
		QPoint viewPoint =  currentView.pView3d->mapFromGlobal(QCursor::pos());
		viewPoint.setY(currentView.pView3d->height() - viewPoint.y());
		auto pos = viewer->getPointOnFocalPlane(SbVec2s(viewPoint.x(), viewPoint.y()));
		std::copy(pos.getValue(), pos.getValue() + 3, &position.x);
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetSelectionTransform(navlib::matrix_t &) const
{
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetIsSelectionEmpty(navlib::bool_t &empty) const
{
	empty = (Gui::SelectionSingleton::instance().hasSelection()? 0 : 1);
	return 0;
}

long NavlibInterface::SetSelectionTransform(const navlib::matrix_t &)
{
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetPivotPosition(navlib::point_t &position) const
{ 
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetPivotPosition(const navlib::point_t &position)
{
    if (pivot.pVisibility == nullptr)
        return navlib::make_result_code(navlib::navlib_errc::no_data_available);

    pivot.pTransform->translation.setValue(position.x, position.y, position.z);
    return 0;

    return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::IsUserPivot(navlib::bool_t &userPivot) const
{
	userPivot = false;
	return 0;
}

long NavlibInterface::GetPivotVisible(navlib::bool_t &visible) const
{ 
	if (pivot.pVisibility == nullptr)
        return navlib::make_result_code(navlib::navlib_errc::no_data_available);

    visible = pivot.pVisibility->whichChild.getValue() == SO_SWITCH_ALL;
    return 0;
}

long NavlibInterface::SetPivotVisible(bool visible)
{ 
	if (pivot.pVisibility == nullptr)
        return navlib::make_result_code(navlib::navlib_errc::no_data_available);

    if (visible) {
        pivot.pVisibility->whichChild = SO_SWITCH_ALL;
    }
    else {
        pivot.pVisibility->whichChild = SO_SWITCH_NONE;
    }
    return 0;
}

long NavlibInterface::GetHitLookAt(navlib::point_t& position) const
{
    if (auto viewer = getViewer()) {
        if (auto sceneGraph = viewer->getSceneGraph()) {

            SoRayPickAction rayPickAction(viewer->getSoRenderManager()->getViewportRegion());
            SbMatrix cameraMatrix;
            SbVec3f closestHitPoint;
            float minLength = MAX_FLOAT;

            getCamera()->orientation.getValue().getValue(cameraMatrix);

            for (uint32_t i = 0; i < hitTestingResolution; i++) {

                SbVec3f transform(
                    hitTestPattern[i][0] * ray.radius,
					hitTestPattern[i][1] * ray.radius,
					0.0f);

                cameraMatrix.multVecMatrix(transform, transform);

                SbVec3f newOrigin = ray.origin + transform;

                rayPickAction.setRay(newOrigin, ray.direction);
                rayPickAction.apply(sceneGraph);
                SoPickedPoint* pickedPoint = rayPickAction.getPickedPoint();

                if (pickedPoint != nullptr) {
                    SbVec3f hitPoint = pickedPoint->getPoint();
                    float distance = (newOrigin - hitPoint).length();

                    if (distance < minLength) {
                        minLength = distance;
                        closestHitPoint = hitPoint;
                    }
                }
            }
            if (minLength < MAX_FLOAT) {
                std::copy(closestHitPoint.getValue(), closestHitPoint.getValue() + 3, &position.x);
                return 0;
            }
        }
    }
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetSelectionExtents(navlib::box_t &extents) const
{
	Base::BoundBox3d bbox;
	auto v = Gui::Selection().getSelection();
	std::for_each(v.begin(), v.end(), [&bbox](Gui::SelectionSingleton::SelObj& sel){
		auto vp = Gui::Application::Instance->getViewProvider(sel.pObject);
		if (!vp)
		{
			return;
		}
			
		bbox.Add(vp->getBoundingBox(sel.SubName, true));

	});

	extents = {bbox.MinX, bbox.MinY, bbox.MinZ, bbox.MaxX, bbox.MaxY, bbox.MaxZ}; 
	return 0;
}

long NavlibInterface::SetHitAperture(double aperture)
{
	ray.radius = aperture;
	return 0;
}

long NavlibInterface::SetHitDirection(const navlib::vector_t &direction)
{
	ray.direction.setValue(direction.x, direction.y, direction.z);
	return 0;
}

long NavlibInterface::SetHitLookFrom(const navlib::point_t &eye)
{
	ray.origin.setValue(eye.x, eye.y, eye.z);
	return 0;
}

long NavlibInterface::SetHitSelectionOnly(bool hitSelection)
{
    ray.selectionOnly = hitSelection;
    return 0;
}

void NavlibInterface::initializePivot()
{
    pivot.pVisibility = new SoSwitch; 
	pivot.pTransform = new SoTransform;
    pivot.pResetTransform = new SoResetTransform;
    pivot.pImage = new SoImage;
    pivot.pDepthTestAlways = new SoDepthBuffer;
    pivot.pDepthTestLess = new SoDepthBuffer;

    pivot.pDepthTestAlways->function.setValue(SoDepthBufferElement::ALWAYS);
    pivot.pDepthTestLess->function.setValue(SoDepthBufferElement::LESS);

	pivot.pivotImage = QImage(QString::fromStdString(":/icons/3dx_pivot.png"));
    Gui::BitmapFactory().convert(pivot.pivotImage, pivot.pImage->image); 

	pivot.pVisibility->ref();
	pivot.pVisibility->whichChild = SO_SWITCH_NONE;
    pivot.pVisibility->addChild(pivot.pDepthTestAlways);
    pivot.pVisibility->addChild(pivot.pTransform);
    pivot.pVisibility->addChild(pivot.pImage);
    pivot.pVisibility->addChild(pivot.pResetTransform);
    pivot.pVisibility->addChild(pivot.pDepthTestLess);
}
