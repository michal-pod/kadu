/*
 * %kadu copyright begin%
 * Copyright 2009, 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2009 Bartłomiej Zimoń (uzi18@o2.pl)
 * Copyright 2010, 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "preview.h"
#include "preview.moc"

#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QVBoxLayout>

Preview::Preview(QWidget *parent) : QFrame{parent}
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setFixedHeight(190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    auto panel = QVariantMap{{QStringLiteral("displayName"), QStringLiteral("Michał Kowalski")},
                             {QStringLiteral("avatarSource"), QUrl{}},
                             {QStringLiteral("detailsText"), QStringLiteral("michal@example.org<br>tel.: +48 600 000 000")},
                             {QStringLiteral("statusText"), QStringLiteral("Dostępny")},
                             {QStringLiteral("descriptionText"), QStringLiteral("Przykładowy opis kontaktu.")},
                             {QStringLiteral("style"), QStringLiteral("Classic")},
                             {QStringLiteral("foregroundColor"), QGuiApplication::palette().text().color().name()},
                             {QStringLiteral("backgroundColor"), QStringLiteral("transparent")},
                             {QStringLiteral("fontFamily"), QString{}},
                             {QStringLiteral("fontPointSize"), 10},
                             {QStringLiteral("fontBold"), false},
                             {QStringLiteral("fontItalic"), false},
                             {QStringLiteral("fontUnderline"), false},
                             {QStringLiteral("showScrollBar"), true}};

    m_view = new QQuickWidget{this};
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setClearColor(Qt::transparent);
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->rootContext()->setContextProperty(QStringLiteral("_infoPanel"), panel);
    m_view->setSource(QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/InfoPanel.qml")});
    layout->addWidget(m_view);
}

Preview::~Preview() = default;

void Preview::setStyleSource(const QUrl &source)
{
    if (m_view && m_view->rootObject())
        m_view->rootObject()->setProperty("styleSource", source);
}

void Preview::setColorScheme(const QString &scheme)
{
    if (m_view && m_view->rootObject())
        m_view->rootObject()->setProperty("colorScheme", scheme);
}
