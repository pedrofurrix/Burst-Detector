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


#ifndef BURSTDETECTOR_H_DEFINED
#define BURSTDETECTOR_H_DEFINED

#include <ProcessorHeaders.h>

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

    /** Runs max-interval detection for each upstream spike. */
    void handleSpike(SpikePtr spike) override;

    /** Handles broadcast messages sent during acquisition. */
    void handleBroadcastMessage(const String& message, const int64 messageTimeMilliseconds) override;

    /** No extra custom state is saved beyond Parameter objects. */
    void saveCustomParametersToXml(XmlElement* parentElement) override;

    /** No extra custom state is loaded beyond Parameter objects. */
    void loadCustomParametersFromXml(XmlElement* parentElement) override;

    bool isActive(const SpikeChannel* chan) const;

    void setActive(const String& identifier, bool active);

    int getNumActiveElectrodes() const;

    Array<SpikeChannel*> spikeChannels;
    std::map<String, bool> spikeChannelActive;

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
    

    /** User-facing detector parameters. All times are in milliseconds. */
    int eventDurationMs = 10;
    int timeoutMs = 200;
    int minElectrodes = 3;
    int maxIsiStartMs = 170;
    int maxIsiEndMs = 300;
    int minDurationMs = 10;
    int minSpikes = 3;
    bool singleBurstEnabled = false;
    int singleBurstLine = -1;
    int networkBurstLine = 1;

    /** Last emitted sample per stream, used to enforce the refractory timeout. */
    std::map<uint16, int64> lastSingleTtlSample;
    std::map<uint16, int64> lastNetworkTtlSample;

    /** True while a network burst is ongoing for a stream, so multiple
        contributing single-electrode burst reports (including refreshes of
        an already-reported burst's end sample) don't each re-fire the
        network TTL - only the onset of a new episode does. */
    std::map<uint16, bool> networkBurstActive;

    /** Highest block-start sample number seen so far for each stream. Used
        to detect a looping/restarted data source (sample numbers jumping
        backwards), which would otherwise strand pending TTL "off" events
        scheduled against the old, higher sample count. */
    std::map<uint16, int64> lastObservedFirstSample;

    /** Resets timing-dependent state for one stream after its sample
        numbers have jumped backwards (see lastObservedFirstSample). Fires
        any pending TTL offs for that stream immediately instead of leaving
        them stranded, and clears debounce/candidate state so detection
        starts cleanly for the new pass over the data. */
    void resetStreamTimingState(uint16 streamId);

    /** Per-electrode burst candidates and recent completed bursts. */
    std::map<ElectrodeKey, ElectrodeState> electrodeStates;
    std::map<uint16, std::deque<DetectedBurst>> recentBursts;

    /** Reused across calls to addDetectedBurst() to avoid allocating a fresh
        container on the audio thread every time a burst is reported. */
    std::vector<std::pair<ElectrodeKey, DetectedBurst>> overlapScratch;

     /** Locally generated TTL event channel for each stream. */
    std::map<uint16, EventChannel*> ttlChannels;
    std::vector<PendingTtlOff> pendingTtlOffs;


    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BurstDetector);
};

#endif // BURSTDETECTOR_H_DEFINED