#include "sources/audiosourceopus.h"

namespace Mixxx {

namespace {

// Parameter for op_channel_count()
// See also: https://mf4.xiph.org/jenkins/view/opus/job/opusfile-unix/ws/doc/html/group__stream__info.html
const int kCurrentStreamLink = -1; // get ... of the current (stream) link

// Parameter for op_pcm_total() and op_bitrate()
// See also: https://mf4.xiph.org/jenkins/view/opus/job/opusfile-unix/ws/doc/html/group__stream__info.html
const int kEntireStreamLink  = -1; // get ... of the whole/entire stream

} // anonymous namespace

// Decoded output of opusfile has a fixed sample rate of 48 kHz
const AudioSource::size_type AudioSourceOpus::kFrameRate = 48000;

AudioSourceOpus::AudioSourceOpus(QUrl url)
        : AudioSource(url),
          m_pOggOpusFile(NULL),
          m_curFrameIndex(0) {
}

AudioSourceOpus::~AudioSourceOpus() {
    preDestroy();
}

AudioSourcePointer AudioSourceOpus::create(QUrl url) {
    return onCreate(new AudioSourceOpus(url));
}

Result AudioSourceOpus::postConstruct() {
    const QByteArray qbaFilename(getLocalFileNameBytes());
    int errorCode = 0;
    m_pOggOpusFile = op_open_file(qbaFilename.constData(), &errorCode);
    if (!m_pOggOpusFile) {
        qDebug() << "Failed to open OggOpus file:" << getUrl() << "errorCode"
                << errorCode;
        return ERR;
    }

    if (!op_seekable(m_pOggOpusFile)) {
        qWarning() << "OggOpus file is not seekable:" << getUrl();
        return ERR;
    }

    const int channelCount = op_channel_count(m_pOggOpusFile, kCurrentStreamLink);
    if (0 < channelCount) {
        setChannelCount(channelCount);
    } else {
        qWarning() << "Failed to read channel configuration of OggOpus file:" << getUrl();
        return ERR;
    }

    const ogg_int64_t pcmTotal = op_pcm_total(m_pOggOpusFile, kEntireStreamLink);
    if (0 <= pcmTotal) {
        setFrameCount(pcmTotal);
    } else {
        qWarning() << "Failed to read total length of OggOpus file:" << getUrl();
        return ERR;
    }

    const opus_int32 bitrate = op_bitrate(m_pOggOpusFile, kEntireStreamLink);
    if (0 < bitrate) {
        setBitrate(bitrate / 1000);
    } else {
        qWarning() << "Failed to determine bitrate of OggOpus file:" << getUrl();
        return ERR;
    }

    setFrameRate(kFrameRate);

    return OK;
}

void AudioSourceOpus::preDestroy() {
    if (m_pOggOpusFile) {
        op_free(m_pOggOpusFile);
        m_pOggOpusFile = NULL;
    }
}

AudioSource::diff_type AudioSourceOpus::seekSampleFrame(diff_type frameIndex) {
    DEBUG_ASSERT(isValidFrameIndex(m_curFrameIndex));
    DEBUG_ASSERT(isValidFrameIndex(frameIndex));

    int seekResult = op_pcm_seek(m_pOggOpusFile, frameIndex);
    if (0 == seekResult) {
        m_curFrameIndex = frameIndex;
    } else {
        qWarning() << "Failed to seek OggOpus file:" << seekResult;
        const ogg_int64_t pcmOffset = op_pcm_tell(m_pOggOpusFile);
        if (0 <= pcmOffset) {
            m_curFrameIndex = pcmOffset;
        } else {
            // Reset to EOF
            m_curFrameIndex = getFrameIndexMax();
        }
    }

    DEBUG_ASSERT(isValidFrameIndex(m_curFrameIndex));
    return m_curFrameIndex;
}

AudioSource::size_type AudioSourceOpus::readSampleFrames(
        size_type numberOfFrames, sample_type* sampleBuffer) {
    DEBUG_ASSERT(isValidFrameIndex(m_curFrameIndex));

    const size_type numberOfFramesTotal = math_min(numberOfFrames,
            size_type(getFrameIndexMax() - m_curFrameIndex));

    sample_type* pSampleBuffer = sampleBuffer;
    size_type numberOfFramesRemaining = numberOfFramesTotal;
    while (0 < numberOfFramesRemaining) {
        int readResult = op_read_float(m_pOggOpusFile,
                pSampleBuffer,
                frames2samples(numberOfFramesRemaining), NULL);
        if (0 < readResult) {
            m_curFrameIndex += readResult;
            pSampleBuffer += frames2samples(readResult);
            numberOfFramesRemaining -= readResult;
        } else {
            qWarning() << "Failed to read sample data from OggOpus file:"
                    << readResult;
            break; // abort
        }
    }

    DEBUG_ASSERT(isValidFrameIndex(m_curFrameIndex));
    DEBUG_ASSERT(numberOfFramesTotal >= numberOfFramesRemaining);
    return numberOfFramesTotal - numberOfFramesRemaining;
}

AudioSource::size_type AudioSourceOpus::readSampleFramesStereo(
        size_type numberOfFrames, sample_type* sampleBuffer,
        size_type sampleBufferSize) {
    DEBUG_ASSERT(isValidFrameIndex(m_curFrameIndex));
    DEBUG_ASSERT(getSampleBufferSize(numberOfFrames, true) <= sampleBufferSize);

    const size_type numberOfFramesTotal = math_min(numberOfFrames,
            size_type(getFrameIndexMax() - m_curFrameIndex));

    sample_type* pSampleBuffer = sampleBuffer;
    size_type numberOfFramesRemaining = numberOfFramesTotal;
    while (0 < numberOfFramesRemaining) {
        int readResult = op_read_float_stereo(m_pOggOpusFile,
                pSampleBuffer,
                numberOfFramesRemaining * 2); // stereo
        if (0 < readResult) {
            m_curFrameIndex += readResult;
            pSampleBuffer += readResult * 2; // stereo
            numberOfFramesRemaining -= readResult;
        } else {
            qWarning() << "Failed to read sample data from OggOpus file:"
                    << readResult;
            break; // abort
        }
    }

    DEBUG_ASSERT(isValidFrameIndex(m_curFrameIndex));
    DEBUG_ASSERT(numberOfFramesTotal >= numberOfFramesRemaining);
    return numberOfFramesTotal - numberOfFramesRemaining;
}

} // namespace Mixxx
