/*
 * %kadu copyright begin%
 * Copyright 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#ifndef SCREENSHOT_TAKER_H
#define SCREENSHOT_TAKER_H

#include <QtCore/QObject>
#include <QtGui/QPixmap>
#include <QtGui/qwindowdefs.h>
#include <injeqt/injeqt.h>

class ChatWidget;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
class PortalScreenshot;
#endif

/**
 * @short Obtains a screenshot for a chat window.
 *
 * The three modes used to be three ways of reading the screen directly. Only the first two remain
 * ours to arrange: hiding the chat window before the picture is taken is a decision about Kadu's
 * own windows. Choosing a window or an area belongs to the desktop now, since a Wayland client
 * cannot see anyone else's window to point at it.
 */
class ScreenshotTaker : public QObject
{
    Q_OBJECT

    ChatWidget *CurrentChatWidget;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    PortalScreenshot *Screenshot;
    bool NeedsCrop;

    void request(bool interactive, bool needsCrop);
#else
    void takeScreenShot(WId windowId, bool needsCrop);
#endif

    bool ChatWindowHidden;
    void restoreChatWindow();

private slots:
    INJEQT_INIT void init();

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    void portalTaken(QPixmap screenshot);
    void portalFailed(const QString &errorMessage);
#endif

public:
    explicit ScreenshotTaker(ChatWidget *chatWidget);
    virtual ~ScreenshotTaker();

public slots:
    void takeStandardShot();
    void takeShotWithChatWindowHidden();
    void takeWindowShot();

signals:
    void screenshotTaken(QPixmap screenshot, bool needsCrop);
    void screenshotNotTaken();
    void screenshotFailed(const QString &errorMessage);
};

#endif   // SCREENSHOT_TAKER_H
