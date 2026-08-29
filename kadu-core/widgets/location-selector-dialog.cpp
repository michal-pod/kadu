/*
 * %kadu copyright begin%
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

#include "location-selector-dialog.h"
#include "location-selector-dialog.moc"

#include <QtCore/QLocale>
#include <QtCore/QUrl>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QVBoxLayout>

LocationSelectorDialog::LocationSelectorDialog(QWidget *parent) : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);
    setMinimumSize(560, 420);
    resize(680, 520);
    setWindowTitle(tr("Send location"));

    auto *layout = new QVBoxLayout{this};
    auto *view = new QQuickWidget{this};
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/LocationSelector.qml")});
    layout->addWidget(view);

    if (auto *root = view->rootObject())
    {
        connect(root, SIGNAL(locationSelected(double, double)), this, SLOT(selectLocation(double, double)));
        connect(root, SIGNAL(cancelled()), this, SLOT(reject()));
    }
}

void LocationSelectorDialog::selectLocation(double latitude, double longitude)
{
    if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
        return;

    const auto geoUri = QStringLiteral("geo:%1,%2")
                            .arg(QLocale::c().toString(latitude, 'f', 6), QLocale::c().toString(longitude, 'f', 6));
    emit locationSelected(geoUri);
    accept();
}
