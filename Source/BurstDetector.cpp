/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2022 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "BurstDetector.h"

#include "BurstDetectorEditor.h"

#include <algorithm>
#include <cmath>

BurstDetector::BurstDetector()
    : GenericProcessor("Burst Detector")
{
}

BurstDetector::~BurstDetector()
{
}

AudioProcessorEditor* BurstDetector::createEditor()
{
    editor = std::make_unique<BurstDetectorEditor>(this);
    return editor.get();
}

void BurstDetector::registerParameters()
{
    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "event_duration",
                    "TTL Duration",
                    "Width of the generated TTL pulse",
                    10,
                    1,
                    2000);

    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "timeout",
                    "Timeout",
                    "Minimum time between TTL pulses",
                    100,
                    0,
                    10000);

    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "min_electrodes",
                    "Min Electrodes",
                    "Minimum number of overlapping electrode bursts required for a network burst",
                    3,
                    1,
                    1024);

    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "max_isi_start",
                    "Max ISI Start",
                    "Maximum interval between the first two spikes of a burst",
                    100,
                    1,
                    10000);

    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "max_isi_end",
                    "Max ISI End",
                    "Maximum interval allowed inside an active burst candidate",
                    200,
                    1,
                    10000);

    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "min_duration",
                    "Min Duration",
                    "Minimum duration required before a burst is reported",
                    20,
                    0,
                    10000);

    addIntParameter(Parameter::PROCESSOR_SCOPE,
                    "min_spikes",
                    "Min Spikes",
                    "Minimum number of spikes required before a burst is reported",
                    3,
                    2,
                    10000);

    addTtlLineParameter(Parameter::STREAM_SCOPE,
                        "single_burst_line",
                        "Single Line",
                        "TTL line used for single-electrode bursts",
                        8,
                        false,
                        false,
                        false);

    addTtlLineParameter(Parameter::STREAM_SCOPE,
                        "network_burst_line",
                        "Network Line",
                        "TTL line used for network bursts",
                        8,
                        false,
                        false,
                        false);
}

void BurstDetector::updateSettings()
{
    ttlChannels.clear();

    for (auto stream : getDataStreams())
    {
        EventChannel::Settings settings{
            EventChannel::Type::TTL,
            "Burst Detector output",
            "TTL pulses generated when single-electrode or network bursts are detected",
            "burst_detector.events",
            getDataStream(stream->getStreamId())
        };

        eventChannels.add(new EventChannel(settings));
        eventChannels.getLast()->addProcessor(this);
        ttlChannels[stream->getStreamId()] = eventChannels.getLast();
    }
}

bool BurstDetector::startAcquisition()
{
    electrodeStates.clear();
    recentBursts.clear();
    pendingTtlOffs.clear();
    lastSingleTtlSample.clear();
    lastNetworkTtlSample.clear();

    return true;
}

void BurstDetector::process(AudioBuffer<float>& buffer)
{
    ignoreUnused(buffer);

    // First close any pulses that were scheduled by earlier blocks. New spikes
    // are then handled by the GUI's event dispatcher.
    emitPendingTtlOffs();
    checkForEvents(true);
}

void BurstDetector::parameterValueChanged(Parameter* param)
{
    const String name = param->getName();

    if (name == "event_duration")
        eventDurationMs = int(param->getValue());
    else if (name == "timeout")
        timeoutMs = int(param->getValue());
    else if (name == "min_electrodes")
        minElectrodes = jmax(1, int(param->getValue()));
    else if (name == "max_isi_start")
        maxIsiStartMs = int(param->getValue());
    else if (name == "max_isi_end")
        maxIsiEndMs = int(param->getValue());
    else if (name == "min_duration")
        minDurationMs = int(param->getValue());
    else if (name == "min_spikes")
        minSpikes = jmax(2, int(param->getValue()));
    else if (name == "single_burst_line")
        singleBurstLine = static_cast<TtlLineParameter*>(param)->getSelectedLine();
    else if (name == "network_burst_line")
        networkBurstLine = static_cast<TtlLineParameter*>(param)->getSelectedLine();
}

void BurstDetector::handleTTLEvent(TTLEventPtr event)
{
    ignoreUnused(event);
}

