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

#include "emoji-model.h"
#include "emoji-catalog-generated.h"
#include "emoji-font.h"

#include <algorithm>

EmojiModel::EmojiModel(QObject *parent) : EmojiModel{QString{}, parent}
{
}

EmojiModel::EmojiModel(const QString &emojiFontPath, QObject *parent) : QAbstractListModel{parent}
{
	if (!emojiFontPath.isEmpty())
		m_emojiFontFamily = EmojiFont::registerFont(emojiFontPath);

	QVariantMap recentCategory;
	recentCategory.insert(QStringLiteral("id"), QStringLiteral("recent"));
	m_categories.append(recentCategory);
	for (const auto category : emojiCatalogueCategories())
	{
		QVariantMap categoryData;
		categoryData.insert(QStringLiteral("id"), category.toString());
		m_categories.append(categoryData);
	}
	m_unicodeVersion = emojiCatalogueUnicodeVersion().toString();
	rebuildVisibleEntries();
}

int EmojiModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_visibleEntries.size());
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
		return {};

	const auto &visibleEntry = m_visibleEntries[static_cast<size_t>(index.row())];
	const auto &emoji = *visibleEntry.emoji;
	switch (role)
	{
	case EmojiRole: return visibleEntry.renderedEmoji.toString();
	case NameRole: return emoji.name.toString();
	case CategoryRole: return emoji.category.toString();
	default: return {};
	}
}

QHash<int, QByteArray> EmojiModel::roleNames() const
{
	return {{EmojiRole, "emoji"}, {NameRole, "name"}, {CategoryRole, "category"}};
}

QString EmojiModel::categoryId() const
{
	return m_categoryId;
}

void EmojiModel::setCategoryId(const QString &categoryId)
{
	if (m_categoryId == categoryId)
		return;

	m_categoryId = categoryId;
	beginResetModel();
	rebuildVisibleEntries();
	endResetModel();
	emit categoryIdChanged();
}

QString EmojiModel::searchText() const
{
	return m_searchText;
}

void EmojiModel::setSearchText(const QString &searchText)
{
	if (m_searchText == searchText)
		return;

	m_searchText = searchText;
	beginResetModel();
	rebuildVisibleEntries();
	endResetModel();
	emit searchTextChanged();
}

QString EmojiModel::skinTone() const
{
	return m_skinTone;
}

void EmojiModel::setSkinTone(const QString &skinTone)
{
	if (m_skinTone == skinTone)
		return;

	m_skinTone = skinTone;
	beginResetModel();
	rebuildVisibleEntries();
	endResetModel();
	emit skinToneChanged();
}

bool EmojiModel::skinTonesDisabled() const
{
	return m_skinTonesDisabled;
}

void EmojiModel::setSkinTonesDisabled(bool disabled)
{
	if (m_skinTonesDisabled == disabled)
		return;

	m_skinTonesDisabled = disabled;
	beginResetModel();
	rebuildVisibleEntries();
	endResetModel();
	emit skinTonesDisabledChanged();
}

QStringList EmojiModel::recentEmojis() const
{
	return m_recentEmojis;
}

void EmojiModel::setRecentEmojis(const QStringList &recentEmojis)
{
	if (m_recentEmojis == recentEmojis)
		return;

	m_recentEmojis = recentEmojis;
	beginResetModel();
	rebuildVisibleEntries();
	endResetModel();
	emit recentEmojisChanged();
}

QVariantList EmojiModel::categories() const
{
	return m_categories;
}

QString EmojiModel::emojiFontFamily() const
{
	return m_emojiFontFamily;
}

QString EmojiModel::unicodeVersion() const
{
	return m_unicodeVersion;
}

QVariantList EmojiModel::entriesForCategory(const QString &categoryId) const
{
	QVariantList entries;
	if (categoryId == QLatin1String{"recent"})
	{
		if (!m_searchText.isEmpty())
			return entries;

		for (const auto &recentEmoji : m_recentEmojis)
		{
			if (const auto *emoji = entryForEmoji(recentEmoji))
			{
				QVariantMap entry;
				entry.insert(QStringLiteral("emoji"), recentEmoji);
				entry.insert(QStringLiteral("name"), emoji->name.toString());
				entry.insert(QStringLiteral("category"), QStringLiteral("recent"));
				entries.append(entry);
			}
		}
		return entries;
	}

	for (const auto &emoji : emojiCatalogueEntries())
	{
		if (emoji.category.toString() != categoryId)
			continue;

		const auto renderedEmoji = emojiForEntry(emoji);
		if (!m_searchText.isEmpty() && !entryMatchesSearch(emoji, renderedEmoji))
			continue;

		QVariantMap entry;
		entry.insert(QStringLiteral("emoji"), renderedEmoji.toString());
		entry.insert(QStringLiteral("name"), emoji.name.toString());
		entry.insert(QStringLiteral("category"), emoji.category.toString());
		entries.append(entry);
	}
	return entries;
}

void EmojiModel::rebuildVisibleEntries()
{
	m_visibleEntries.clear();
	if (m_searchText.isEmpty() && m_categoryId == QLatin1String{"recent"})
	{
		for (const auto &recentEmoji : m_recentEmojis)
		{
			if (const auto *emoji = entryForEmoji(recentEmoji))
				m_visibleEntries.push_back({emoji, QStringView{recentEmoji}});
		}
		return;
	}

	for (const auto &emoji : emojiCatalogueEntries())
	{
		const auto renderedEmoji = emojiForEntry(emoji);
		if (!m_searchText.isEmpty())
		{
			if (entryMatchesSearch(emoji, renderedEmoji))
				m_visibleEntries.push_back({&emoji, renderedEmoji});
		}
		else if (m_categoryId == QLatin1String{"all"} || emoji.category.toString() == m_categoryId)
			m_visibleEntries.push_back({&emoji, renderedEmoji});
	}
}

const EmojiCatalogueEntry *EmojiModel::entryForEmoji(const QString &emoji) const
{
	const auto entries = emojiCatalogueEntries();
	const auto entry = std::find_if(entries.begin(), entries.end(), [&emoji](const auto &candidate) {
		if (candidate.value.toString() == emoji)
			return true;
		return std::any_of(candidate.tones.cbegin(), candidate.tones.cend(), [&emoji](const auto &tone) {
			return !tone.value.isEmpty() && tone.value.toString() == emoji;
		});
	});
	return entry == entries.end() ? nullptr : &*entry;
}

QStringView EmojiModel::emojiForEntry(const EmojiCatalogueEntry &emoji) const
{
	if (!m_skinTonesDisabled && !m_skinTone.isEmpty())
	{
		const auto tone = std::find_if(emoji.tones.cbegin(), emoji.tones.cend(), [this](const auto &candidate) {
			return candidate.id.toString() == m_skinTone;
		});
		if (tone != emoji.tones.cend() && !tone->value.isEmpty())
			return tone->value;
	}
	return emoji.value;
}

bool EmojiModel::entryMatchesSearch(const EmojiCatalogueEntry &emoji, QStringView renderedEmoji) const
{
	return emoji.name.toString().contains(m_searchText, Qt::CaseInsensitive) || renderedEmoji.toString().contains(m_searchText);
}
