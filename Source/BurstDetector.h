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

#ifndef PROCESSORPLUGIN_H_DEFINED
#define PROCESSORPLUGIN_H_DEFINED

#include <ProcessorHeaders.h>


class BurstDetector : public GenericProcessor
{
public:
	/** The class constructor, used to initialize any members. */
	BurstDetector();

	/** The class destructor, used to deallocate memory */
	~BurstDetector();

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor* createEditor() override;

	/** All plugin parameter objects must be created inside this method */
    void registerParameters() override;

	/** Called every time the settings of an upstream plugin are changed.
		Allows the processor to handle variations in the channel configuration or any other parameter
		passed through signal chain. The processor can use this function to modify channel objects that
		will be passed to downstream plugins. */
	// void updateSettings() override;

	/** Defines the functionality of the processor.
		The process method is called every time a new data buffer is available.
		Visualizer plugins typically use this method to send data to the canvas for display purposes */
	void process(AudioBuffer<float>& buffer) override;

	/** Handles events received by the processor
		Called automatically for each received event whenever checkForEvents() is called from
		the plugin's process() method */
	void handleTTLEvent(TTLEventPtr event) override;

	/** Handles spikes received by the processor
		Called automatically for each received spike whenever checkForEvents(true) is called from
		the plugin's process() method */
	void handleSpike(SpikePtr spike) override;

	/** Handles broadcast messages sent during acquisition
		Called automatically whenever a broadcast message is sent through the signal chain */
    void handleBroadcastMessage (const String& message, const int64 messageTimeMilliseconds) override;

	/** Saving custom settings to XML. This method is not needed to save the state of
		Parameter objects */
	void saveCustomParametersToXml(XmlElement* parentElement) override;

	/** Load custom settings from XML. This method is not needed to load the state of
		Parameter objects*/
	void loadCustomParametersFromXml(XmlElement* parentElement) override;


private:

    // functions
    int getNumActiveElectrodes();
    void updateSettings() override;
    ;

    // internals
    StreamSettings<MeanSpikeRateSettings> settings;
    std::map<uint16, int> currSample; // per-buffer - allows processing samples while handling events
    std::map<uint16, double> spikeAmp; // updated once per buffer
    std::map<uint16, float> currMean;
    std::map<uint16, float*> wpBuffer;
    std::map<uint16, double> decayPerSample; // updated once per buffer

    const String OUTPUT_TOOLTIP = "Continuous channel to overwrite with the spike rate (meaned over time and selected electrodes)";
    const String TIME_CONST_TOOLTIP = "Time for the influence of a single spike to decay to 36.8% (1/e) of its initial value (larger = smoother, smaller = faster reaction to changes)";
	



	// parameters
	int eventDuration; // in milliseconds
    int timeout; // milliseconds after an event onset when no more events are allowed.
	int minElectrodes; // minimum number of electrodes that must fire within the time window to count as a burst
	int maxISIStart; // maximum inter-spike interval (in milliseconds) between the first two spikes of a burst
	int maxISIEnd; // maximum inter-spike interval (in milliseconds) between the last two spikes of a burst
	int minDuration; // minimum duration of a burst (in milliseconds)
	int minSpikes; // minimum number of spikes in a burst
	
	
	float timeConstant; // time constant for the exponential decay of the mean spike rate
	float outputGain; // gain for the output channel
	EventChannel* ttlChannel; // local pointer to TTL output channel

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeanSpikeRate);

};
endif