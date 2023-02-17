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
    long IsUserPivot(navlib::bool_t&) const override;
    void ExportCommands();

	long GetCameraMatrix(navlib::matrix_t&) const override;
	long GetPointerPosition(navlib::point_t&) const override;
	long GetViewExtents(navlib::box_t&) const override;
	long GetViewFOV(double&) const override;
	long GetViewFrustum(navlib::frustum_t&) const override;
	long GetIsViewPerspective(navlib::bool_t&) const override;
	long GetModelExtents(navlib::box_t&) const override;
	long GetSelectionExtents(navlib::box_t&) const override;
	long GetSelectionTransform(navlib::matrix_t&) const override;
	long GetIsSelectionEmpty(navlib::bool_t&) const override;
	long GetPivotPosition(navlib::point_t&) const override;
	long GetPivotVisible(navlib::bool_t&) const override;	
	long GetHitLookAt(navlib::point_t&) const override;
    long GetFrontView(navlib::matrix_t&) const override;
    long GetCoordinateSystem(navlib::matrix_t&) const override;
    long GetIsViewRotatable(navlib::bool_t&) const override;

	long SetCameraMatrix(const navlib::matrix_t&) override;
    long SetViewExtents(const navlib::box_t&) override;
    long SetViewFOV(double) override;
    long SetViewFrustum(const navlib::frustum_t&) override;
	long SetSelectionTransform(const navlib::matrix_t&) override;
	long SetPivotPosition(const navlib::point_t&) override;
	long SetPivotVisible(bool) override;
	long SetHitAperture(double) override;
	long SetHitDirection(const navlib::vector_t&) override;
	long SetHitLookFrom(const navlib::point_t&) override;
	long SetHitSelectionOnly(bool) override;
	long SetActiveCommand(std::string) override;
	long SetTransaction(long) override;

	struct Pivot
    {
        Pivot();
        ~Pivot();
        void init(SoGroup*);
        bool isInitialized();
        void updatePivotResolution();
        const std::string imagePath;
        // open inventor part
        SoTransform *pTransform;
        SoSwitch *pVisibility;
        SoResetTransform *pResetView;
        SoImage *pImage;
        QMetaObject::Connection connection;    
    };

private:

	template<class cameraOut = SoCamera*> cameraOut getCamera() const;
    Gui::View3DInventorViewer *getViewer() const;
    void onViewChanged(const Gui::MDIView*);
    bool is3DView() const;
    bool is2DView() const;
    NavlibInterface::Pivot *getCurrentPivot() const;
    void onWorkbenchChanged(std::string const&);
    //cmds
    void extractCommand(Gui::Command&, TDx::SpaceMouse::CCategory&, std::vector<TDx::CImage>&);
    void extractCommands(Gui::ActionGroup&, Gui::Command&, TDx::SpaceMouse::CCategory&,
                         std::vector<TDx::CImage>&);

	struct FreecadCmd 
	{
		FreecadCmd() = default;
		FreecadCmd(QAction*, Gui::Command*, int param = -1);
        void run();
        std::string name() const;
        std::string id() const;
        std::string description() const;
        TDx::CImage extractImage() const;
        TDx::SpaceMouse::CCommand extractCmd() const;
		QAction *pAction;
		enum class Type 
		{
			NONE,
			CHECKABLE,
			GROUP
		} type;
		Gui::Command *pCommand;
		int parameter;
	};

	struct Navigation2D 
	{
		void init(QGraphicsView *v);
        void updateGraphics(QGraphicsView*) const;
		QRectF modelExtents;
		QMatrix4x4 data;
		double scale;
	} navigation2d;

	struct
    {
        const Gui::View3DInventor *pView3d = nullptr;
        QGraphicsView *pView2d = nullptr;
    } currentView;

	struct
    {
        SbVec3f origin;
        SbVec3f direction;
        float radius;
        bool selectionOnly;
    } ray;

	std::unordered_map<QGraphicsView*, Navigation2D> data2dMap;
    std::unordered_map<const Gui::Document*, std::shared_ptr<NavlibInterface::Pivot>> doc2Pivot;
    std::pair<int, std::string> activeTab = {-1, ""};
    std::unordered_map<std::string, std::shared_ptr<FreecadCmd>> commands;
};
#endif // NAVLIB_PLUGIN_NAVLIB_INTERFACE_H
