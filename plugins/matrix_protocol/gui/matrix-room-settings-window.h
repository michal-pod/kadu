/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#pragma once

#include "chat/chat.h"

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVector>
#include <QtWidgets/QWidget>

#include <optional>

class MatrixPowerLevelEditor;
class IconsManager;
class QCloseEvent;
class QComboBox;
class QCompleter;
class QFormLayout;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPlainTextEdit;
class QPixmap;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTabWidget;
class QTableWidget;
class QToolButton;

namespace Quotient
{
class Connection;
class Room;
}

class MatrixRoomSettingsWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit MatrixRoomSettingsWindow(
        const Chat &chat, Quotient::Connection *connection, Quotient::Room *room, IconsManager *iconsManager,
        QWidget *parent = nullptr);
    virtual ~MatrixRoomSettingsWindow() = default;

protected:
    virtual void closeEvent(QCloseEvent *event) override;

private:
    enum MemberSearchDataRole
    {
        MemberIdRole = Qt::UserRole + 1,
        MemberSearchTextRole
    };

    enum class PowerLevelLocation
    {
        Root,
        Events,
        Notifications
    };

    struct PowerLevelSetting
    {
        QString label;
        QString key;
        PowerLevelLocation location;
        bool stateEvent;
        qint64 specificationDefault;
        MatrixPowerLevelEditor *editor;
    };

    struct UserPowerLevelSetting
    {
        QString userId;
        QLabel *avatarLabel;
        QLabel *nameLabel;
        QLabel *mxidLabel;
        MatrixPowerLevelEditor *editor;
    };

    Chat m_chat;
    QPointer<Quotient::Connection> m_connection;
    QPointer<Quotient::Room> m_room;
    QPointer<IconsManager> m_iconsManager;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QPlainTextEdit *m_topicEdit = nullptr;
    QLabel *m_avatarPreview = nullptr;
    QPushButton *m_changeAvatarButton = nullptr;
    QPushButton *m_removeAvatarButton = nullptr;
    QLabel *m_accessSummaryLabel = nullptr;
    QLabel *m_historySummaryLabel = nullptr;
    QLabel *m_encryptionSummaryLabel = nullptr;
    QLabel *m_directorySummaryLabel = nullptr;
    QLabel *m_membersSummaryLabel = nullptr;
    QComboBox *m_joinRuleCombo = nullptr;
    QComboBox *m_historyVisibilityCombo = nullptr;
    QComboBox *m_guestAccessCombo = nullptr;
    QLineEdit *m_memberSearchEdit = nullptr;
    QToolButton *m_addMemberButton = nullptr;
    QTableWidget *m_userPowerLevelsTable = nullptr;
    QStandardItemModel *m_memberSearchModel = nullptr;
    QSortFilterProxyModel *m_memberSearchProxy = nullptr;
    QCompleter *m_memberCompleter = nullptr;
    QLabel *m_accessEncryptionLabel = nullptr;
    QLabel *m_roomVersionLabel = nullptr;
    QLabel *m_canonicalAliasLabel = nullptr;
    QLabel *m_predecessorLabel = nullptr;
    QLabel *m_successorLabel = nullptr;
    QWidget *m_customPowerLevelsGroup = nullptr;
    QFormLayout *m_customPowerLevelsForm = nullptr;
    QPushButton *m_okButton = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    int m_communicationTabIndex = -1;
    int m_moderationTabIndex = -1;
    int m_administrationTabIndex = -1;
    int m_usersTabIndex = -1;

    QString m_savedName;
    QString m_savedTopic;
    QUrl m_savedAvatarUrl;
    QString m_avatarFileName;
    QJsonObject m_savedJoinRules;
    QJsonObject m_savedHistoryVisibility;
    QJsonObject m_savedGuestAccess;
    QJsonObject m_savedPowerLevels;
    QVector<PowerLevelSetting> m_powerLevelSettings;
    QVector<UserPowerLevelSetting> m_userPowerLevelSettings;
    QHash<QString, QString> m_memberDisplayNames;
    QHash<QString, QUrl> m_memberAvatarUrls;
    QString m_selectedMemberId;
    bool m_customPowerLevelsCreated = false;
    bool m_memberSearchLoading = false;
    bool m_memberSearchLoaded = false;
    bool m_removeAvatar = false;
    bool m_saving = false;
    bool m_closeAfterSave = false;
    int m_pendingOperations = 0;
    QStringList m_errors;

    void createGui();
    QWidget *createGeneralTab();
    QWidget *createAccessTab();
    QWidget *createCommunicationTab();
    QWidget *createModerationTab();
    QWidget *createAdministrationTab();
    QWidget *createUsersTab();
    QWidget *createAdvancedTab();
    void addPowerLevelSetting(
        QFormLayout *form, const QString &label, const QString &key, PowerLevelLocation location,
        bool stateEvent = false, qint64 specificationDefault = 0);
    void createCustomPowerLevelSettings();
    void ensureMemberSearchModel();
    void refreshMemberSearch();
    void selectMemberSearchResult(const QModelIndex &index);
    void addSelectedMember();
    void loadUserPowerLevels();
    void addUserPowerLevel(const QString &userId, std::optional<qint64> explicitValue, bool selectRow = false);
    void refreshUserPowerLevel(const QString &userId);
    void connectRoom();
    void loadRoomData();
    void loadPowerLevels();
    void loadDirectoryVisibility();
    void refreshPermissions();
    void refreshState();
    void refreshRoomInformation();
    void refreshPowerLevelDefaults();
    void refreshAvatarPreview();
    void setAvatarPreview(const QPixmap &avatar);
    void setSaving(bool saving);
    bool canSendState(const QString &eventType) const;
    bool hasChanges() const;
    bool hasAccessChanges() const;
    bool hasPowerLevelChanges() const;
    bool powerLevelEditorsValid() const;
    qint64 desiredRootPowerLevel(const QString &key, qint64 fallback) const;
    QJsonObject desiredJoinRules() const;
    QJsonObject desiredHistoryVisibility() const;
    QJsonObject desiredGuestAccess() const;
    QJsonObject desiredPowerLevels() const;
    QString joinRuleName(const QString &joinRule) const;
    QString historyVisibilityName(const QString &historyVisibility) const;
    QString guestAccessName(const QString &guestAccess) const;
    void save(bool closeAfterSave);
    void startStateUpdate(
        const QString &eventType, const QJsonObject &content, const QString &settingName,
        bool countOperation = true);
    void startAvatarUpdate();
    void finishOperation(const QString &error = {});

private slots:
    void chooseAvatar();
    void removeAvatar();
};
