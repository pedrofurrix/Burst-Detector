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

/*
Burst Detector Plugin was developed by:
Pedro Félix Alves (pedrofalves@i3s.up.pt)
Paulo Aguiar
Neuroengineering and Computational Neuroscience Lab
i3S - Institute for Research and Innovation in Health
University of Porto, Portugal
Contact email: pauloaguiar@i3s.up.pt
*/

#pragma once

#ifndef BURSTDETECTOR_H_DEFINED
#define BURSTDETECTOR_H_DEFINED


// Enable (1) or disable (0) debug logging
#define BURST_DETECTOR_DEBUG 1

#if BURST_DETECTOR_DEBUG
    #define BD_LOG(...) LOGD(__VA_ARGS__)
#else
    #define BD_LOG(...)
#endif


#include <ProcessorHeaders.h>

#include <atomic>
#include <deque>
#include <map>
#include <vector>




/**
    Detects single-electrode and network bursts from upstream Spike events.

    The single-electrode detector follows the common max-interval burst rule:
    a burst candidate starts when two consecutive spikes are closer than
    max_isi_start, continues while consecutive spikes remain closer than
    max_isi_end, and becomes a detected burst after it satisfies min_spikes
    and min_duration.
    (For a further description of the method, please check https://doi.org/10.1152/jn.00093.2016)

    Network bursts are detected by looking for temporal overlap between
    detected single-electrode bursts on at least min_electrodes different
    spike channels.
*/
class BurstDetector : public GenericProcessor
{
public:
    /** Constructor */
    BurstDetector();

    /** Destructor */
    ~BurstDetector();

    /** Creates this processor's compact parameter editor. */
    AudioProcessorEditor* createEditor() override;

    /** Creates parameters before the editor asks for them. */
    void registerParameters() override;

    /** Adds the TTL output event channel for each incoming data stream. */
    void updateSettings() override;

    /** Resets all detection state when acquisition starts. */
    bool startAcquisition() override;

    /** Handles incoming Spike events and pending TTL pulse ends. */
    void process(AudioBuffer<float>& buffer) override;

    /** Responds to parameter edits from the GUI. */
    void parameterValueChanged(Parameter* param) override;

    /** Incoming TTL events are not used by this detector. */
    void handleTTLEvent(TTLEventPtr event) override;

    /** Runs max-interval detection for each spike. */
    void handleSpike(SpikePtr spike) override;

    /** Handles broadcast messages sent during acquisition. */
    void handleBroadcastMessage(const String& message, const int64 messageTimeMilliseconds) override;

    /** Persists the active-electrode selection (not a Parameter object). */
    void saveCustomParametersToXml(XmlElement* parentElement) override;

    /** Restores the active-electrode selection saved by saveCustomParametersToXml(). */
    void loadCustomParametersFromXml(XmlElement* parentElement) override;

    bool isActive(const SpikeChannel* chan) const;

    void setActive(const String& identifier, bool active);

    int getNumActiveElectrodes() const;

    Array<SpikeChannel*> spikeChannels; // stores pointers to all spike channels in the system
    std::map<String, bool> spikeChannelActive; //stores the active state of each spike channel in the system

private:
    struct ElectrodeKey
    {
        uint16 streamId = 0;
        uint16 processorId = 0;
        uint16 channelIndex = 0;

        bool operator<(const ElectrodeKey& other) const
        {
            if (streamId != other.streamId)
                return streamId < other.streamId;

            if (processorId != other.processorId)
                return processorId < other.processorId;

            return channelIndex < other.channelIndex;
        }

        bool operator==(const ElectrodeKey& other) const
        {
            return streamId == other.streamId
                && processorId == other.processorId
                && channelIndex == other.channelIndex;
        }
    };

    struct ElectrodeState
    {
        bool inCandidate = false;
        bool emittedForCandidate = false;
        int spikeCount = 0;
        int64 firstSpikeSample = 0;
        int64 lastSpikeSample = 0;
    };

    struct DetectedBurst
    {
        ElectrodeKey electrode;
        int64 startSample = 0;
        int64 endSample = 0;
    };

    struct PendingTtlOff
    {
        uint16 streamId = 0;
        int64 sampleNumber = 0;
        uint8 line = 0;
    };
    struct OverlapEntry
    {
        ElectrodeKey electrode;
        DetectedBurst burst;
    };

