/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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

#include "onlinesoundsstatusmodel.h"

#include "async/async.h"
#include "translation.h"

using namespace mu::notation;
using namespace mu::playback;
using namespace muse::audio;

OnlineSoundsStatusModel::OnlineSoundsStatusModel(QObject* parent)
    : QObject(parent), muse::Injectable(muse::iocCtxForQmlObject(this))
{
}

void OnlineSoundsStatusModel::load()
{
    updateHasOnlineSounds();
    playbackController()->onlineSoundsChanged().onNotify(this, [this]() {
        updateHasOnlineSounds();
    });

    updateManualProcessingAllowed();

    globalContext()->currentMasterNotationChanged().onNotify(this, [this]() {
        updateManualProcessingAllowed();
    });

    audioConfiguration()->autoProcessOnlineSoundsInBackgroundChanged().onReceive(this, [this](bool) {
        updateManualProcessingAllowed();
    });

    muse::Progress progress = playbackController()->onlineSoundsProcessingProgress();
    setStatus(progress.isStarted() ? Status::Processing : Status::Success);

    progress.started().onNotify(this, [this]() {
        setStatus(Status::Processing);
    });

    progress.finished().onReceive(this, [this](const muse::ProgressResult& result) {
        setStatus(result.ret.success() ? Status::Success : Status::Error);

        if (result.ret.success() && m_manualProcessingAllowed) {
            setManualProcessingAllowed(false);
        }
    });
}

void OnlineSoundsStatusModel::processOnlineSounds()
{
    if (m_manualProcessingAllowed) {
        dispatcher()->dispatch("process-online-sounds");
    }
}

bool OnlineSoundsStatusModel::hasOnlineSounds() const
{
    return !m_onlineTrackIdSet.empty();
}

bool OnlineSoundsStatusModel::manualProcessingAllowed() const
{
    return m_manualProcessingAllowed;
}

int OnlineSoundsStatusModel::status() const
{
    return static_cast<int>(m_status);
}

QString OnlineSoundsStatusModel::errorTitle() const
{
    if (m_status != Status::Error) {
        return QString();
    }

    return muse::qtrc("playback", "Some online sounds aren’t ready yet");
}

QString OnlineSoundsStatusModel::errorDescription() const
{
    if (m_status != Status::Error) {
        return QString();
    }

    return muse::qtrc("global", "Please check your internet connection or try again later.");
}

void OnlineSoundsStatusModel::updateHasOnlineSounds()
{
    const std::map<TrackId, AudioResourceMeta>& onlineSounds = playbackController()->onlineSounds();
    const IPlaybackController::InstrumentTrackIdMap& instrumentTrackIdMap = playbackController()->instrumentTrackIdMap();

    const bool wasEmpty = m_onlineTrackIdSet.empty();
    m_onlineTrackIdSet.clear();
    m_onlineTrackIdSet.reserve(onlineSounds.size());

    for (const auto& pair : instrumentTrackIdMap) {
        if (muse::contains(onlineSounds, pair.second)) {
            m_onlineTrackIdSet.insert(pair.first);
        }
    }

    if (m_onlineTrackIdSet.empty() != wasEmpty) {
        emit hasOnlineSoundsChanged();
    }
}

void OnlineSoundsStatusModel::updateManualProcessingAllowed()
{
    if (audioConfiguration()->autoProcessOnlineSoundsInBackground()) {
        m_tracksDataChanged.resetOnReceive(this);
        setManualProcessingAllowed(false);
        return;
    }

    IMasterNotationPtr master = globalContext()->currentMasterNotation();
    if (!master) {
        m_tracksDataChanged.resetOnReceive(this);
        setManualProcessingAllowed(false);
        return;
    }

    m_tracksDataChanged = master->playback()->tracksDataChanged();
    m_tracksDataChanged.onReceive(this, [this](const InstrumentTrackIdSet& changedTrackIdSet) {
        for (const InstrumentTrackId& trackId : changedTrackIdSet) {
            if (muse::contains(m_onlineTrackIdSet, trackId)) {
                setManualProcessingAllowed(true);
                return;
            }
        }
    });
}

void OnlineSoundsStatusModel::setManualProcessingAllowed(bool allowed)
{
    if (m_manualProcessingAllowed == allowed) {
        return;
    }

    m_manualProcessingAllowed = allowed;
    emit manualProcessingAllowedChanged();

    if (allowed && m_shouldNotifyToursThatManualProcessingAllowed) {
        muse::async::Async::call(this, [=]() {
            tours()->onEvent(u"online_sounds_manual_processing_allowed");
        });

        m_shouldNotifyToursThatManualProcessingAllowed = false;
    }
}

void OnlineSoundsStatusModel::setStatus(Status status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}
