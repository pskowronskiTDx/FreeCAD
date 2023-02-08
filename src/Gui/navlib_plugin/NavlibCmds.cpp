#include <PreCompiled.h>
#include "NavlibInterface.h"

#include <SpaceMouse/CCategory.hpp>
#include <SpaceMouse/CCommand.hpp>
#include <SpaceMouse/CCommandSet.hpp>
#include <SpaceMouse/CImage.hpp>

#include <Gui/Command.h>
#include <Gui/Action.h>
#include <Gui/Application.h>
#include <Gui/WorkbenchManager.h>
#include <Gui/MainWindow.h>

#include <Gui/Command.h>
#include <Gui/Workbench.h>

#include <QString>
#include <QScrollBar >
#include <Gui/View3DInventor.h>

using TDx::SpaceMouse::CCategory;
using TDx::SpaceMouse::CCommand;
using TDx::SpaceMouse::CCommandSet;

static const std::string wkbchStr("Workbench");
static const std::string startWorkbench("StartWorkbench");
static const std::string suffix2d("_2D");


NavlibInterface::FreecadCmd::FreecadCmd(QAction* action, Gui::Command* cmd, int param): 
										action_(action), cmd_(cmd), param_(param){
	type_= Type::NONE;
	if (cmd->getAction() != nullptr){
		if(action->isCheckable()){
			type_= Type::CHECKABLE;
		} 
		if(Gui::ActionGroup* pcAction = qobject_cast<Gui::ActionGroup*>(cmd_->getAction())){
			if(pcAction->actions().size()==1){
				//Group can be checkable too
				type_= Type::CHECKABLE;
			}else{
				type_= Type::GROUP;
			}
		}
	}
}

void NavlibInterface::FreecadCmd::Run(){
	switch(type_){
		case FreecadCmd::Type::NONE:
			Gui::Application::Instance->commandManager().runCommandByName(cmd_->getName());
			break;
		case FreecadCmd::Type::CHECKABLE:
			action_->toggle();
			break;
		case FreecadCmd::Type::GROUP:
			cmd_->invoke(param_);
			break;
	}
}

std::string NavlibInterface::FreecadCmd::Id()const{
	if(type_ == FreecadCmd::Type::GROUP){
		return std::string(cmd_->getName())+"_"+action_->text().toStdString();
	}else{
		return cmd_->getName();
	}
}

std::string NavlibInterface::FreecadCmd::Name()const{
	if(type_ == FreecadCmd::Type::GROUP){
		return action_->text().toStdString();
	}else{
		std::string str(cmd_->getMenuText());
		str.erase(std::remove(str.begin(), str.end(), '&'), str.end());
		return str;
	}
}

std::string NavlibInterface::FreecadCmd::Description()const{
	if(type_ == FreecadCmd::Type::GROUP){
		return action_->toolTip().toStdString();
	}else{
		return cmd_->getToolTipText();
	}
}

TDx::CImage NavlibInterface::FreecadCmd::ExtractImage()const{
	QIcon iconImg = action_->icon();
	QImage qimage = iconImg.pixmap(QSize(256, 256)).toImage();
	QByteArray qbyteArray;
	QBuffer qbuffer(&qbyteArray);
	qimage.save(&qbuffer, "PNG"); // writes the image in PNG format inside the buffer
	QString iconBase64 = QString::fromLatin1(qbyteArray.toBase64().data());
	return TDx::CImage::FromData(qbyteArray.toStdString(), 0, Id().c_str());
}

TDx::SpaceMouse::CCommand NavlibInterface::FreecadCmd::ExtractCmd()const{
	return CCommand(Id(), Name(), Description());
}

long NavlibInterface::SetActiveCommand(std::string commandId)
{
	if(commandId != ""){
		cmds_[commandId]->Run();
	}
	return 0;
}

