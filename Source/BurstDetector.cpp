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
    addIntParameter(Parameter::PROCESSOR_SCOPE, "event_duration", "TTL Duration", "Width of the generated TTL pulse", 10, 1, 2000);


    addIntParameter(Parameter::PROCESSOR_SCOPE, "timeout", "Timeout", "Minimum time between TTL pulses (burst detections)", 200, 1, 10000);


    addIntParameter(Parameter::PROCESSOR_SCOPE, "min_electrodes", "Min Electrodes", "Minimum number of overlapping electrode bursts required for a network burst", 3, 1, 1024);


    addIntParameter(Parameter::PROCESSOR_SCOPE, "max_isi_start", "Max ISI Start", "Maximum interval between the first two spikes of a burst", 30, 1, 1000);


    addIntParameter(Parameter::PROCESSOR_SCOPE, "max_isi_end", "Max ISI End", "Maximum interval allowed inside an active burst candidate", 50, 1, 1000);


    addIntParameter(Parameter::PROCESSOR_SCOPE, "min_duration", "Min Duration", "Minimum duration required before a burst is reported", 10, 0, 100);


    addIntParameter(Parameter::PROCESSOR_SCOPE, "min_spikes", "Min Spikes", "Minimum number of spikes required before a burst is reported", 3, 2, 50);  


    addBooleanParameter(Parameter::STREAM_SCOPE, "single_burst_enabled", "Single TTL", "Turns on TTL output for single-electrode bursts. Off by default; the line selector below always shows a line pre-selected, but no TTL is sent until this is enabled.", false, false);


    addTtlLineParameter(Parameter::STREAM_SCOPE, "single_burst_line", "Single Line", "TTL line used for single-electrode bursts", 8, false, true, false);


    addTtlLineParameter(Parameter::STREAM_SCOPE, "network_burst_line", "Network Line", "TTL line used for network bursts", 8, false, true, false);
}


   

void BurstDetector::updateSettings()
{
    // Rebuild processor state whenever the available data streams change.
    // Remove previously created TTL and spike channels.
    ttlChannels.clear();
    spikeChannels.clear();

    // Iterate over every input data stream.
    for (auto stream : getDataStreams())
    {
        // Create one TTL event channel for this stream.
        // This channel will be used to output burst-detection TTL pulses.
        EventChannel::Settings settings{
            EventChannel::Type::TTL,
            "Burst Detector output",
            "TTL pulses generated when single-electrode or network bursts are detected",
            "burst_detector.events",
            getDataStream(stream->getStreamId())
        };

        eventChannels.add(new EventChannel(settings));
        eventChannels.getLast()->addProcessor(this);

        // Store the channel so it can later be retrieved using the stream ID.
        ttlChannels[stream->getStreamId()] = eventChannels.getLast();

        // Make sure a burst deque exists for this stream, 
        // so the audio-thread path in addDetectedBurst() never needs
        // to insert a new map entry the first time a burst is reported.
        recentBursts[stream->getStreamId()];

        // Register every spike channel belonging to this stream.
        // Initialise newly discovered channels as non-active by default.
        for (auto chan : stream->getSpikeChannels())
        {
            spikeChannels.add(chan);

            String id = chan->getIdentifier();

            if (spikeChannelActive.find(id) == spikeChannelActive.end())
                spikeChannelActive[id] = false;
        }
    }
}

bool BurstDetector::startAcquisition()
{
    electrodeStates.clear();
    recentBursts.clear();
    pendingTtlOffs.clear();
    lastSingleTtlSample.clear();
    lastNetworkTtlSample.clear();
    networkBurstActive.clear();
    lastObservedFirstSample.clear();

    return true;
}

