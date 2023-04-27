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
    if (type == FCCommand::Type::GROUP) {
        return std::string(pCommand->getName()) + "_" + pAction->text().toStdString();
    }
    else {
        return pCommand->getName();
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

void NavlibInterface::extractCommand(Gui::Command& command,
                                     TDx::SpaceMouse::CCategory& category,
                                     std::vector<TDx::CImage>& images)
{

	if (command.getAction() == nullptr) {
        return;
    }

	auto actionsGroup = qobject_cast<Gui::ActionGroup*>(command.getAction());

    if (actionsGroup != nullptr) {
        if (actionsGroup->actions().size() > 1) {
            return extractCommands(*actionsGroup, command, category, images);
        }
    }

    const char *commandId = command.getName();

    if (commands.find(commandId) == commands.end()) {
        commands.insert({commandId, std::make_shared<FCCommand>(command.getAction()->action(), &command)});
    }

	if (commands.at(commandId)->toCCommand().Label == "") {
        return;
    }

    category.push_back(commands.at(commandId)->toCCommand());

#ifdef WIN32
    if (!command.getAction()->icon().isNull()) {
        images.push_back(commands.at(commandId)->extractImage());
    }
#endif

    return;
}

void NavlibInterface::extractCommands(const Gui::ActionGroup& actionsGroup,
	                                  Gui::Command& command,
                                      TDx::SpaceMouse::CCategory& category,
                                      std::vector<TDx::CImage>& images)
{

    uint32_t index = 0;

    for (QAction* pAction : actionsGroup.actions()) {

        if (pAction->isSeparator()) {
            continue;
        }

        std::string commandId =
            std::string(command.getName()) + "_" + pAction->text().toStdString();

        commands.insert({commandId, std::make_shared<FCCommand>(pAction, &command, index++)});

        if (commands.at(commandId)->toCCommand().Label == "") {
            return;
        }

        category.push_back(commands.at(commandId)->toCCommand());

#ifdef WIN32
        if (!pAction->icon().isNull()) {
            images.push_back(commands.at(commandId)->extractImage());
        }
#endif
    }
}

void NavlibInterface::exportCommands(const std::string &workbench)
{
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
        extractCommand(*command, category, images);
        commandsSet.push_back(std::move(category));
    }

    CNav3D::AddCommandSet(commandsSet);
    CNav3D::PutActiveCommands(workbench);
#ifdef WIN32
    CNav3D::AddImages(images);
#endif
}