    /** Converts a millisecond parameter to samples for a specific stream. */
    int64 msToSamples(uint16 streamId, int milliseconds) const;

    /** Returns the generated TTL channel for a stream, or nullptr if unavailable. */
    EventChannel* getTtlChannel(uint16 streamId) const;

    /** Emits a TTL pulse and schedules the matching falling edge. */
    void triggerTtlPulse(uint16 streamId, int64 sampleNumber, uint8 line);

    /** Emits pending falling edges whose timestamps fall in the current block. */
    void emitPendingTtlOffs();

    /** Adds (or refreshes) a detected electrode burst and checks whether it creates a network burst. */
    void addDetectedBurst(const DetectedBurst& burst, int64 triggerSample);

    /** Prunes old burst intervals so network overlap checks stay bounded. */
    void pruneOldBursts(uint16 streamId, int64 newestSample);

    /** Emits a single-electrode burst TTL and forwards the interval to the network detector. */
    void emitSingleElectrodeBurst(const ElectrodeKey& electrode,
                                  const ElectrodeState& state,
                                  int64 triggerSample);

    /** Safely adds a TTL event at a sample offset in the currently processed block. */
    void addTtlEvent(uint16 streamId, int64 sampleNumber, uint8 line, bool state);

    /** Per-stream TTL output settings. These parameters are STREAM_SCOPE, so they
        are read from the owning stream rather than cached in a single member. */
    bool isSingleBurstEnabled(uint16 streamId) const;
    int singleBurstLineForStream(uint16 streamId) const;
    int networkBurstLineForStream(uint16 streamId) const;


    /** Cached copies of the PROCESSOR_SCOPE detector parameters. All times are in
        milliseconds. These initial values must match the defaults registered in
        registerParameters(), which are the single source of truth.

        The literature values for IPSCs (NeuroExplorer Manual, Nex Technologies 2014;
        burst detection methods comparison paper, https://doi.org/10.1152/jn.00093.2016)
        are max_isi_start = 170 and max_isi_end = 300. For primary hippocampal cultures
        our lab has found those exceedingly large, so the registered defaults use lower,
        more sensitive values. */
    int eventDurationMs = 10;
    int timeoutMs = 200;
    int minElectrodes = 3;
    int maxIsiStartMs = 30;
    int maxIsiEndMs = 50;
    int minDurationMs = 10;
    int minSpikes = 3;

    /** Last emitted sample per stream, used to enforce the refractory timeout. */
    std::map<uint16, int64> lastSingleTtlSample;
    std::map<uint16, int64> lastNetworkTtlSample;

    /** True while a network burst is ongoing for a stream, so multiple
        contributing single-electrode bursts don't each re-fire the
        network TTL - only the onset of a new episode does. */
    std::map<uint16, bool> networkBurstActive;

    /** Highest block-start sample number seen so far for each stream. Used
        to detect a looping/restarted data source. */
    std::map<uint16, int64> lastObservedFirstSample;

    /** Resets timing-dependent state for one stream after its sample
        numbers have jumped backwards. */
    void resetStreamTimingState(uint16 streamId);

    /** Drops all in-progress burst candidates and recent-burst history so stale
        detections can't survive a change to the electrode selection or the stream
        layout. Keeps the per-stream recentBursts keys, so it is safe to call from
        the audio thread. */
    void resetDetectionState();

    /** Set by setActive() (message thread) when the electrode selection changes;
        consumed by process() (audio thread) to run resetDetectionState() without
        racing the detection code. */
    std::atomic<bool> detectionResetPending { false };

    /** Per-electrode burst candidates and recent completed bursts. */
    std::map<ElectrodeKey, ElectrodeState> electrodeStates;
    std::map<uint16, std::deque<DetectedBurst>> recentBursts;

    /** Reused across calls to addDetectedBurst() to avoid allocating a fresh
        container on the audio thread every time a burst is reported. */
    std::vector<OverlapEntry> overlapScratch;

     /** Locally generated TTL event channel for each stream. */
    std::map<uint16, EventChannel*> ttlChannels;
    std::vector<PendingTtlOff> pendingTtlOffs;


    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BurstDetector);
};

#endif // BURSTDETECTOR_H_DEFINED