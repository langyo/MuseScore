/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "fontsdatabase.h"

#include <QFontDatabase>

#include "global/io/file.h"
#include "global/io/dir.h"
#include "global/serialization/json.h"

#include "log.h"

using namespace muse::draw;

static int s_fontID = -1;

void FontsDatabase::setDefaultFont(Font::Type type, const FontDataKey& key)
{
    m_defaults[type] = key;
}

const FontDataKey& FontsDatabase::defaultFont(Font::Type type) const
{
    auto it = m_defaults.find(type);
    if (it != m_defaults.end()) {
        return it->second;
    }

    it = m_defaults.find(Font::Type::Unknown);
    IF_ASSERT_FAILED(it != m_defaults.end()) {
        static FontDataKey null;
        return null;
    }
    return it->second;
}

int FontsDatabase::addFont(const FontDataKey& key, const mu::io::path_t& path)
{
    s_fontID++;
    m_fonts.push_back(FontInfo { s_fontID, key, path });

    QFontDatabase::addApplicationFont(path.toQString());

    return s_fontID;
}

FontDataKey FontsDatabase::actualFont(const FontDataKey& requireKey, Font::Type type) const
{
    mu::io::path_t path = fontInfo(requireKey).path;
    if (!path.empty() && mu::io::File::exists(path)) {
        return requireKey;
    }

    return defaultFont(type);
}

std::vector<FontDataKey> FontsDatabase::substitutionFonts(Font::Type type) const
{
    auto it = m_substitutions.find(type);
    if (it != m_substitutions.end()) {
        return it->second;
    }

    static std::vector<FontDataKey> null;
    return null;
}

FontData FontsDatabase::fontData(const FontDataKey& requireKey, Font::Type type) const
{
    FontDataKey key = actualFont(requireKey, type);
    mu::io::path_t path = fontInfo(key).path;
    IF_ASSERT_FAILED(mu::io::File::exists(path)) {
        return FontData();
    }

    mu::io::File file(path);
    if (!file.open()) {
        LOGE() << "failed open font file: " << path;
        return FontData();
    }

    FontData fd;
    fd.key = key;
    fd.data = file.readAll();
    return fd;
}

mu::io::path_t FontsDatabase::fontPath(const FontDataKey& requireKey, Font::Type type) const
{
    FontDataKey key = actualFont(requireKey, type);
    mu::io::path_t path = fontInfo(key).path;
    if (!mu::io::File::exists(path)) {
        LOGE() << "not exists font: " << path;
        DO_ASSERT(mu::io::File::exists(path));
        return mu::io::path_t();
    }
    return path;
}

const FontsDatabase::FontInfo& FontsDatabase::fontInfo(const FontDataKey& key) const
{
    for (const FontInfo& fi : m_fonts) {
        if (fi.key == key) {
            return fi;
        }
    }

    static FontInfo null;
    return null;
}

void FontsDatabase::addAdditionalFonts(const mu::io::path_t& path)
{
    mu::io::File f(path + "/fontslist.json");
    if (!f.open(mu::io::IODevice::ReadOnly)) {
        LOGE() << "failed open file: " << f.filePath();
        return;
    }

    mu::io::path_t absolutePath = mu::io::Dir(path).absolutePath() + "/";

    mu::ByteArray data = f.readAll();
    std::string err;
    mu::JsonDocument json = mu::JsonDocument::fromJson(data, &err);
    if (!err.empty()) {
        LOGE() << "failed parse: " << f.filePath();
        return;
    }

    mu::JsonArray fontInfos = json.rootArray();
    for (size_t i = 0; i < fontInfos.size(); ++i) {
        mu::JsonObject infoObj = fontInfos.at(i).toObject();

        std::string file = infoObj.value("file").toStdString();
        if (file.empty()) {
            continue;
        }
        std::string family = infoObj.value("family").toStdString();
        if (family.empty()) {
            continue;
        }
        bool bold = infoObj.value("bold").toBool();
        bool italic = infoObj.value("italic").toBool();

        FontDataKey fontDataKey(family, bold, italic);
        addFont(fontDataKey, absolutePath + file);
        m_substitutions[Font::Type::Text].push_back(fontDataKey);
    }
}
