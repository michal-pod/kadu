/*
 * %kadu copyright begin%
 * Copyright 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2010, 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2009 Bartłomiej Zimoń (uzi18@o2.pl)
 * Copyright 2010, 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2012, 2013, 2014, 2015 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
 * Copyright 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2010, 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2009 Bartłomiej Zimoń (uzi18@o2.pl)
 * Copyright 2010, 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2012, 2013, 2014, 2015 Rafał Przemysław Malinowski (rafal.przemyslaw@kadu.im)
 * Copyright 2026 Kadu Qt6 port
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

#include "buddy-info-panel.h"
#include "buddy-info-panel.moc"

#include "avatars/avatar-id.h"
#include "avatars/avatars.h"
#include "buddies/buddy-preferred-manager.h"
#include "buddies/buddy.h"
#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "parser/parser.h"
#include "talkable/talkable-converter.h"
#include "widgets/info-panel-style-manager.h"

#include <QtCore/QMetaObject>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QVBoxLayout>

BuddyInfoPanel::BuddyInfoPanel(QWidget *parent) : QFrame{parent}
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new QQuickWidget{this};
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setClearColor(Qt::transparent);
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->rootContext()->setContextProperty(QStringLiteral("_infoPanel"), this);
    m_view->setSource(QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/InfoPanel.qml")});
    layout->addWidget(m_view);
}

BuddyInfoPanel::~BuddyInfoPanel()
{
    disconnect(m_buddyPreferredManager, nullptr, this, nullptr);
}

QString BuddyInfoPanel::displayName() const
{
    return m_displayName;
}
QUrl BuddyInfoPanel::avatarSource() const
{
    return m_avatarSource;
}
QString BuddyInfoPanel::detailsText() const
{
    return m_detailsText;
}
QString BuddyInfoPanel::statusText() const
{
    return m_statusText;
}
QString BuddyInfoPanel::descriptionText() const
{
    return m_descriptionText;
}
QString BuddyInfoPanel::style() const
{
    return m_style;
}
QUrl BuddyInfoPanel::styleSource() const
{
    return m_styleSource;
}
QString BuddyInfoPanel::colorScheme() const
{
    return m_colorScheme;
}
bool BuddyInfoPanel::useCustomColors() const
{
    return m_useCustomColors;
}
QString BuddyInfoPanel::foregroundColor() const
{
    return m_foregroundColor;
}
QString BuddyInfoPanel::backgroundColor() const
{
    return m_backgroundColor;
}
QString BuddyInfoPanel::fontFamily() const
{
    return m_fontFamily;
}
int BuddyInfoPanel::fontPointSize() const
{
    return m_fontPointSize;
}
bool BuddyInfoPanel::fontBold() const
{
    return m_fontBold;
}
bool BuddyInfoPanel::fontItalic() const
{
    return m_fontItalic;
}
bool BuddyInfoPanel::fontUnderline() const
{
    return m_fontUnderline;
}
bool BuddyInfoPanel::showScrollBar() const
{
    return m_showScrollBar;
}

QString BuddyInfoPanel::selectedText() const
{
    return m_view && m_view->rootObject() ? m_view->rootObject()->property("selectedText").toString() : QString{};
}

void BuddyInfoPanel::setAvatars(Avatars *avatars)
{
    m_avatars = avatars;
}
void BuddyInfoPanel::setBuddyPreferredManager(BuddyPreferredManager *manager)
{
    m_buddyPreferredManager = manager;
}
void BuddyInfoPanel::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}
void BuddyInfoPanel::setInfoPanelStyleManager(InfoPanelStyleManager *manager)
{
    m_infoPanelStyleManager = manager;
}
void BuddyInfoPanel::setParser(Parser *parser)
{
    m_parser = parser;
}
void BuddyInfoPanel::setTalkableConverter(TalkableConverter *converter)
{
    m_talkableConverter = converter;
}

void BuddyInfoPanel::init()
{
    connect(m_avatars, &Avatars::updated, this, &BuddyInfoPanel::avatarUpdated);
    connect(m_buddyPreferredManager, SIGNAL(buddyUpdated(Buddy)), this, SLOT(buddyUpdated(Buddy)));
    configurationUpdated();
}

void BuddyInfoPanel::configurationUpdated()
{
    if (!m_configuration)
        return;

    const auto font = m_configuration->deprecatedApi()->readFontEntry("Look", "PanelFont");
    m_style = m_configuration->deprecatedApi()->readEntry("Look", "InfoPanelStyle", "Classic");
    m_style =
        m_infoPanelStyleManager ? m_infoPanelStyleManager->normalizedStyleName(m_style) : QStringLiteral("Classic");
    m_styleSource = m_infoPanelStyleManager ? m_infoPanelStyleManager->styleSource(m_style) : QUrl{};
    const auto configuredColorScheme =
        m_configuration->deprecatedApi()->readEntry("Look", "InfoPanelStyleVariant", "System");
    m_colorScheme = m_infoPanelStyleManager
                        ? m_infoPanelStyleManager->normalizedColorScheme(m_style, configuredColorScheme)
                        : QStringLiteral("System");
    m_useCustomColors = m_configuration->deprecatedApi()->readBoolEntry("Look", "InfoPanelCustomColors") &&
                        (!m_infoPanelStyleManager || m_infoPanelStyleManager->isBuiltIn(m_style));
    m_foregroundColor = m_useCustomColors
                            ? m_configuration->deprecatedApi()->readColorEntry("Look", "InfoPanelFgColor").name()
                            : QGuiApplication::palette().text().color().name();
    m_backgroundColor = m_useCustomColors && m_configuration->deprecatedApi()->readBoolEntry("Look", "InfoPanelBgFilled")
                            ? m_configuration->deprecatedApi()->readColorEntry("Look", "InfoPanelBgColor").name()
                            : QStringLiteral("transparent");
    m_fontFamily = font.family();
    m_fontPointSize = font.pointSize() > 0 ? font.pointSize() : 10;
    m_fontBold = font.bold();
    m_fontItalic = font.italic();
    m_fontUnderline = font.underline();
    m_showScrollBar = m_configuration->deprecatedApi()->readBoolEntry("Look", "PanelVerticalScrollbar");
    update();
}

void BuddyInfoPanel::buddyUpdated(const Buddy &buddy)
{
    if (m_talkableConverter && buddy == m_talkableConverter->toBuddy(m_item))
        update();
}

void BuddyInfoPanel::update()
{
    if (m_item.isEmpty() || !m_talkableConverter || !m_parser)
    {
        m_displayName.clear();
        m_avatarSource = {};
        m_detailsText.clear();
        m_statusText.clear();
        m_descriptionText.clear();
    }
    else
    {
        m_displayName = m_talkableConverter->toDisplay(m_item);
        const auto avatarPath = m_talkableConverter->toAvatarPath(m_item);
        m_avatarSource = avatarPath.isEmpty() ? QUrl{} : QUrl::fromLocalFile(avatarPath);
        m_detailsText = m_parser->parse(
            QStringLiteral("[<b>%a</b>][ (%u)][<br>tel.: %m][<br>IP: %i]"), m_item, ParserEscape::HtmlEscape);
        m_statusText = m_parser->parse(QStringLiteral("%s"), m_item, ParserEscape::HtmlEscape);
        m_descriptionText = m_parser->parse(QStringLiteral("%d"), m_item, ParserEscape::HtmlEscape);
    }
    emit panelChanged();
}

void BuddyInfoPanel::avatarUpdated(const AvatarId &id)
{
    if (!m_talkableConverter)
        return;
    if (id == avatarId(m_talkableConverter->toBuddy(m_item)) || id == avatarId(m_talkableConverter->toContact(m_item)))
        update();
}

void BuddyInfoPanel::connectItem()
{
    if (!m_talkableConverter)
        return;
    const auto buddy = m_talkableConverter->toBuddy(m_item);
    if (buddy)
        connect(buddy, SIGNAL(updated()), this, SLOT(update()));
    const auto contact = m_talkableConverter->toContact(m_item);
    if (contact)
        connect(contact, SIGNAL(updated()), this, SLOT(update()));
}

void BuddyInfoPanel::disconnectItem()
{
    if (!m_talkableConverter)
        return;
    const auto buddy = m_talkableConverter->toBuddy(m_item);
    if (buddy)
        disconnect(buddy, nullptr, this, nullptr);
    const auto contact = m_talkableConverter->toContact(m_item);
    if (contact)
        disconnect(contact, nullptr, this, nullptr);
}

void BuddyInfoPanel::displayItem(Talkable item)
{
    disconnectItem();
    m_item = item;
    connectItem();
    if (isVisible())
        update();
}

void BuddyInfoPanel::setVisible(bool visible)
{
    QFrame::setVisible(visible);
    if (visible)
        update();
}

void BuddyInfoPanel::copySelection()
{
    if (m_view && m_view->rootObject())
        QMetaObject::invokeMethod(m_view->rootObject(), "copySelection");
}
