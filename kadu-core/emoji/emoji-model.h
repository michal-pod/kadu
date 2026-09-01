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

#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QStringView>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

struct EmojiCatalogueEntry;

class EmojiModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(QString categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)
	Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
	Q_PROPERTY(QString skinTone READ skinTone WRITE setSkinTone NOTIFY skinToneChanged)
	Q_PROPERTY(bool skinTonesDisabled READ skinTonesDisabled WRITE setSkinTonesDisabled NOTIFY skinTonesDisabledChanged)
	Q_PROPERTY(QStringList recentEmojis READ recentEmojis WRITE setRecentEmojis NOTIFY recentEmojisChanged)
	Q_PROPERTY(QVariantList categories READ categories CONSTANT)
	Q_PROPERTY(QString emojiFontFamily READ emojiFontFamily CONSTANT)
	Q_PROPERTY(QString unicodeVersion READ unicodeVersion CONSTANT)

public:
	enum Role
	{
		EmojiRole = Qt::UserRole,
		NameRole,
		CategoryRole,
	};
	Q_ENUM(Role)

	explicit EmojiModel(QObject *parent = nullptr);
	EmojiModel(const QString &emojiFontPath, QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = {}) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	QString categoryId() const;
	void setCategoryId(const QString &categoryId);
	QString searchText() const;
	void setSearchText(const QString &searchText);
	QString skinTone() const;
	void setSkinTone(const QString &skinTone);
	bool skinTonesDisabled() const;
	void setSkinTonesDisabled(bool disabled);
	QStringList recentEmojis() const;
	void setRecentEmojis(const QStringList &recentEmojis);
	QVariantList categories() const;
	QString emojiFontFamily() const;
	QString unicodeVersion() const;
	Q_INVOKABLE QVariantList entriesForCategory(const QString &categoryId) const;

signals:
	void categoryIdChanged();
	void searchTextChanged();
	void skinToneChanged();
	void skinTonesDisabledChanged();
	void recentEmojisChanged();

private:
	void rebuildVisibleEntries();
	const EmojiCatalogueEntry *entryForEmoji(const QString &emoji) const;
	QStringView emojiForEntry(const EmojiCatalogueEntry &emoji) const;
	bool entryMatchesSearch(const EmojiCatalogueEntry &emoji, QStringView renderedEmoji) const;

	struct VisibleEntry
	{
		const EmojiCatalogueEntry *emoji;
		QStringView renderedEmoji;
	};

	std::vector<VisibleEntry> m_visibleEntries;
	QVariantList m_categories;
	QStringList m_recentEmojis;
	QString m_categoryId{QStringLiteral("all")};
	QString m_searchText;
	QString m_skinTone;
	QString m_emojiFontFamily;
	QString m_unicodeVersion;
	bool m_skinTonesDisabled{false};
};
