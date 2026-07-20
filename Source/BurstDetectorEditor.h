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

#ifndef PROCESSORPLUGINEDITOR_H_DEFINED
#define PROCESSORPLUGINEDITOR_H_DEFINED

#include <EditorHeaders.h>
#include "BurstDetector.h"


/** 

    Used to change the selection state for a particular electrode
*/
class ElectrodeStateButton : public ElectrodeButton
{
public:
    /** Constructor */
    // Calls the ElectrodeButton constructor with a null pointer for the SpikeChannel. Stores the identifier string for the channel.
    ElectrodeStateButton (SpikeChannel* chan) : ElectrodeButton (0)
    {
        identifier = chan->getIdentifier();
    }

    /** Destructor */
    ~ElectrodeStateButton() {};

    /** Returns the identifier string for this electrode */
    String getIdentifier() { return identifier; }

private:
    String identifier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectrodeStateButton);
};

/** 

    Scrollable viewport for electrode buttons

*/
class ElectrodeViewport : public Viewport
{
public:
    /** Constructor */
    ElectrodeViewport() {};

    /** Destructor */
    ~ElectrodeViewport() {};

    /** Called when viewport changes size*/
    void resized() override {};

    /** Override mouseWheelMove to prevent scrolling conflict with editor viewport */
    void mouseWheelMove (const MouseEvent& event, const MouseWheelDetails& wheel) override {}
};


class BurstDetectorEditor : public GenericEditor, public Button::Listener
{
public:

	/** Constructor */
	BurstDetectorEditor(GenericProcessor* parentNode);

	/** Destructor */
	~BurstDetectorEditor();

	/** Updates the UI with the current processor state */
    void updateSettings() override;

    /** Called when the selected stream has changed */
    void selectedStreamHasChanged() override { updateSettings(); }

    /** Returns the number of currently selected electrodes */
    int getNumActiveElectrodes();

    /** Call back for electrode selection buttons*/
    void buttonClicked (Button* button) override;

    /** Returns true if a particular electrode is enabled */
    bool getSpikeChannelEnabled (int index);

    /** Sets the enabled state for a particular electrode */
    void setSpikeChannelEnabled (int index, bool enabled);

private:
	
	ElectrodeStateButton* makeNewChannelButton (SpikeChannel* chan);
	void layoutChannelButtons();

    // UI elements
    ScopedPointer<Label> noInputChannelsLabel;
    ScopedPointer<ElectrodeViewport> spikeChannelViewport;
    ScopedPointer<Component> spikeChannelCanvas;
    OwnedArray<ElectrodeStateButton> spikeChannelButtons;

    // constants
    static const int BUTTON_WIDTH = 27;
    static const int BUTTON_HEIGHT = 15;

    static const int WIDTH = 400;
    static const int VIEWPORT_WIDTH = 90;
    static const int VIEWPORT_HEIGHT = 50;
    static const int BUTTONS_PER_ROW = 3; //CONTENT_WIDTH / BUTTON_WIDTH;

	/** Generates an assertion if this class leaks */
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BurstDetectorEditor);
	

};

#endif // BurstDetectorEditor_H_DEFINED