/*
 * %kadu copyright begin%
 * Copyright 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "chat-style/chat-style-manager.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>

#include "chat-configuration-holder.h"
#include "chat-configuration-holder.moc"

ChatConfigurationHolder::ChatConfigurationHolder(QObject *parent)
        : ConfigurationHolder{parent}, AutoSend{}, NiceDateFormat{}, CustomColors{}, ChatTextCustomColors{},
          ForceCustomChatFont{}, ChatBgFilled{}, UseTransparency{},
          TimelineDetails{ChatTimelineDetails::AllEvents}, ContactStateChats{}
{
}

ChatConfigurationHolder::~ChatConfigurationHolder()
{
}

void ChatConfigurationHolder::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void ChatConfigurationHolder::setChatStyleManager(ChatStyleManager *chatStyleManager)
{
    m_chatStyleManager = chatStyleManager;
    connect(m_chatStyleManager, &ChatStyleManager::chatStyleConfigurationUpdated, this, [this] {
        if (m_configuration)
            configurationUpdated();
    });
}

void ChatConfigurationHolder::init()
{
    configurationUpdated();
}

void ChatConfigurationHolder::configurationUpdated()
{
    AutoSend = m_configuration->deprecatedApi()->readBoolEntry("Chat", "AutoSend");
    NiceDateFormat = m_configuration->deprecatedApi()->readBoolEntry("Look", "NiceDateFormat");

    // Custom conversation colours belong to bundled themes only. External QML themes own their
    // palette completely, while the stored legacy values stay intact for a later return to a
    // bundled theme.
    const auto selectedStyle = m_configuration->deprecatedApi()->readEntry("Look", "Style", "KaduClassic");
    CustomColors = m_configuration->deprecatedApi()->readBoolEntry("Look", "ChatCustomColors") &&
                   (!m_chatStyleManager || m_chatStyleManager->isBuiltIn(selectedStyle));
    auto const palette = QGuiApplication::palette();

    ChatTextCustomColors =
        CustomColors && m_configuration->deprecatedApi()->readBoolEntry("Look", "ChatTextCustomColors");
    ChatTextBgColor = CustomColors ? m_configuration->deprecatedApi()->readColorEntry("Look", "ChatTextBgColor")
                                   : palette.base().color();
    ChatTextFontColor = CustomColors ? m_configuration->deprecatedApi()->readColorEntry("Look", "ChatTextFontColor")
                                     : palette.text().color();

    ForceCustomChatFont = m_configuration->deprecatedApi()->readBoolEntry("Look", "ForceCustomChatFont");
    ChatFont = m_configuration->deprecatedApi()->readFontEntry("Look", "ChatFont");

    // One side of the conversation stands on the window's own background and the other on the
    // colour a list uses for every second row, which is how a desktop tells two kinds of thing
    // apart without naming a colour of its own.
    MyBackgroundColor = CustomColors ? m_configuration->deprecatedApi()->readEntry("Look", "ChatMyBgColor")
                                     : palette.base().color().name();
    MyFontColor = CustomColors ? m_configuration->deprecatedApi()->readEntry("Look", "ChatMyFontColor")
                               : palette.text().color().name();
    MyNickColor = CustomColors ? m_configuration->deprecatedApi()->readEntry("Look", "ChatMyNickColor")
                               : palette.text().color().name();
    UsrBackgroundColor = CustomColors ? m_configuration->deprecatedApi()->readEntry("Look", "ChatUsrBgColor")
                                      : palette.alternateBase().color().name();
    UsrFontColor = CustomColors ? m_configuration->deprecatedApi()->readEntry("Look", "ChatUsrFontColor")
                                : palette.text().color().name();
    UsrNickColor = CustomColors ? m_configuration->deprecatedApi()->readEntry("Look", "ChatUsrNickColor")
                                : palette.text().color().name();

    ContactStateChats = m_configuration->deprecatedApi()->readBoolEntry("Chat", "ContactStateChats");

    ChatBgFilled = CustomColors && m_configuration->deprecatedApi()->readBoolEntry("Look", "ChatBgFilled");
    ChatBgColor = CustomColors ? m_configuration->deprecatedApi()->readColorEntry("Look", "ChatBgColor")
                               : palette.base().color();

    UseTransparency = m_configuration->deprecatedApi()->readBoolEntry("Chat", "UseTransparency");
    const auto timelineDetails = m_configuration->deprecatedApi()->readNumEntry(
        "Chat", "TimelineDetails", static_cast<int>(ChatTimelineDetails::AllEvents));
    const auto loadedTimelineDetails = static_cast<ChatTimelineDetails>(timelineDetails);
    TimelineDetails = isConcreteChatTimelineDetails(loadedTimelineDetails) ? loadedTimelineDetails
                                                                           : ChatTimelineDetails::AllEvents;

    emit chatConfigurationUpdated();
}
