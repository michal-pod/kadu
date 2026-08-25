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

#include <memory>

class QEvent;
class QWindow;

class KaWinTaskbarButton;

class KaWinTaskbarProgress
{
public:
    explicit KaWinTaskbarProgress(KaWinTaskbarButton *button);

    void setRange(int minimum, int maximum);
    void setVisible(bool visible);
    void setValue(int value);

private:
    KaWinTaskbarButton *m_button;
    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = 0;
    bool m_visible = false;

    friend class KaWinTaskbarButton;
};

class KaWinTaskbarButton : public QObject, private QAbstractNativeEventFilter
{
public:
    explicit KaWinTaskbarButton(QWindow *window = nullptr);
    ~KaWinTaskbarButton() override;

    void setWindow(QWindow *window);

    KaWinTaskbarProgress *progress();

private:
    class Private;
    std::unique_ptr<Private> m_private;
    KaWinTaskbarProgress m_progress;

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void synchronize();

    friend class KaWinTaskbarProgress;
};