void BurstDetector::process(AudioBuffer<float>& buffer)
{
    ignoreUnused(buffer);

    // If a stream's sample numbers have gone backwards since the last block
    // (e.g. a looping File Reader source restarting playback), any TTL offs
    // pending for that stream would stay up. 
    // Make sure they are fired immediately and the stream's debounce/candidate state is reset.
    for (auto stream : getDataStreams())
    {
        const uint16 streamId = stream->getStreamId();
        const int64 firstSample = getFirstSampleNumberForBlock(streamId); // get the first sample number for the current block of this stream

        const auto it = lastObservedFirstSample.find(streamId); // get the last observed first sample for this stream
        if (it != lastObservedFirstSample.end() && firstSample < it->second) // if the current first sample is less than the last observed first sample, the file has looped or restarted
            resetStreamTimingState(streamId);

        lastObservedFirstSample[streamId] = firstSample;
    }

    // Close any pulses that were scheduled by earlier blocks. New spikes
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
        minElectrodes = int(param->getValue());
    else if (name == "max_isi_start")
        maxIsiStartMs = int(param->getValue());
    else if (name == "max_isi_end")
        maxIsiEndMs = int(param->getValue());
    else if (name == "min_duration")
        minDurationMs = int(param->getValue());
    else if (name == "min_spikes")
        minSpikes = jmax(2, int(param->getValue()));
    else if (name == "single_burst_enabled")
        singleBurstEnabled = bool(param->getValue());
    else if (name == "single_burst_line")
        singleBurstLine = static_cast<TtlLineParameter*>(param)->getSelectedLine();
    else if (name == "network_burst_line")
        networkBurstLine = static_cast<TtlLineParameter*>(param)->getSelectedLine();

}

// Returns whether a particular spike channel is currently active for burst detection.
bool BurstDetector::isActive(const SpikeChannel* chan) const
{
    auto it = spikeChannelActive.find(chan->getIdentifier());

    if (it == spikeChannelActive.end())
        return false;

    return it->second;
}

// Sets whether a particular spike channel is currently active for burst detection.
void BurstDetector::setActive(const String& identifier, bool active)
{
    spikeChannelActive[identifier] = active;
    BD_LOG("Set ", identifier, " to ", (active ? "active" : "inactive"));
}

// Returns the number of currently active electrodes for burst detection
int BurstDetector::getNumActiveElectrodes() const
{
    int count = 0;

    for (auto chan : spikeChannels)
    {
        if (isActive(chan))
            ++count;
    }

    return count;
}


void BurstDetector::handleTTLEvent(TTLEventPtr event)
{
    ignoreUnused(event);
}

