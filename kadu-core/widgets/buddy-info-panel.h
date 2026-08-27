/*
 * %kadu copyright begin%
 * Copyright 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2010, 2012 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
 * Copyright 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2010, 2012 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw@kadu.im)
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

#pragma once

#include "configuration/configuration-aware-object.h"
#include "talkable/talkable.h"

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtWidgets/QFrame>
#include <injeqt/injeqt.h>

class Avatars;
class Buddy;
class BuddyPreferredManager;
class Configuration;
class InfoPanelStyleManager;
class Parser;
class QQuickWidget;
class TalkableConverter;
struct AvatarId;

class BuddyInfoPanel : public QFrame, private ConfigurationAwareObject
{
    Q_OBJECT

    Q_PROPERTY(QString displayName READ displayName NOTIFY panelChanged)
    Q_PROPERTY(QUrl avatarSource READ avatarSource NOTIFY panelChanged)
    Q_PROPERTY(QString detailsText READ detailsText NOTIFY panelChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY panelChanged)
    Q_PROPERTY(QString descriptionText READ descriptionText NOTIFY panelChanged)
    Q_PROPERTY(QString style READ style NOTIFY panelChanged)
    Q_PROPERTY(QUrl styleSource READ styleSource NOTIFY panelChanged)
    Q_PROPERTY(QString foregroundColor READ foregroundColor NOTIFY panelChanged)
    Q_PROPERTY(QString backgroundColor READ backgroundColor NOTIFY panelChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY panelChanged)
    Q_PROPERTY(int fontPointSize READ fontPointSize NOTIFY panelChanged)
    Q_PROPERTY(bool fontBold READ fontBold NOTIFY panelChanged)
    Q_PROPERTY(bool fontItalic READ fontItalic NOTIFY panelChanged)
    Q_PROPERTY(bool fontUnderline READ fontUnderline NOTIFY panelChanged)
    Q_PROPERTY(bool showScrollBar READ showScrollBar NOTIFY panelChanged)

public:
    explicit BuddyInfoPanel(QWidget *parent = nullptr);
    ~BuddyInfoPanel() override;

    QString displayName() const;
    QUrl avatarSource() const;
    QString detailsText() const;
    QString statusText() const;
    QString descriptionText() const;
    QString style() const;
    QUrl styleSource() const;
    QString foregroundColor() const;
    QString backgroundColor() const;
    QString fontFamily() const;
    int fontPointSize() const;
    bool fontBold() const;
    bool fontItalic() const;
    bool fontUnderline() const;
    bool showScrollBar() const;
    QString selectedText() const;

    void setVisible(bool visible) override;

public slots:
    void displayItem(Talkable item);
    void update();
    void copySelection();

signals:
    void panelChanged();

private:
    QPointer<Avatars> m_avatars;
    QPointer<BuddyPreferredManager> m_buddyPreferredManager;
    QPointer<Configuration> m_configuration;
    QPointer<InfoPanelStyleManager> m_infoPanelStyleManager;
    QPointer<Parser> m_parser;
    QPointer<TalkableConverter> m_talkableConverter;
    Talkable m_item;
    QQuickWidget *m_view = nullptr;
    QString m_displayName;
    QUrl m_avatarSource;
    QString m_detailsText;
    QString m_statusText;
    QString m_descriptionText;
    QString m_style;
    QUrl m_styleSource;
    QString m_foregroundColor;
    QString m_backgroundColor;
    QString m_fontFamily;
    int m_fontPointSize = 10;
    bool m_fontBold = false;
    bool m_fontItalic = false;
    bool m_fontUnderline = false;
    bool m_showScrollBar = false;

    void avatarUpdated(const AvatarId &id);
    void connectItem();
    void disconnectItem();

protected:
    void configurationUpdated() override;

private slots:
    INJEQT_SET void setAvatars(Avatars *avatars);
    INJEQT_SET void setBuddyPreferredManager(BuddyPreferredManager *buddyPreferredManager);
    INJEQT_SET void setConfiguration(Configuration *configuration);
    INJEQT_SET void setInfoPanelStyleManager(InfoPanelStyleManager *infoPanelStyleManager);
    INJEQT_SET void setParser(Parser *parser);
    INJEQT_SET void setTalkableConverter(TalkableConverter *talkableConverter);
    INJEQT_INIT void init();
    void buddyUpdated(const Buddy &buddy);
};
