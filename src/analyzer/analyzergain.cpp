#include <replaygain.h>
#include <QtDebug>

#include "analyzer/analyzergain.h"
#include "track/track.h"
#include "util/math.h"
#include "util/sample.h"
#include "util/timer.h"

AnalyzerGain::AnalyzerGain(UserSettingsPointer pConfig)
        : m_rgSettings(pConfig),
          m_iBufferSize(0) {
    m_pReplayGain = new ReplayGain();
}

AnalyzerGain::~AnalyzerGain() {
    delete m_pReplayGain;
}

bool AnalyzerGain::initialize(TrackPointer tio,
        mixxx::audio::SampleRate sampleRate,
        int totalSamples) {
    if (m_rgSettings.isAnalyzerDisabled(1, tio) || totalSamples == 0) {
        qDebug() << "Skipping AnalyzerGain";
        return false;
    }

    return m_pReplayGain->initialise(static_cast<long>(sampleRate), 2);
}

void AnalyzerGain::cleanup() {
}

bool AnalyzerGain::processSamples(const CSAMPLE *pIn, const int iLen) {
    ScopedTimer t("AnalyzerGain::process()");

    int halfLength = static_cast<int>(iLen / 2);
    if (halfLength > m_iBufferSize) {
        m_pLeftTempBuffer.resize(halfLength);
        m_pRightTempBuffer.resize(halfLength);
    }
    SampleUtil::deinterleaveBuffer(m_pLeftTempBuffer.data(),
            m_pRightTempBuffer.data(),
            pIn,
            halfLength);
    SampleUtil::applyGain(m_pLeftTempBuffer.data(), 32767, halfLength);
    SampleUtil::applyGain(m_pRightTempBuffer.data(), 32767, halfLength);
    return m_pReplayGain->process(m_pLeftTempBuffer.data(), m_pRightTempBuffer.data(), halfLength);
}

void AnalyzerGain::storeResults(TrackPointer tio) {
    //TODO: We are going to store values as relative peaks so that "0" means that no replaygain has been evaluated.
    // This means that we are going to transform from dB to peaks and vice-versa.
    // One may think to digg into replay_gain code and modify it so that
    // it directly sends results as relative peaks.
    // In that way there is no need to spend resources in calculating log10 or pow.

    float fReplayGainOutput = m_pReplayGain->end();
    if (fReplayGainOutput == GAIN_NOT_ENOUGH_SAMPLES) {
        qWarning() << "ReplayGain 1.0 analysis failed";
        return;
    }

    mixxx::ReplayGain replayGain(tio->getReplayGain());
    replayGain.setRatio(db2ratio(fReplayGainOutput));
    tio->setReplayGain(replayGain);
    qDebug() << "ReplayGain 1.0 result is" << fReplayGainOutput << "dB for" << tio->getLocation();
}
