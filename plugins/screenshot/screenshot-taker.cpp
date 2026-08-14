/*
 * %kadu copyright begin%
 * Copyright 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2012 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QWidget>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include "portal-screenshot.h"
#endif
#include "widgets/chat-widget/chat-widget.h"

#include "screenshot-taker.h"
#include "screenshot-taker.moc"

namespace
{
// Long enough for a window that has just been hidden to be gone from the screen before the picture
// is taken. The compositor decides when that has happened and tells nobody, so this is a wait, not
// a synchronisation.
constexpr int HideDelayMsec = 1000;
}

ScreenshotTaker::ScreenshotTaker(ChatWidget *chatWidget)
        : QObject(chatWidget), CurrentChatWidget(chatWidget), ChatWindowHidden(false)
{
}

ScreenshotTaker::~ScreenshotTaker()
{
    restoreChatWindow();
}

void ScreenshotTaker::init()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    Screenshot = new PortalScreenshot{this};

    connect(Screenshot, &PortalScreenshot::taken, this, &ScreenshotTaker::portalTaken);
    connect(Screenshot, &PortalScreenshot::failed, this, &ScreenshotTaker::portalFailed);
#endif
}

void ScreenshotTaker::takeStandardShot()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    request(false, true);
#else
    takeScreenShot(0, true);
#endif
}

void ScreenshotTaker::takeShotWithChatWindowHidden()
{
    CurrentChatWidget->window()->hide();
    ChatWindowHidden = true;

    QTimer::singleShot(HideDelayMsec, this, [this] {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        request(false, true);
#else
        takeScreenShot(0, true);
#endif
    });
}

void ScreenshotTaker::takeWindowShot()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // The desktop's own tool offers whatever it offers -- a window, an area, a whole screen -- and
    // what comes back is already what the user chose, so Kadu does not crop it afterwards.
    request(true, false);
#else
    takeScreenShot(CurrentChatWidget->window()->winId(), false);
#endif
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
void ScreenshotTaker::request(bool interactive, bool needsCrop)
{
    // Remembered rather than passed through the portal, which answers with a picture and nothing
    // else.
    NeedsCrop = needsCrop;

    Screenshot->take(interactive);
}

void ScreenshotTaker::portalTaken(QPixmap screenshot)
{
    restoreChatWindow();

    emit screenshotTaken(screenshot, NeedsCrop);
}

void ScreenshotTaker::portalFailed(const QString &errorMessage)
{
    restoreChatWindow();

    if (errorMessage.isEmpty())
        emit screenshotNotTaken();
    else
        emit screenshotFailed(errorMessage);
}
#else
void ScreenshotTaker::takeScreenShot(WId windowId, bool needsCrop)
{
    auto screen = QGuiApplication::primaryScreen();
    auto screenshot = screen ? screen->grabWindow(windowId) : QPixmap{};

    restoreChatWindow();

    if (screenshot.isNull())
        emit screenshotFailed(tr("Could not capture the screen."));
    else
        emit screenshotTaken(screenshot, needsCrop);
}
#endif

void ScreenshotTaker::restoreChatWindow()
{
    if (!ChatWindowHidden)
        return;

    ChatWindowHidden = false;
    CurrentChatWidget->window()->show();
}
