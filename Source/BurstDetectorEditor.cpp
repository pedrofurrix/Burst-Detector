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

#include "BurstDetectorEditor.h"
#include "BurstDetector.h"

BurstDetectorEditor::BurstDetectorEditor(GenericProcessor* parentNode) 
    : GenericEditor(parentNode)
{
    desiredWidth = WIDTH;
    
    auto processor = static_cast<BurstDetector*> (getProcessor());

    noInputChannelsLabel = new Label ("NoInputChannelsLabel", "No spike channels detected");
    noInputChannelsLabel->setFont (Font (12.0f, Font::bold));
    noInputChannelsLabel->setBounds (10, 35, 190, 40);
    noInputChannelsLabel->setJustificationType (Justification::centred);
    addAndMakeVisible (noInputChannelsLabel);

    // spike channels
    spikeChannelViewport = new ElectrodeViewport();
    spikeChannelViewport->setScrollBarsShown (true, false, false, false);
    spikeChannelViewport->setBounds (10, 30, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    spikeChannelCanvas = new Component();
    spikeChannelViewport->setViewedComponent (spikeChannelCanvas);
    addAndMakeVisible (spikeChannelViewport);


    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "event_duration", 10 + VIEWPORT_WIDTH + 10, 35);
    ParameterEditor* durationEditor = getParameterEditor ("event_duration");
    durationEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    durationEditor->setSize (80, 30);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "timeout", 10 + VIEWPORT_WIDTH + 10+80+10, 35);
    ParameterEditor* timeoutEditor = getParameterEditor ("timeout");
    timeoutEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    timeoutEditor->setSize (80, 30);


    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "max_isi_start", 10 + VIEWPORT_WIDTH + 10, 70);
    ParameterEditor* maxIsiStartEditor = getParameterEditor ("max_isi_start");
    maxIsiStartEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    maxIsiStartEditor->setSize (80, 30);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "max_isi_end", 10 + VIEWPORT_WIDTH + 10+80+10, 70);
    ParameterEditor* maxIsiEndEditor = getParameterEditor ("max_isi_end");
    maxIsiEndEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    maxIsiEndEditor->setSize (80, 30);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "min_duration", 10 + VIEWPORT_WIDTH + 10, 105);
    ParameterEditor* minDurationEditor = getParameterEditor ("min_duration");
    minDurationEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    minDurationEditor->setSize (80, 30);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "min_spikes", 10 + VIEWPORT_WIDTH + 10+80+10, 105);
    ParameterEditor* minSpikesEditor = getParameterEditor ("min_spikes");
    minSpikesEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    minSpikesEditor->setSize (80, 30);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "min_electrodes", 10 + VIEWPORT_WIDTH + 10, 140);
    ParameterEditor* minElectrodesEditor = getParameterEditor ("min_electrodes");
    minElectrodesEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    minElectrodesEditor->setSize (80, 30);

    addTtlLineParameterEditor(Parameter::STREAM_SCOPE, "single_burst_line", 10 + VIEWPORT_WIDTH + 10, 175);
    ParameterEditor* singleBurstLineEditor = getParameterEditor ("single_burst_line");
    singleBurstLineEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    singleBurstLineEditor->setSize (80, 30);

    addTtlLineParameterEditor(Parameter::STREAM_SCOPE, "network_burst_line", 10 + VIEWPORT_WIDTH + 10+80+10, 175);
    ParameterEditor* networkBurstLineEditor = getParameterEditor ("network_burst_line");
    networkBurstLineEditor->setLayout (ParameterEditor::Layout::nameOnTop);
    networkBurstLineEditor->setSize (80, 30);

}

BurstDetectorEditor::~BurstDetectorEditor()
{
    spikeChannelButtons.clear();
}


void BurstDetectorEditor::updateSettings()
{
    layoutChannelButtons();
}

int BurstDetectorEditor::getNumActiveElectrodes()
{
    int numActive = 0;
    for (auto button : spikeChannelButtons)
    {
        if (button->getToggleState())
        {
            numActive++;
        }
    }
    return numActive;
}

void BurstDetectorEditor::buttonClicked (Button* button)
{
    auto processor = static_cast<BurstDetector*> (getProcessor());

    auto electrodeButton = static_cast<ElectrodeStateButton*> (button);

    bool isActive = electrodeButton->getToggleState();

    processor->spikeChannelActive[electrodeButton->getIdentifier()] = isActive;
}

bool BurstDetectorEditor::getSpikeChannelEnabled (int index)
{
    if (index < 0 || index >= spikeChannelButtons.size())
    {
        jassertfalse;
        return false;
    }
    return spikeChannelButtons[index]->getToggleState();
}

void BurstDetectorEditor::setSpikeChannelEnabled (int index, bool enabled)
{
    if (index < 0 || index >= spikeChannelButtons.size())
    {
        jassertfalse;
        return;
    }
    spikeChannelButtons[index]->setToggleState (enabled, sendNotificationSync);
}

/* -------- private ----------- */

ElectrodeStateButton* BurstDetectorEditor::makeNewChannelButton (SpikeChannel* chan)
{
    auto processor = static_cast<BurstDetector*> (getProcessor());

    bool isActive = processor->isActive (chan);

    auto button = new ElectrodeStateButton (chan);
    button->setToggleState (isActive, dontSendNotification);

    String prefix;
    switch (chan->getChannelType())
    {
        case SpikeChannel::SINGLE:
            prefix = "SE";
            break;

        case SpikeChannel::STEREOTRODE:
            prefix = "ST";
            break;

        case SpikeChannel::TETRODE:
            prefix = "TT";
            break;

        default:
            prefix = "IV";
            break;
    }

    button->setButtonText (prefix + String (chan->getLocalIndex()));
    button->setTooltip (chan->getName());

    return button;
}

void BurstDetectorEditor::layoutChannelButtons()
{
    auto processor = static_cast<BurstDetector*>(getProcessor());

    // Get the currently selected data stream
    DataStream* stream = processor->getDataStream(getCurrentStream());

    // Remove any existing buttons
    spikeChannelButtons.clear();
    spikeChannelCanvas->removeAllChildren();

    // Create one button per spike channel in this stream
    for (auto spikeChannel : stream->getSpikeChannels())
    {
        auto* button = makeNewChannelButton(spikeChannel);

        button->addListener(this);

        spikeChannelButtons.add(button);
        spikeChannelCanvas->addAndMakeVisible(button);
    }

    // Compute canvas size
    int nButtons = spikeChannelButtons.size();
    int nRows = (nButtons > 0) ? ((nButtons - 1) / BUTTONS_PER_ROW + 1) : 0;

    spikeChannelCanvas->setBounds(
        0,
        0,
        VIEWPORT_WIDTH,
        nRows * BUTTON_HEIGHT);

    // Position the buttons
    for (int i = 0; i < nButtons; ++i)
    {
        int row = i / BUTTONS_PER_ROW;
        int col = i % BUTTONS_PER_ROW;

        spikeChannelButtons[i]->setBounds(
            col * BUTTON_WIDTH,
            row * BUTTON_HEIGHT,
            BUTTON_WIDTH,
            BUTTON_HEIGHT);
    }
}