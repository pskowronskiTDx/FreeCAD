#include <PreCompiled.h>
#include "NavlibInterface.h"

#include <Inventor/SoRenderManager.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbViewVolume.h>

#include <Gui/Application.h>

#include <Base/BoundBox.h>
#include "Inventor/actions/SoGetBoundingBoxAction.h"
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>

#include <Gui/MDIView.h>
#include <Gui/View3DInventor.h>
#include <Gui/View3DInventorViewer.h>
#include <Gui/NavigationStyle.h>
#include <Gui/Document.h>
#include <Gui/MainWindow.h>
#include <Gui/WorkbenchManager.h>
#include <Gui/Workbench.h>

#include <Gui/ViewProvider.h>

#include <SoFCBoundingBox.h>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTimer>
#include <QGraphicsSvgItem>

double ComputeScale(QGraphicsView* v)
{
	const auto t = v->transform();
	return std::sqrt(t.m11() * t.m11() + t.m12() * t.m12());
}

template<class cameraOut>
cameraOut NavlibInterface::GetCamera() const
{
	if (Is3DView()) 
	{
		if (currentView_._3D->getViewer() != nullptr) 
		{
			if (auto cam = dynamic_cast<cameraOut>(currentView_._3D->getViewer()->getCamera())) 
			{
				return cam;
			}
		}
	}
	return nullptr;
}

Gui::View3DInventorViewer* NavlibInterface::GetViewer() const
{
	if (Is3DView()) 
	{
		return currentView_._3D->getViewer();
	}
	return nullptr;
}

void NavlibInterface::OnViewChanged(const Gui::MDIView* view) 
{
	nav3d::Write(navlib::motion_k, false);
	QTimer::singleShot(1000, [this]
	{ nav3d::Write(navlib::motion_k, true); });
	if (view != nullptr) 
	{
		if (auto view3d = dynamic_cast<const Gui::View3DInventor*>(view)) 
		{
			currentView_._3D = view3d;
			currentView_._2D = nullptr;
			navlib::matrix_t coord, front;
			GetCoordinateSystem(coord);
			GetFrontView(front);
			Write(navlib::coordinate_system_k, coord);
			Write(navlib::views_front_k, front);
			return;
		}
	}

	currentView_._3D = nullptr;
	for (auto viewinternal : view->findChildren<QGraphicsView*>()) 
	{
		for (auto svgitem : viewinternal->findChildren<QGraphicsScene*>()) 
		{
			for (auto item : svgitem->items()) 
			{
				if (auto it = dynamic_cast<QGraphicsSvgItem*>(item)) 
				{
					if (it->isActive() && it->isEnabled() && it->isVisible())
					{
						currentView_._2D = svgitem->views().first();
						if (data2d_.find(currentView_._2D) == data2d_.end())
						{
							auto& elem = data2d_[currentView_._2D] = Nav2d();
							elem.Init(currentView_._2D);
							elem.UpdateGraphics(currentView_._2D);
						}
						navlib::box_t me;
						GetModelExtents(me);
						Write(navlib::model_extents_k, me);
						return;
					}
				}
			}
		}
	}
}

NavlibInterface::NavlibInterface() 
	: CNavigation3D(false, false)
{ }

void NavlibInterface::EnableNavigation()
{
	PutProfileHint("Freecad");
	nav3d::EnableNavigation(true);
	PutFrameTimingSource(TimingSource::SpaceMouse);
	Write(navlib::active_k, true);
	// maintains map of pivots.
	Gui::Application::Instance->signalNewDocument.connect([this](const Gui::Document& doc, bool) {
		doc2Pivot_[&doc] = std::make_shared<NavlibInterface::Pivot>();
	});
	Gui::Application::Instance->signalDeleteDocument.connect([this](const Gui::Document& doc) {
		doc2Pivot_.erase(&doc);
	});
	if (auto area = Gui::MainWindow::getInstance()->findChild<QMdiArea*>()) {
		if (auto tabs = area->findChild<QTabBar*>()) {
			tabs->connect(tabs, &QTabBar::currentChanged, [this, tabs](int idx) {
				activeTab_ = { idx, idx >= 0 ? tabs->tabText(idx).toStdString() : "" };
			});
		}
	}
}