void NavlibInterface::ExtractCommand(Gui::Command& cmd, TDx::SpaceMouse::CCategory& cat, std::vector<TDx::CImage>& images){
	if(Gui::ActionGroup* actionGrp = qobject_cast<Gui::ActionGroup*>(cmd.getAction())){
		if(actionGrp->actions().size()>1){
			return ExtractCommands(*actionGrp, cmd, cat, images);
		}
	}
	if(cmd.getAction() == nullptr){
		return;
	}
		
	auto cmdId = cmd.getName();
	if(cmds_.find(cmdId)== cmds_.end()){
		cmds_[cmdId]=std::make_shared<FreecadCmd>(cmd.getAction()->action(), &cmd);
	}
	auto localCmd=cmds_[cmdId];
	cat.push_back(localCmd->ExtractCmd());
	if(!cmd.getAction()->icon().isNull()){
		images.push_back(localCmd->ExtractImage());
	}
}

void NavlibInterface::ExtractCommands(Gui::ActionGroup& actions, Gui::Command& cmd, TDx::SpaceMouse::CCategory& cat, std::vector<TDx::CImage>& images){
	int actionIdx=0;
	for(auto action: actions.actions()){
		if(action->isSeparator()){
			continue;
		}
		auto cmdId= std::string(cmd.getName())+"_"+action->text().toStdString();
		auto  localCmd = cmds_[cmdId]=std::make_shared<FreecadCmd>(action, &cmd, actionIdx++);
		cat.push_back(localCmd->ExtractCmd());
		if(!action->icon().isNull()){
			images.push_back(localCmd->ExtractImage());
		}
	}
}

void NavlibInterface::ExportCommands(){
	std::vector<TDx::CImage> images;
	const auto basicCmds = Gui::Application::Instance->commandManager().getAllCommands();
	auto workbenches = Gui::Application::Instance->workbenches();
	// put start workbench at the begining of the list to load first this command set.
	workbenches[workbenches.indexOf(QString::fromStdString(startWorkbench))] = workbenches[0];
	workbenches[0] = QString::fromStdString(startWorkbench);
	for (const auto &workbench : workbenches)
	{
		if (workbench.toStdString() == "NoneWorkbench")
		{
			continue;
		}
		std::string wrkbenchName(workbench.toStdString());
		auto posStr = wrkbenchName.find(wkbchStr);
		if (posStr != std::string::npos){
			wrkbenchName.erase(posStr, wkbchStr.size());
		}
		CCommandSet cSet(workbench.toStdString(), wrkbenchName);
		Gui::Application::Instance->activateWorkbench(workbench.toStdString().c_str());
		auto workbenchcmds = Gui::Application::Instance->commandManager().getGroupCommands(wrkbenchName.c_str());
		auto extApp = Gui::Application::Instance->commandManager().getModuleCommands(wrkbenchName.c_str());
		workbenchcmds.insert(workbenchcmds.end(), basicCmds.begin(), basicCmds.end());
		workbenchcmds.insert(workbenchcmds.end(), extApp.begin(), extApp.end());
		for (auto &cmd : workbenchcmds)
		{
			CCategory cCat(cmd->getGroupName(), cmd->getGroupName());
			ExtractCommand(*cmd, cCat, images);
			cSet.push_back(std::move(cCat));
		}
		nav3d::AddCommandSet(cSet);
		if (workbench.toStdString() == startWorkbench)
		{
			nav3d::PutActiveCommands(startWorkbench);
			nav3d::AddImages(images);
		}
	}
	Gui::Application::Instance->activateWorkbench(startWorkbench.c_str());
	nav3d::AddImages(images);
	Gui::Application::Instance->signalActivateView.connect(boost::bind(&NavlibInterface::OnViewChanged, this, _1));
	App::GetApplication().signalFinishOpenDocument.connect([this]{
		OnViewChanged(Gui::Application::Instance->activeView());
	});

	Gui::Application::Instance->signalActivateWorkbench.connect([this](const char *wb)
																{ OnWorkbenchChanged(std::string(wb)); });

}

void NavlibInterface::OnWorkbenchChanged(std::string const &workbench)
{
	nav3d::PutActiveCommands(workbench);
}