// Burst detection in the audio thread, triggered by incoming spikes.
// This is the main detection logic for single-electrode bursts, which are
// then forwarded to the network-burst detector (addDetectedBurst()) if they satisfy the minimum criteria.
void BurstDetector::handleSpike(SpikePtr spike)
{

    const SpikeChannel* spikeChannel = spike->getChannelInfo(); // get the spike channel for this spike

    if (spikeChannel == nullptr)
        return;
    
    // Ignore spikes from channels that are not currently active for burst detection.
    if (!isActive(const_cast<SpikeChannel*>(spikeChannel)))
    return;

    // Create a unique key for this electrode based on its stream ID, processor ID, and channel index. Used to look up the electrode's state in the electrodeStates map.
    const ElectrodeKey electrode{
        spike->getStreamId(),
        spike->getProcessorId(),
        spike->getChannelIndex()
    };


    // Get the current state for this electrode, or create a new one if it doesn't exist yet.
    ElectrodeState& state = electrodeStates[electrode];
    const int64 spikeSample = spike->getSampleNumber();

    BD_LOG(
        "Spike detected: Stream: ", spike->getStreamId(),
        "  Channel: ", spikeChannel->getName(),
        "  Sample: ", spikeSample,
        "  Spike count: ", state.spikeCount,
        "  First spike: ", state.firstSpikeSample,
        "  Last spike: ", state.lastSpikeSample,
        "  In candidate: ", state.inCandidate,
        "  Emitted: ", state.emittedForCandidate
    );

    // If this is the first spike for this electrode, initialise the state and return.
    if (state.spikeCount == 0)
    {
        state.spikeCount = 1;
        state.firstSpikeSample = spikeSample;
        state.lastSpikeSample = spikeSample;
        return;
    }

    // Convert ms-based parameters to samples for this stream, and compute the ISI between the current spike and the last one.
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
    // If the ISI is within the max-interval range, continue accumulating spikes for this candidate.
    else if (isiSamples >= 0 && isiSamples <= maxEndSamples)
    {
        state.spikeCount++;
        state.lastSpikeSample = spikeSample;
    }
    // If the ISI is too long, the candidate has ended. Validate it and start a new candidate from the current spike.
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
    else if (state.inCandidate && state.emittedForCandidate)
    {
        //Refresh the recorded interval so network-burst overlap checks are computed against the burst's true extent rather than the truncated interval captured at the moment of early emission.
        addDetectedBurst({ electrode, state.firstSpikeSample, state.lastSpikeSample }, spikeSample);
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

// Resets timing-dependent state for one stream after its sample
// numbers have jumped backwards (see lastObservedFirstSample). Fires
// any pending TTL offs for that stream immediately instead of leaving
// them stranded, and clears debounce/candidate state so detection  
// starts cleanly for the new pass over the data.
void BurstDetector::resetStreamTimingState(uint16 streamId)
{
    BD_LOG("Stream ", streamId, " sample number went backwards (source likely looped or restarted) "
         "- resetting burst detector timing state for this stream");

    // Fire any pending TTL offs for this stream right now
    const int64 firstSample = getFirstSampleNumberForBlock(streamId);

    std::vector<PendingTtlOff> remaining;
    remaining.reserve(pendingTtlOffs.size());

    for (const auto& off : pendingTtlOffs)
    {
        if (off.streamId == streamId)
            addTtlEvent(off.streamId, firstSample, off.line, false);
        else
            remaining.push_back(off);
    }

    pendingTtlOffs = std::move(remaining);

    // Clear debounce/episode state so the new pass over the data starts
    // clean instead of comparing against sample numbers from before the jump.
    lastSingleTtlSample.erase(streamId);
    lastNetworkTtlSample.erase(streamId);
    networkBurstActive.erase(streamId);
    recentBursts[streamId].clear();

    for (auto it = electrodeStates.begin(); it != electrodeStates.end(); )
    {
        if (it->first.streamId == streamId)
            it = electrodeStates.erase(it);
        else
            ++it;
    }
}

// Safely emits any pending TTL offs that were scheduled by earlier blocks.
void BurstDetector::emitPendingTtlOffs()
{
    if (pendingTtlOffs.empty())
        return;

    auto pending = std::move(pendingTtlOffs);
    pendingTtlOffs.clear();

    for (const auto& off : pending)
    {
        const int64 firstSample = getFirstSampleNumberForBlock(off.streamId);
        const int64 numSamples = int64(getNumSamplesInBlock(off.streamId));

        if (numSamples <= 0)
        {
            // Stream isn't producing samples this block; keep waiting.
            pendingTtlOffs.push_back(off);
            continue;
        }

        const int64 lastSample = firstSample + numSamples - 1;

        if (off.sampleNumber < firstSample)
        {
            // We missed the exact target sample. 
            // Fire the falling edge as early as possible in this block
            addTtlEvent(off.streamId, firstSample, off.line, false);
        }
        else if (off.sampleNumber <= lastSample)
        {
            addTtlEvent(off.streamId, off.sampleNumber, off.line, false);
        }
        else
        {
            // Still in the future - keep waiting.
            pendingTtlOffs.push_back(off);
        }
    }
}

// Adds a new single-electrode burst to the list of recent bursts for this stream,
// then checks whether enough electrodes are currently overlapping to trigger a network burst.
void BurstDetector::addDetectedBurst(const DetectedBurst& burst, int64 triggerSample)
{
    std::deque<DetectedBurst>& streamBursts = recentBursts[burst.electrode.streamId];
    
    // 1. Look to see if this electrode already has an unpruned, active burst entry
    auto ongoingBurstIt = std::find_if(streamBursts.rbegin(), streamBursts.rend(),
        [&](const DetectedBurst& b) { 
            return b.electrode == burst.electrode && b.endSample >= burst.startSample; 
        });

    // We need a pointer to the true, updated burst data for the network calculations below
    const DetectedBurst* activeBurstPtr = nullptr;

    if (ongoingBurstIt != streamBursts.rend())
    {
        // Update in-place to keep memory usage tiny and O(1) stable
        ongoingBurstIt->endSample = burst.endSample;
        activeBurstPtr = &(*ongoingBurstIt);
    }
    else
    {
        // Push a genuinely new distinct burst sequence
        streamBursts.push_back(burst);
        activeBurstPtr = &(streamBursts.back());
    }

    // Now prune the history window safely
    pruneOldBursts(burst.electrode.streamId, activeBurstPtr->endSample);
    
    // Clear our pre-allocated scratch buffer (zero audio-thread allocations)
    overlapScratch.clear();

    // 2. Compute overlaps using the ground-truth updated burst
    for (const auto& candidate : streamBursts)
    {
        const int64 overlapStart = jmax(activeBurstPtr->startSample, candidate.startSample);
        const int64 overlapEnd = jmin(activeBurstPtr->endSample, candidate.endSample);

        if (overlapStart > overlapEnd)
            continue;

        // Keep only the most recent overlapping burst per electrode
        auto existing = std::find_if(overlapScratch.begin(), overlapScratch.end(),
            [&](const auto& entry) { return entry.electrode == candidate.electrode; });

        if (existing == overlapScratch.end())
            overlapScratch.push_back({ candidate.electrode, candidate });
        else
            existing->burst = candidate;
    }

    // 3. Evaluate threshold metrics
    if (int(overlapScratch.size()) < minElectrodes)
    {
        networkBurstActive[burst.electrode.streamId] = false;
        return;
    }

    if (networkBurstActive[burst.electrode.streamId])
    {
        return; // Already in a network burst episode, don't double-trigger TTL
    }

    // 4. Calculate definitive Network Boundaries using the true active burst
    int64 networkStart = activeBurstPtr->startSample;
    int64 networkEnd = activeBurstPtr->endSample;

    for (const auto& electrodeAndBurst : overlapScratch)
    {
        networkStart = jmax(networkStart, electrodeAndBurst.burst.startSample);
        networkEnd = jmin(networkEnd, electrodeAndBurst.burst.endSample);
    }

    if (networkStart > networkEnd)
        return;

    // 5. Enforce debounce timeout checks
    const int64 timeoutSamples = msToSamples(burst.electrode.streamId, timeoutMs);
    const auto lastIter = lastNetworkTtlSample.find(burst.electrode.streamId);

    if (lastIter != lastNetworkTtlSample.end()
        && triggerSample - lastIter->second < timeoutSamples)
    {
        return;
    }

    BD_LOG("Network burst detected on stream ", burst.electrode.streamId,
          ": ", int(overlapScratch.size()), " electrodes overlapping (threshold ", minElectrodes,
          "), TTL at sample ", triggerSample);

    networkBurstActive[burst.electrode.streamId] = true;
    triggerTtlPulse(burst.electrode.streamId, triggerSample, uint8(networkBurstLine));
    lastNetworkTtlSample[burst.electrode.streamId] = triggerSample;
}

// Removes any bursts from the recent-bursts list that are now too old to contribute to a network burst.
void BurstDetector::pruneOldBursts(uint16 streamId, int64 newestSample)
{
    std::deque<DetectedBurst>& streamBursts = recentBursts[streamId];
    const int64 keepWindowSamples = msToSamples(streamId, jmax(maxIsiEndMs, minDurationMs) + timeoutMs + eventDurationMs);

    // Remove bursts whose end sample is older than the newest sample minus the keep window.
    while (! streamBursts.empty() && streamBursts.front().endSample + keepWindowSamples < newestSample)
        streamBursts.pop_front();
}

// Emits a single-electrode burst event and its TTL output if enabled.
void BurstDetector::emitSingleElectrodeBurst(const ElectrodeKey& electrode,
                                             const ElectrodeState& state,
                                             int64 triggerSample)
{
    BD_LOG("Single-electrode burst detected: stream ", electrode.streamId,
         ", processor ", electrode.processorId, ", channel ", electrode.channelIndex,
         ", spikes ", state.spikeCount, ", sample ", triggerSample);

    // Single-electrode TTL output is off by default: it requires the
    // explicit "single_burst_enabled" toggle (see registerParameters()),
    // since the TTL line selector itself always shows some line
    // pre-selected and can't reliably default to "off" on its own.
    // Detection and the network-burst feed below still run either way.
    if (singleBurstEnabled && singleBurstLine >= 0)
    {
        const int64 timeoutSamples = msToSamples(electrode.streamId, timeoutMs);
        const auto lastIter = lastSingleTtlSample.find(electrode.streamId);

        if (lastIter == lastSingleTtlSample.end()
            || triggerSample - lastIter->second >= timeoutSamples)
        {
            triggerTtlPulse(electrode.streamId, triggerSample, uint8(singleBurstLine));
            lastSingleTtlSample[electrode.streamId] = triggerSample;
        }
    }

    addDetectedBurst({ electrode, state.firstSpikeSample, state.lastSpikeSample }, triggerSample);
}

// Adds a TTL event to the appropriate channel for the given stream, at the given sample number and line, with the given state (high or low).
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