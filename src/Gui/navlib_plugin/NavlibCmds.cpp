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

NavlibInterface::FreecadCmd::FreecadCmd(QAction *action, Gui::Command *cmd, int param): 
										pAction(action), pCommand(cmd), parameter(param){
	type= Type::NONE;
	if (cmd->getAction() != nullptr){
		if(action->isCheckable()){
			type= Type::CHECKABLE;
		} 
		if(auto pcAction = qobject_cast<Gui::ActionGroup*>(pCommand->getAction())){
			if(pcAction->actions().size()==1){
				//Group can be checkable too
				type= Type::CHECKABLE;
			}else{
				type= Type::GROUP;
			}
		}
	}
}

void NavlibInterface::FreecadCmd::run(){
	switch(type){
		case FreecadCmd::Type::NONE:
			Gui::Application::Instance->commandManager().runCommandByName(pCommand->getName());
			break;
		case FreecadCmd::Type::CHECKABLE:
			pAction->toggle();
			break;
		case FreecadCmd::Type::GROUP:
			pCommand->invoke(parameter);
			break;
	}
}

std::string NavlibInterface::FreecadCmd::id()const{
	if(type == FreecadCmd::Type::GROUP){
		return std::string(pCommand->getName())+"_"+pAction->text().toStdString();
	}else{
		return pCommand->getName();
	}
}

std::string NavlibInterface::FreecadCmd::name()const{
	if(type == FreecadCmd::Type::GROUP){
		return pAction->text().toStdString();
	}else{
		std::string str(pCommand->getMenuText());
		str.erase(std::remove(str.begin(), str.end(), '&'), str.end());
		return str;
	}
}

std::string NavlibInterface::FreecadCmd::description()const{
	if(type == FreecadCmd::Type::GROUP){
		return pAction->toolTip().toStdString();
	}else{
		return pCommand->getToolTipText();
	}
}

TDx::CImage NavlibInterface::FreecadCmd::extractImage()const{
	QIcon iconImg = pAction->icon();
	QImage qimage = iconImg.pixmap(QSize(256, 256)).toImage();
	QByteArray qbyteArray;
	QBuffer qbuffer(&qbyteArray);
	qimage.save(&qbuffer, "PNG"); // writes the image in PNG format inside the buffer
	QString iconBase64 = QString::fromLatin1(qbyteArray.toBase64().data());
	return TDx::CImage::FromData(qbyteArray.toStdString(), 0, id().c_str());
}

TDx::SpaceMouse::CCommand NavlibInterface::FreecadCmd::toCCommand()const{
	return CCommand(id(), name(), description());
}

long NavlibInterface::SetActiveCommand(std::string commandId)
{
	if(commandId != ""){
		commands[commandId]->run();
	}
	return 0;
}

void NavlibInterface::extractCommand(Gui::Command &command, TDx::SpaceMouse::CCategory &category, std::vector<TDx::CImage> &images){
	if(auto actionsGroup = qobject_cast<Gui::ActionGroup*>(command.getAction())){
		if(actionsGroup->actions().size() > 1){
			return extractCommands(*actionsGroup, command, category, images);
		}
	}
	if(command.getAction() == nullptr){
		return;
	}
		
	auto commandId = command.getName();

	if(commands.find(commandId)== commands.end()){
		commands[commandId]=std::make_shared<FreecadCmd>(command.getAction()->action(), &command);
	}
	auto localCommand=commands[commandId];

	if (localCommand->toCCommand().Label == "")
        return;

	category.push_back(localCommand->toCCommand());
#ifdef WIN32	
	if(!command.getAction()->icon().isNull()){
		images.push_back(localCommand->extractImage());
	}
#endif
}

void NavlibInterface::extractCommands(Gui::ActionGroup &actionsGroup, Gui::Command &command, TDx::SpaceMouse::CCategory &category, std::vector<TDx::CImage> &images){
	int index = 0;
	for(auto action : actionsGroup.actions()){
		if(action->isSeparator()){
			continue;
		}
		auto commandId= std::string(command.getName())+"_"+action->text().toStdString();
		auto localCommand = commands[commandId]=std::make_shared<FreecadCmd>(action, &command, index++);

		if (localCommand->toCCommand().Label == "")
            return;

		category.push_back(localCommand->toCCommand());
#ifdef WIN32			
		if(!action->icon().isNull()){
			images.push_back(localCommand->extractImage());
		}
#endif
	}
}

void NavlibInterface::exportCommands(const std::string &workbench)
{
    static const std::string substring("Workbench");
	static const std::string noneWorkbench("NoneWorkbench");

    if (workbench == noneWorkbench)
        return;

    std::string shortName(workbench);
    auto index = shortName.find(substring);

    if (index != std::string::npos) {
        shortName.erase(index, substring.size());
    }

    CCommandSet commandsSet(workbench, shortName);

    static const auto basicCommands = Gui::Application::Instance->commandManager().getAllCommands();
    auto workbenchCommands = Gui::Application::Instance->commandManager().getGroupCommands(shortName.c_str());
    auto moduleCommands = Gui::Application::Instance->commandManager().getModuleCommands(shortName.c_str());

    workbenchCommands.insert(workbenchCommands.end(), basicCommands.begin(), basicCommands.end());
    workbenchCommands.insert(workbenchCommands.end(), moduleCommands.begin(), moduleCommands.end());

    std::vector<TDx::CImage> images;

    for (auto &command : workbenchCommands) {

		std::string groupName(command->getGroupName());
		
        if ((groupName == "<string>") || (groupName.empty()))
            groupName = "Others";

        CCategory category(groupName, groupName);
        extractCommand(*command, category, images);
        commandsSet.push_back(std::move(category));
    }

    nav3d::AddCommandSet(commandsSet);
    nav3d::PutActiveCommands(workbench);
#ifdef WIN32
    nav3d::AddImages(images);
#endif
}