void BurstDetector::handleSpike(SpikePtr spike)
{
    const SpikeChannel* spikeChannel = spike->getChannelInfo();

    if (spikeChannel == nullptr)
        return;

    const ElectrodeKey electrode{
        spike->getStreamId(),
        spike->getProcessorId(),
        spike->getChannelIndex()
    };

    ElectrodeState& state = electrodeStates[electrode];
    const int64 spikeSample = spike->getSampleNumber();

    if (state.spikeCount == 0)
    {
        state.spikeCount = 1;
        state.firstSpikeSample = spikeSample;
        state.lastSpikeSample = spikeSample;
        return;
    }

    const int64 isiSamples = spikeSample - state.lastSpikeSample;
    const int64 maxStartSamples = msToSamples(electrode.streamId, maxIsiStartMs);
    const int64 maxEndSamples = msToSamples(electrode.streamId, maxIsiEndMs);

    if (! state.inCandidate)
    {
        // Two close spikes are the seed for a max-interval burst candidate.
        if (isiSamples >= 0 && isiSamples <= maxStartSamples)
        {
            state.inCandidate = true;
            state.emittedForCandidate = false;
            state.spikeCount = 2;
        }
        else
        {
            state.spikeCount = 1;
            state.firstSpikeSample = spikeSample;
        }

        state.lastSpikeSample = spikeSample;
    }
    else if (isiSamples >= 0 && isiSamples <= maxEndSamples)
    {
        state.spikeCount++;
        state.lastSpikeSample = spikeSample;
    }
    else
    {
        // The candidate has ended. If it was never reported, validate it one
        // final time before starting over from the current spike.
        if (! state.emittedForCandidate
            && state.spikeCount >= minSpikes
            && (state.lastSpikeSample - state.firstSpikeSample) >= msToSamples(electrode.streamId, minDurationMs))
        {
            emitSingleElectrodeBurst(electrode, state, state.lastSpikeSample);
        }

        state.inCandidate = false;
        state.emittedForCandidate = false;
        state.spikeCount = 1;
        state.firstSpikeSample = spikeSample;
        state.lastSpikeSample = spikeSample;
        return;
    }

    // Report as soon as the candidate satisfies the minimum criteria. This
    // keeps closed-loop TTL output earlier than waiting for the burst-ending ISI.
    if (state.inCandidate
        && ! state.emittedForCandidate
        && state.spikeCount >= minSpikes
        && (state.lastSpikeSample - state.firstSpikeSample) >= msToSamples(electrode.streamId, minDurationMs))
    {
        state.emittedForCandidate = true;
        emitSingleElectrodeBurst(electrode, state, spikeSample);
    }
}

void BurstDetector::handleBroadcastMessage(const String& msg, const int64 messageTimeMilliseconds)
{
    ignoreUnused(msg, messageTimeMilliseconds);
}

void BurstDetector::saveCustomParametersToXml(XmlElement* parentElement)
{
    ignoreUnused(parentElement);
}

void BurstDetector::loadCustomParametersFromXml(XmlElement* parentElement)
{
    ignoreUnused(parentElement);
}

int64 BurstDetector::msToSamples(uint16 streamId, int milliseconds) const
{
    const DataStream* stream = getDataStream(streamId);
    const double sampleRate = stream != nullptr ? stream->getSampleRate() : getDefaultSampleRate();

    return int64(std::llround((double(milliseconds) / 1000.0) * sampleRate));
}

EventChannel* BurstDetector::getTtlChannel(uint16 streamId) const
{
    const auto iter = ttlChannels.find(streamId);

    if (iter == ttlChannels.end())
        return nullptr;

    return iter->second;
}

void BurstDetector::triggerTtlPulse(uint16 streamId, int64 sampleNumber, uint8 line)
{
    addTtlEvent(streamId, sampleNumber, line, true);

    const int64 offSample = sampleNumber + msToSamples(streamId, eventDurationMs);

    if (offSample <= sampleNumber)
        addTtlEvent(streamId, sampleNumber, line, false);
    else
        pendingTtlOffs.push_back({ streamId, offSample, line });
}

