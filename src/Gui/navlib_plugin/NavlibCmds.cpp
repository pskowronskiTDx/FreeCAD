#include <PreCompiled.h>
#include "NavlibInterface.h"

#include <QScrollBar >
#include <QString>

#include <algorithm>
#include <iterator>

#include <SpaceMouse/CCategory.hpp>
#include <SpaceMouse/CCommand.hpp>
#include <SpaceMouse/CCommandSet.hpp>
#include <SpaceMouse/CImage.hpp>

#include <Gui/Action.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/MainWindow.h>
#include <Gui/View3DInventor.h>
#include <Gui/Workbench.h>
#include <Gui/WorkbenchManager.h>

constexpr uint8_t LCD_ICON_WIDTH = 24u;

NavlibInterface::ParsedData NavlibInterface::parseCommandId(const std::string& commandId) const
{
    ParsedData result {"", "", 0};

    if (commandId.empty())
        return result;

    auto groupDelimiter = std::find(commandId.cbegin(), commandId.cend(), '|');
    if (groupDelimiter >= --commandId.cend())
        return result;

    std::string groupName(commandId.cbegin(), groupDelimiter);
    groupDelimiter++;

    auto commandDelimiter = std::find(groupDelimiter, commandId.cend(), '|');
    if (commandDelimiter == --commandId.cend())
        return result;

    std::string commandName(groupDelimiter, commandDelimiter);
    int32_t actionIndex = -1;

    if (commandDelimiter != commandId.cend()) {
        QString indexStr =
            QString::fromStdString(std::string(++commandDelimiter, commandId.cend()).c_str());

        bool conversionOk = false;
        actionIndex = indexStr.toInt(&conversionOk);

        if (!conversionOk)
            return result;
    }

    result.groupName = groupName;
    result.commandName = commandName;
    result.actionIndex = actionIndex;

    return result;
}

std::string NavlibInterface::getId(const Gui::Command& command, const int32_t parameter) const
{
    std::string name = std::string(command.getName());

    if (parameter != -1) {
        name.push_back('|');
        name.append(std::to_string(parameter));
    }

    std::string groupName = command.getGroupName();

    if (groupName.compare("<string>") == 0 || groupName.empty())
        groupName = "Others";

    return groupName + "|" + name;
}

TDx::CImage NavlibInterface::getImage(const QAction& qaction, const std::string& id) const
{
    const QIcon iconImg = qaction.icon();
    const QImage qimage = iconImg.pixmap(QSize(LCD_ICON_WIDTH, LCD_ICON_WIDTH)).toImage();
    QByteArray qbyteArray;
    QBuffer qbuffer(&qbyteArray);
    qimage.save(&qbuffer, "PNG");

    return TDx::CImage::FromData(qbyteArray.toStdString(), 0, id.c_str());
}

void NavlibInterface::removeMarkups(std::string& text) const
{
    for (auto textBegin = text.cbegin(); textBegin != text.cend();) {
        auto markupBegin = std::find(textBegin, text.cend(), '<');
        if (markupBegin == text.cend())
            return;

        auto markupEnd = std::find(textBegin, text.cend(), '>');
        if (markupEnd == text.cend())
            return;

        if (markupBegin > markupEnd) {
            textBegin = markupBegin;
            continue;
        }

        if (*std::prev(markupEnd) == 'p')
            text.insert(std::next(markupEnd), 2, '\n');

        textBegin = text.erase(markupBegin, ++markupEnd);
    }
    return;
}

TDxCommand NavlibInterface::getCCommand(const Gui::Command& command,
                                      const QAction& qAction,
                                      const int32_t parameter) const
{
    std::string commandName = qAction.text().toStdString();
    std::string commandId = getId(command, parameter);

    if (commandName.empty() || commandId.empty())
        return TDxCommand();

    std::string commandDescription =
        parameter == -1 ? command.getToolTipText() : qAction.toolTip().toStdString();

    auto newEnd = std::remove(commandName.begin(), commandName.end(), '&');
    commandName.erase(newEnd, commandName.end());
    removeMarkups(commandDescription);

    return TDxCommand(commandId, commandName, commandDescription);
}

long NavlibInterface::SetActiveCommand(std::string commandId)
{
    ParsedData parsedData = parseCommandId(commandId);
    auto& commandManager = Gui::Application::Instance->commandManager();

    if (parsedData.groupName.compare("Others")) {
        auto commands = commandManager.getGroupCommands(parsedData.groupName.c_str());

        for (Gui::Command* command : commands) {
            if (!std::string(command->getName()).compare(parsedData.commandName)) {
                if (parsedData.actionIndex == -1) {
                    Gui::Action* pAction = command->getAction();
                    pAction->action()->trigger();
                }
                else
                    command->invoke(parsedData.actionIndex);
                return 0;
            }
        }
    }
    else
        commandManager.runCommandByName(parsedData.commandName.c_str());

    return 0;
}

void NavlibInterface::unpackCommands(Gui::Command& command,
                                     TDxCategory& category,
                                     std::vector<TDx::CImage>& images)
{
    if (command.getAction() == nullptr)
        return;

    QList<QAction*> pQActions;
    TDxCategory subCategory;
    int32_t index = -1;
    auto actionGroup = qobject_cast<Gui::ActionGroup*>(command.getAction());

    if (actionGroup != nullptr) {
        pQActions = actionGroup->actions();
        std::string subCategoryName = actionGroup->text().toStdString();
        subCategory = TDxCategory(subCategoryName, subCategoryName);
        index = 0;
    }
    else
        pQActions.push_back(command.getAction()->action());

    for (QAction* pQAction : pQActions) {
        if (pQAction->isSeparator())
            continue;

        TDxCommand ccommand = getCCommand(command, *pQAction, index);

        if (ccommand.GetId().empty())
            continue;

        if (!pQAction->icon().isNull()) {
            TDx::CImage commandImage = getImage(*pQAction, ccommand.GetId());
            images.push_back(commandImage);
        }

        if (pQActions.size() > 1)
            subCategory.push_back(std::move(ccommand));
        else {
            category.push_back(std::move(ccommand));
            return;
        }
        index++;
    }
    category.push_back(std::move(subCategory));

    return;
}

void NavlibInterface::exportCommands(const std::string& workbench)
{
    if (errorCode || (workbench.compare(noneWorkbenchStr) == 0))
        return;

    std::string shortName(workbench);
    size_t index = shortName.find(workbenchStr);

    if (index != std::string::npos)
        shortName.erase(index, workbenchStr.size());

    auto guiCommands = Gui::Application::Instance->commandManager().getAllCommands();

    std::vector<TDx::CImage> images;
    std::unordered_map<std::string, TDxCategory> categories;

    for (Gui::Command* command : guiCommands) {
        std::string groupName(command->getGroupName());
        std::string groupId(groupName);

        if ((groupName.compare("<string>") == 0) || groupName.empty()) {
            groupName = "...";
            groupId = "Others";
        }

        categories.emplace(groupName, TDxCategory(groupId, groupName));
        unpackCommands(*command, categories.at(groupName), images);
    }

    TDxCommandSet commandsSet(workbench, shortName);

    for (auto& itr : categories)
        commandsSet.push_back(std::move(itr.second));

    CNav3D::AddCommandSet(commandsSet);
    CNav3D::PutActiveCommands(workbench);
    CNav3D::AddImages(images);
}