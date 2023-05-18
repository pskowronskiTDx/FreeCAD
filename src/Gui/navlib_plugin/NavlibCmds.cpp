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

NavlibInterface::FCCommand::FCCommand(
	QAction* const pAction_,
	Gui::Command* const pCommand_,
	int parameter_)
    : pAction(pAction_),
      pCommand(pCommand_),
      parameter(parameter_)
{
    type = Type::NONE;

    if (pCommand->getAction() != nullptr) {

        if (pAction->isCheckable()) {
            type = Type::CHECKABLE;
        }

        const auto pActionGroup = qobject_cast<Gui::ActionGroup*>(pCommand->getAction());

        if (pActionGroup != nullptr) {

            if (pActionGroup->actions().size() == 1) {
                type = Type::CHECKABLE;
            }
            else {
                type = Type::GROUP;
            }
        }
    }
}

void NavlibInterface::FCCommand::run()
{
    switch (type) {
        case FCCommand::Type::NONE:
            Gui::Application::Instance->commandManager().runCommandByName(pCommand->getName());
            break;
        case FCCommand::Type::CHECKABLE:
            pAction->toggle();
            break;
        case FCCommand::Type::GROUP:
            pCommand->invoke(parameter);
            break;
    }
}

std::string NavlibInterface::FCCommand::id() const
{
    if (pAction->icon().isNull()) {
        return "NULL";
    }

    std::string name = pAction->text().toStdString();

    if (type == FCCommand::Type::GROUP) {
        return std::string(pCommand->getGroupName()) + "_" + name;
    }
    else {
        return name;
    }
}

std::string NavlibInterface::FCCommand::name() const
{
    if (type == FCCommand::Type::GROUP) {
        return pAction->text().toStdString();
    }
    else {
        std::string name(pCommand->getMenuText());
        name.erase(std::remove(name.begin(), name.end(), '&'), name.end());
        return name;
    }
}

std::string NavlibInterface::FCCommand::description() const
{
    if (type == FCCommand::Type::GROUP) {
        return pAction->toolTip().toStdString();
    }
    else {
        return pCommand->getToolTipText();
    }
}

#ifdef WIN32
TDx::CImage NavlibInterface::FCCommand::extractImage() const
{
    const QIcon iconImg = pAction->icon();
    const QImage qimage = iconImg.pixmap(QSize(256, 256)).toImage();
    QByteArray qbyteArray;
    QBuffer qbuffer(&qbyteArray);
    qimage.save(&qbuffer, "PNG");

    return TDx::CImage::FromData(qbyteArray.toStdString(), 0, id().c_str());
}
#endif

TDx::SpaceMouse::CCommand NavlibInterface::FCCommand::toCCommand()const{
	return CCommand(id(), name(), description());
}

long NavlibInterface::SetActiveCommand(std::string commandId)
{
    if (commandId != "") {
        commands.at(commandId)->run();
    }
    return 0;
}

void NavlibInterface::unpackCommands(Gui::Command& command,
                                     TDx::SpaceMouse::CCategory& category,
                                     std::vector<TDx::CImage>& images)
{

	if (command.getAction() == nullptr) {
        return;
    }

	auto actionGroup = qobject_cast<Gui::ActionGroup*>(command.getAction());
    QList<QAction*> pActions;

    if (actionGroup != nullptr) {
        if (actionGroup->actions().size() > 1) {
            pActions = actionGroup->actions();
        }
    }
	else {
        pActions.push_back(command.getAction()->action());
	}

	uint32_t index = 0;

	for (QAction *pAction : pActions) {

		if (pAction->isSeparator()) {
            continue;
        }

		std::shared_ptr<FCCommand> pFCCommand = std::make_shared<FCCommand>(pAction, &command, index++);

		if (pFCCommand->id() == "") {
            continue;
		}

        commands.insert({pFCCommand->id(), pFCCommand});   
		category.push_back(pFCCommand->toCCommand());
		
#ifdef WIN32
        if (!pAction->icon().isNull()) {
            images.push_back(pFCCommand->extractImage());
        }
#endif
	}
    return;
}

void NavlibInterface::exportCommands(const std::string &workbench)
{
    if (errorCode) {
        return;
    }

    static const std::string substring("Workbench");
	static const std::string noneWorkbench("NoneWorkbench");

	if (workbench == noneWorkbench) {
        return;
	}

    std::string shortName(workbench);
    uint32_t index = shortName.find(substring);

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

    for (Gui::Command *command : workbenchCommands) {

        std::string groupName(command->getGroupName());

        if ((groupName == "<string>") || (groupName.empty())) {
            groupName = "Others";
        }

        CCategory category(groupName, groupName);
        unpackCommands(*command, category, images);
        commandsSet.push_back(std::move(category));
    }

    CNav3D::AddCommandSet(commandsSet);
    CNav3D::PutActiveCommands(workbench);
#ifdef WIN32
    CNav3D::AddImages(images);
#endif
}