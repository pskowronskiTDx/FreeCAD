#ifndef NAVLIB_PLUGIN_NAVLIB_INTERFACE_H
#define NAVLIB_PLUGIN_NAVLIB_INTERFACE_H

#include <SpaceMouse/CNavigation3D.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <Inventor/nodes/SoResetTransform.h>

#include <QMatrix4x4>

using nav3d = TDx::SpaceMouse::Navigation3D::CNavigation3D;

class QGraphicsView;
class QAction;

namespace Gui
{
	class MDIView;
	class View3DInventor;
	class View3DInventorViewer;
	class Document;
	class Command;
	class ActionGroup;
}

class NavlibInterface : public nav3d 
{

public:
	NavlibInterface();
	~NavlibInterface();
	void EnableNavigation();

	long GetCameraMatrix(navlib::matrix_t&) const override;
	long GetPointerPosition(navlib::point_t&) const override;
	long GetViewExtents(navlib::box_t&) const override;
	long GetViewFOV(double&) const override;
	long GetViewFrustum(navlib::frustum_t&) const override;
	long GetIsViewPerspective(navlib::bool_t&) const override;
	long SetCameraMatrix(const navlib::matrix_t&) override;
	long SetViewExtents(const navlib::box_t&) override;
	long SetViewFOV(double) override;
	long SetViewFrustum(const navlib::frustum_t&) override;
	long GetModelExtents(navlib::box_t&) const override;
	long GetSelectionExtents(navlib::box_t&) const override;
	long GetSelectionTransform(navlib::matrix_t&) const override;
	long GetIsSelectionEmpty(navlib::bool_t&) const override;
	long SetSelectionTransform(const navlib::matrix_t&) override;
	long GetPivotPosition(navlib::point_t&) const override;
	long IsUserPivot(navlib::bool_t&) const override;
	long SetPivotPosition(const navlib::point_t&) override;
	long GetPivotVisible(navlib::bool_t&) const override;
	long SetPivotVisible(bool) override;
	long GetHitLookAt(navlib::point_t&) const override;
	long SetHitAperture(double) override;
	long SetHitDirection(const navlib::vector_t&) override;
	long SetHitLookFrom(const navlib::point_t&) override;
	long SetHitSelectionOnly(bool) override;
	long SetActiveCommand(std::string) override;
	long SetTransaction(long) override;

	long GetFrontView(navlib::matrix_t&) const override;
	long GetCoordinateSystem(navlib::matrix_t&) const override;
	long GetIsViewRotatable(navlib::bool_t&)const override;

	void ExportCommands();

	struct Pivot 
	{
		const std::string rpath_;
		// open inventor part
		SoTransform* transf_;
		SoSwitch* visibility_;
		SoResetTransform* resetView_;
		SoImage* img_;
		QMetaObject::Connection conn_;
		Pivot();
		~Pivot();
		void Init(SoGroup*);
		bool IsInitialized();
		void UpdatePivotResolution();
	};

private:
	struct FreecadCmd 
	{
		FreecadCmd() = default;
		FreecadCmd(QAction*, Gui::Command*, int param = -1);
		QAction* action_;
		enum class Type 
		{
			NONE,
			CHECKABLE,
			GROUP
		} type_;
		Gui::Command* cmd_;
		int param_;
		void Run();
		std::string Name()const;
		std::string Id()const;
		std::string Description()const;
		TDx::CImage ExtractImage()const;
		TDx::SpaceMouse::CCommand ExtractCmd() const;
	};

	struct Nav2d 
	{
		void Init(QGraphicsView* v);
		QRectF modelExtents_;
		QMatrix4x4 data_;
		double scale_;
		void UpdateGraphics(QGraphicsView*)const;
	} nav2d_;
	std::unordered_map<QGraphicsView*, Nav2d> data2d_;

	template<class cameraOut = SoCamera*>
	cameraOut GetCamera() const;
	Gui::View3DInventorViewer* GetViewer() const;
	void OnViewChanged(const Gui::MDIView*);
	bool Is3DView() const;
	bool Is2DView() const;
	struct
	{
		const Gui::View3DInventor* _3D = nullptr;
		QGraphicsView* _2D = nullptr;
	} currentView_;

	std::unordered_map<const Gui::Document*, std::shared_ptr<NavlibInterface::Pivot>> doc2Pivot_;
	NavlibInterface::Pivot* GetCurrentPivot()const;
	void OnWorkbenchChanged(std::string const&);

	struct
	{
		SbVec3f origin_;
		SbVec3f direction_;
		float radius_;
		bool selectionOnly_;
	} ray_;

	std::pair<int, std::string> activeTab_ = { -1,"" };

	//cmds
	std::unordered_map<std::string, std::shared_ptr<FreecadCmd>> cmds_;
	void ExtractCommand(Gui::Command&, TDx::SpaceMouse::CCategory&, std::vector<TDx::CImage>&);
	void ExtractCommands(Gui::ActionGroup&, Gui::Command&, TDx::SpaceMouse::CCategory&, std::vector<TDx::CImage>&);
};
#endif // NAVLIB_PLUGIN_NAVLIB_INTERFACE_H