void BurstDetector::emitPendingTtlOffs()
{
    auto pending = pendingTtlOffs;
    pendingTtlOffs.clear();

    for (const auto& off : pending)
    {
        const int64 firstSample = getFirstSampleNumberForBlock(off.streamId);
        const int64 lastSample = firstSample + int64(getNumSamplesInBlock(off.streamId)) - 1;

        if (off.sampleNumber >= firstSample && off.sampleNumber <= lastSample)
            addTtlEvent(off.streamId, off.sampleNumber, off.line, false);
        else
            pendingTtlOffs.push_back(off);
    }
}

void BurstDetector::addDetectedBurst(const DetectedBurst& burst, int64 triggerSample)
{
    std::deque<DetectedBurst>& streamBursts = recentBursts[burst.electrode.streamId];
    streamBursts.push_back(burst);

    pruneOldBursts(burst.electrode.streamId, burst.endSample);

    // Count one overlapping interval per electrode. The network burst starts
    // where the shared overlap starts and ends where that overlap ends.
    std::map<ElectrodeKey, DetectedBurst> overlappingByElectrode;

    for (const auto& candidate : streamBursts)
    {
        const int64 overlapStart = jmax(burst.startSample, candidate.startSample);
        const int64 overlapEnd = jmin(burst.endSample, candidate.endSample);

        if (overlapStart <= overlapEnd)
            overlappingByElectrode[candidate.electrode] = candidate;
    }

    if (int(overlappingByElectrode.size()) < minElectrodes)
        return;

    int64 networkStart = burst.startSample;
    int64 networkEnd = burst.endSample;

    for (const auto& electrodeAndBurst : overlappingByElectrode)
    {
        networkStart = jmax(networkStart, electrodeAndBurst.second.startSample);
        networkEnd = jmin(networkEnd, electrodeAndBurst.second.endSample);
    }

    if (networkStart > networkEnd)
        return;

    const int64 timeoutSamples = msToSamples(burst.electrode.streamId, timeoutMs);
    const auto lastIter = lastNetworkTtlSample.find(burst.electrode.streamId);

    if (lastIter != lastNetworkTtlSample.end()
        && triggerSample - lastIter->second < timeoutSamples)
    {
        return;
    }

    triggerTtlPulse(burst.electrode.streamId, triggerSample, uint8(networkBurstLine));
    lastNetworkTtlSample[burst.electrode.streamId] = triggerSample;
}

void BurstDetector::pruneOldBursts(uint16 streamId, int64 newestSample)
{
    std::deque<DetectedBurst>& streamBursts = recentBursts[streamId];
    const int64 keepWindowSamples = msToSamples(streamId, jmax(maxIsiEndMs, minDurationMs) + timeoutMs + eventDurationMs);

    while (! streamBursts.empty() && streamBursts.front().endSample + keepWindowSamples < newestSample)
        streamBursts.pop_front();
}

void BurstDetector::emitSingleElectrodeBurst(const ElectrodeKey& electrode,
                                             const ElectrodeState& state,
                                             int64 triggerSample)
{
    const int64 timeoutSamples = msToSamples(electrode.streamId, timeoutMs);
    const auto lastIter = lastSingleTtlSample.find(electrode.streamId);

    if (lastIter == lastSingleTtlSample.end()
        || triggerSample - lastIter->second >= timeoutSamples)
    {
        triggerTtlPulse(electrode.streamId, triggerSample, uint8(singleBurstLine));
        lastSingleTtlSample[electrode.streamId] = triggerSample;
    }

    addDetectedBurst({ electrode, state.firstSpikeSample, state.lastSpikeSample }, triggerSample);
}

void BurstDetector::addTtlEvent(uint16 streamId, int64 sampleNumber, uint8 line, bool state)
{
    EventChannel* channel = getTtlChannel(streamId);

    if (channel == nullptr)
        return;

    const int64 firstSample = getFirstSampleNumberForBlock(streamId);
    const int64 numSamples = int64(getNumSamplesInBlock(streamId));

    if (numSamples <= 0)
        return;

    const int64 lastSample = firstSample + numSamples - 1;
    const int sampleOffset = int(jlimit<int64>(0, numSamples - 1, sampleNumber - firstSample));
    const int64 eventSample = jlimit<int64>(firstSample, lastSample, sampleNumber);

    TTLEventPtr event = TTLEvent::createTTLEvent(channel, eventSample, line, state);
    addEvent(event, sampleOffset);
}
