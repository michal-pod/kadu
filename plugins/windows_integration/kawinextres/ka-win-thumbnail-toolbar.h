/*
 * %kadu copyright begin%
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

#pragma once

#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtGui/QIcon>

#include <memory>

class QEvent;
class QWindow;

class KaWinThumbnailToolButton;

class KaWinThumbnailToolBar : public QObject, private QAbstractNativeEventFilter
{
public:
    explicit KaWinThumbnailToolBar(QWindow *window = nullptr);
    ~KaWinThumbnailToolBar() override;

    void setWindow(QWindow *window);

    void clear();

    void addButton(KaWinThumbnailToolButton *button);

private:
    class Private;
    std::unique_ptr<Private> m_private;

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void synchronize();
    void buttonChanged(KaWinThumbnailToolButton *button);

    friend class KaWinThumbnailToolButton;
};

class KaWinThumbnailToolButton : public QObject
{
    Q_OBJECT

public:
    explicit KaWinThumbnailToolButton(KaWinThumbnailToolBar *parent = nullptr);

    void setToolTip(const QString &toolTip);
    void setIcon(const QIcon &icon);
    void setDismissOnClick(bool dismissOnClick);

signals:
    void clicked();

private:
    QString m_toolTip;
    QIcon m_icon;
    bool m_dismissOnClick = false;
    KaWinThumbnailToolBar *m_toolBar = nullptr;

    friend class KaWinThumbnailToolBar;
};
