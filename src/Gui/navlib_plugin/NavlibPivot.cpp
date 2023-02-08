
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

// initiliazed pivot wether it is possible. And return true if this initialization has been done. False otherwise.
bool InitPivot(NavlibInterface::Pivot* pivot)
{
	Gui::Document* doc = Gui::Application::Instance->activeDocument();
	if (doc) 
	{
		if (auto view = dynamic_cast<Gui::View3DInventor*>(doc->getActiveView())) 
		{
			if (auto viewer = view->getViewer()) 
			{
				if (auto grp = dynamic_cast<SoGroup*>(viewer->getSceneGraph())) 
				{
					if (!pivot->IsInitialized())
					{
						pivot->Init(grp);
					}
					return true;
				}
			}
		}
	}
	return false;
}

long NavlibInterface::GetPointerPosition(navlib::point_t &position) const
{
	if (Is2DView())
	{
		QPoint viewPoint = currentView_._2D->mapFromGlobal(QCursor::pos());
		auto const& data2d = data2d_.at(currentView_._2D);
		QPointF size(data2d.modelExtents_.width(), (data2d.modelExtents_.height()));
		auto sceneVP = (currentView_._2D->mapToScene(viewPoint) - size / 2.0) / data2d.scale_;
		QVector4D pos = data2d.data_ * QVector4D(sceneVP.x(), -sceneVP.y(), 1, 1);
		position = { {pos.x(), pos.y(), 0} };
		return 0;
	}
	if (auto viewer = GetViewer())
	{
		QPoint viewPoint =  currentView_._3D->mapFromGlobal(QCursor::pos());
		viewPoint.setY(currentView_._3D->height() - viewPoint.y());
		auto pos = viewer->getPointOnFocalPlane(SbVec2s(viewPoint.x(), viewPoint.y()));
		std::copy(pos.getValue(), pos.getValue() + 3, position.coordinates);
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
	if (Is3DView())
	{
		if (auto viewer = GetViewer()){
			if (auto pivot = GetCurrentPivot()) 
			{
				auto trans = pivot->transf_->translation.getValue();
				std::copy(trans.getValue(), trans.getValue() + 3, position.coordinates);
				return 0;
			}
		}
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetPivotPosition(const navlib::point_t &position)
{	
	if(Is3DView()){	
		if(auto pivot = GetCurrentPivot()){
			pivot->transf_->translation.setValue(position.x, position.y, position.z);
			return 0;
		}
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::IsUserPivot(navlib::bool_t &userPivot) const
{
	userPivot = false;
	return 0;
}

long NavlibInterface::GetPivotVisible(navlib::bool_t &visible) const
{	
	if(auto pivot = GetCurrentPivot()){
			visible = pivot->visibility_->whichChild.getValue() == SO_SWITCH_ALL;
			return 0;
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::SetPivotVisible(bool visible)
{
	auto pivot = GetCurrentPivot();
	if (pivot == nullptr)
	{
		return navlib::make_result_code(navlib::navlib_errc::no_data_available);
	}
	
	if (visible)
	{
		pivot->visibility_->whichChild = SO_SWITCH_ALL;
	}
	else
	{
		pivot->visibility_->whichChild = SO_SWITCH_NONE;
	}
	return 0;
}

long NavlibInterface::GetHitLookAt(navlib::point_t &position) const
{
	if (auto viewer = GetViewer())
	{
		if (auto graph = viewer->getSceneGraph())
		{
			SoRayPickAction rpaction(viewer->getSoRenderManager()->getViewportRegion());
			rpaction.setRay(ray_.origin_, ray_.direction_);
			rpaction.apply(graph);
			SoPickedPoint *picked = rpaction.getPickedPoint();
			if (picked != nullptr)
			{
				auto hitpoint = picked->getPoint();
				std::copy(hitpoint.getValue(), hitpoint.getValue() + 3, position.coordinates);
				return 0;
			}
		}
	}
	return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long NavlibInterface::GetSelectionExtents(navlib::box_t &extents) const
{
	Base::BoundBox3d bbox;
	auto v = Gui::Selection().getSelection(/* 0, Gui::ResolveMode::NoResolve */);
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
	ray_.radius_ = aperture;
	return 0;
}

long NavlibInterface::SetHitDirection(const navlib::vector_t &direction)
{
	ray_.direction_.setValue(direction.components);
	return 0;
}

long NavlibInterface::SetHitLookFrom(const navlib::point_t &eye)
{
	ray_.origin_.setValue(eye.coordinates);
	return 0;
}

long NavlibInterface::SetHitSelectionOnly(bool hitSelection)
{
	ray_.selectionOnly_ = hitSelection;
	return 0;
}

NavlibInterface::Pivot* NavlibInterface::GetCurrentPivot() const
{
	if (const Gui::Document* doc = Gui::Application::Instance->activeDocument())
	{
		if (doc2Pivot_.find(doc) != doc2Pivot_.end())
		{
			auto pivot = doc2Pivot_.at(doc).get();
			if (InitPivot(pivot))
			{
				return pivot;
			}
		}
	}
	return nullptr;
}

NavlibInterface::Pivot::Pivot()
	: rpath_(":/icons/3dx_pivot.png")
	, transf_(nullptr)
	, visibility_(nullptr)
	, resetView_(nullptr)
	, img_(nullptr)
{ }

void NavlibInterface::Pivot::Init(SoGroup* grp)
{
	transf_ = new SoTransform;
	visibility_ = new SoSwitch;
	resetView_ = new SoResetTransform;
	img_ = new SoImage;
	SoDepthBuffer * deptb = new SoDepthBuffer;
	deptb->function.setValue(SoDepthBufferElement::ALWAYS);
	UpdatePivotResolution();
	conn_=qApp->focusWindow()->connect(qApp->focusWindow(), &QWindow::screenChanged, [this](QScreen*s){UpdatePivotResolution();});
	visibility_->whichChild = SO_SWITCH_NONE;
	grp->addChild(visibility_);
	visibility_->addChild(deptb);
	visibility_->addChild(resetView_);
	visibility_->addChild(transf_);
	visibility_->addChild(img_);
}

bool NavlibInterface::Pivot::IsInitialized()
{
	return transf_ != nullptr && visibility_ != nullptr && resetView_ != nullptr && img_ != nullptr;
}

void NavlibInterface::Pivot::UpdatePivotResolution(){
	QImage qimage(QString::fromStdString(rpath_));
	qimage= qimage.scaled(qimage.size()*qApp->focusWindow()->devicePixelRatio());
	Gui::BitmapFactory().convert(qimage, img_->image); 
}

NavlibInterface::Pivot::~Pivot(){
  qApp->focusWindow()->disconnect(conn_);
}