NavlibInterface::~NavlibInterface()
{
	nav3d::EnableNavigation(false);
}

long NavlibInterface::GetCameraMatrix(navlib::matrix_t& matrix) const
{
	if (activeTab_.first == -1 || activeTab_.second == "Start page")
	{
		return navlib::make_result_code(navlib::navlib_errc::no_data_available);
	}
	else if (Is3DView())
	{
		if (auto cam = GetCamera()) {
			SbMatrix mat;
			cam->orientation.getValue().getValue(mat);
			for (int i = 0; i < 4; i++) {
				std::copy(mat[i], mat[i] + 4, matrix.m4x4[i]);
			}
			const auto& position = cam->position;
			std::copy(position.getValue().getValue(), position.getValue().getValue() + 3, matrix.m4x4[3]);
			return 0;
		}
	}
	else if (Is2DView())
	{
		std::copy(data2d_.at(currentView_._2D).data_.data(), data2d_.at(currentView_._2D).data_.data() + 16, matrix.m);
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::function_not_supported);
}

long NavlibInterface::SetCameraMatrix(const navlib::matrix_t &matrix)
{
	if (Is2DView())
	{
		std::copy(matrix.m,matrix.m+16, data2d_.at(currentView_._2D).data_.data());
		return 0;
	}
	else if (auto cam = GetCamera())
	{
		cam->orientation = SbRotation(SbMatrix(matrix.m4x4));
		cam->position.setValue(matrix.m4x4[3][0], matrix.m4x4[3][1], matrix.m4x4[3][2]);
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetViewFrustum(navlib::frustum_t &frustum) const
{
	if (auto cam = GetCamera<SoPerspectiveCamera *>()){
		auto vv = cam->getViewVolume(cam->aspectRatio.getValue());
		frustum = {-vv.getWidth()/2.0, vv.getWidth()/2.0, -vv.getHeight()/2.0, vv.getHeight()/2.0, vv.getNearDist(), 10.0 * (vv.getNearDist()+ vv.nearToFar) };
		if (auto cam = GetCamera())
		{
			cam->focalDistance.setValue((2.0 * vv.getNearDist() + vv.nearToFar) / 2.0);
			cam->touch();
		}
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetViewFrustum(const navlib::frustum_t& frustum)
{
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}


long NavlibInterface::GetViewExtents(navlib::box_t& extents) const
{
	if (auto cam = GetCamera<SoOrthographicCamera*>())
	{
		auto vVol = cam->getViewVolume(cam->aspectRatio.getValue());
		float height = vVol.getHeight();
		float width = vVol.getWidth();
		auto farVal = vVol.nearToFar + vVol.nearDist;
		extents = { -width / 2.0, -height / 2.0, -farVal, width / 2.0, height / 2.0, farVal };
		return 0;
	}
	else if (Is2DView())
	{
		auto r = currentView_._2D->mapToScene(currentView_._2D->viewport()->geometry()).boundingRect();
		auto scaling = ComputeScale(currentView_._2D) / (data2d_.at(currentView_._2D).scale_);
		auto oldCenter = r.center();
		r.setWidth(r.width() * scaling);
		r.setHeight(r.height() * scaling);
		r.moveCenter(oldCenter);

		extents.min_x = r.topLeft().x();
		extents.min_y = r.topLeft().y();
		extents.max_x = r.bottomRight().x();
		extents.max_y = r.bottomRight().y();
		extents.min_z = -1;
		extents.max_z = 0;
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetViewExtents(const navlib::box_t &extents)
{
	if(Is2DView())
	{
		auto r = currentView_._2D->mapToScene(currentView_._2D->viewport()->geometry()).boundingRect();
		data2d_.at(currentView_._2D).scale_ = ComputeScale(currentView_._2D)*r.height()/(extents.max_y-extents.min_y);
		return 0;
	}
	else
	{
		navlib::box_t e;
		GetViewExtents(e);
		if (auto cam = GetCamera())
		{
			cam->scaleHeight((extents.max_x - extents.min_x) / (e.max_x - e.min_x));
			return 0;
		}
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetViewFOV(double &fov) const
{
	if (auto cam = GetCamera<SoPerspectiveCamera *>())
	{
		fov = cam->heightAngle.getValue();
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetViewFOV(double fov)
{
	if (auto cam = GetCamera<SoPerspectiveCamera *>())
	{
		cam->heightAngle.setValue(fov);
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetIsViewPerspective(navlib::bool_t &perspective) const
{
	if (auto cam = GetCamera<SoPerspectiveCamera *>())
	{
		perspective = true;
		return 0;
	}
	else if (auto cam = GetCamera<SoOrthographicCamera *>())
	{
		perspective = false;
		return 0;
	}
	else if (Is2DView())
	{
		perspective = false;
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetModelExtents(navlib::box_t &extents) const
{
	if (auto viewer = GetViewer())
	{
		SoGetBoundingBoxAction action(viewer->getSoRenderManager()->getViewportRegion());
		action.apply(viewer->getSceneGraph());
		SbBox3f nbbox = action.getBoundingBox();
		std::copy(nbbox.getMin().getValue(), nbbox.getMin().getValue() + 3, extents.min.coordinates);
		std::copy(nbbox.getMax().getValue(), nbbox.getMax().getValue() + 3, extents.max.coordinates);
		return 0;
	} 
	else if (Is2DView())
	{
		const QRectF modelExtents = data2d_.at(currentView_._2D).modelExtents_;

		extents.min_x = modelExtents.topLeft().x();
		extents.min_y = modelExtents.topLeft().y();
		extents.max_x = modelExtents.bottomRight().x();
		extents.max_y = modelExtents.bottomRight().y();
		extents.max_z = 0;
		extents.min_z = -1;
		return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetTransaction(long value)
{
	if(Is2DView() && value==0)
	{
		data2d_.at(currentView_._2D).UpdateGraphics(currentView_._2D);
		return 0;
	}
	
	if (value == 0)
	{
		if(auto viewer = GetViewer())
		{
			if (auto cam = GetCamera<SoPerspectiveCamera*>())
			{
 				auto vv = cam->getViewVolume(cam->aspectRatio.getValue());
				cam->focalDistance.setValue((2.0 * vv.getNearDist() + vv.nearToFar) / 2.0);
				cam->touch();
			}
			viewer->getSceneGraph()->touch();
		}
	}
	return 0;
}

long NavlibInterface::GetFrontView(navlib::matrix_t &matrix) const
{
	matrix = {  1., 0., 0., 0.,
				0., 0., 1., 0.,
				0., -1., 0., 0.,
				0., 0., 0., 1. };

	return 0;
}

long NavlibInterface::GetCoordinateSystem(navlib::matrix_t &matrix) const
{
	matrix = {  1., 0., 0., 0.,
				0., 0., -1., 0.,
				0., 1., 0., 0.,
				0., 0., 0., 1. };
	return 0;
}

long NavlibInterface::GetIsViewRotatable(navlib::bool_t& isRotatable) const
{
	isRotatable = Is3DView();
	return 0;
}

bool NavlibInterface::Is3DView() const
{
	return currentView_._3D != nullptr;
}

bool NavlibInterface::Is2DView() const
{
	return currentView_._2D != nullptr;
}

void NavlibInterface::Nav2d::Init(QGraphicsView* view)
{
	modelExtents_ = (view->scene()->itemsBoundingRect());
	scale_ = 1.0;
	QMatrix4x4 init;
	init.translate(modelExtents_.center().x(), modelExtents_.center().y(), 150);
	data_ = init;
}

void NavlibInterface::Nav2d::UpdateGraphics(QGraphicsView* view) const
{
	QPointF size(modelExtents_.width(), modelExtents_.height());
	auto camToWorld = data_;
	camToWorld(0, 3) -= size.x() / 2.0;
	camToWorld(1, 3) -= size.y() / 2.0;

	auto worldToCam = camToWorld.inverted();
	QPointF newPos(worldToCam(0, 3) * scale_, -worldToCam(1, 3) * scale_);
	for (auto item : view->items())
	{
		item->resetTransform();
		item->setTransformOriginPoint(item->boundingRect().center());
		item->setScale(scale_);
		item->setPos(newPos);
	}
}