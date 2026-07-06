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
    // Register parameters here, if any
    addFloatParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
                    "event_duration", // parameter name
                    "TTL Duration", // display name
                    "TTL Event Duration", // parameter description
                    "ms", // unit
                    100.0f, // default value
                    0.0f, // minimum value
                    2000.0f, // maximum value
                    1.0f); // step size
                    
    addFloatParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
                    "timeout", // parameter name
                    "Timeout", // display name
                    "Timeout for event generation (0 = off)", // parameter description
                    "ms", // unit
                    100.0f, // default value
                    0.0f, // minimum value
                    2000.0f, // maximum value
                    1.0f); // step size

    addFloatParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
            "minElectrodes", // parameter name
            "Minimum Electrodes", // display name
            "Minimum number of active electrodes required for event generation", // parameter description
            "", // unit
            1.0f, // default value
            1.0f, // minimum value
            1000.0f, // maximum value
            1.0f); // step size

    addFloatParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
        "minElectrodes", // parameter name
        "Minimum Electrodes", // display name
        "Minimum number of active electrodes required for event generation", // parameter description
        "", // unit
        1.0f, // default value
        1.0f, // minimum value
        1000.0f, // maximum value
        1.0f); // step size
}


void BurstDetector::updateSettings()
{
    settings.update (getDataStreams());
        
        for (auto stream : getDataStreams())
        {
        //addTTLChannel("BurstDetector", stream->getStreamID(), settings.eventDuration, settings.timeout);
        }

}


void BurstDetector::process(AudioBuffer<float>& buffer)
{

    checkForEvents(true);

}


void BurstDetector::handleTTLEvent(TTLEventPtr event)
{

}


void BurstDetector::handleSpike(SpikePtr spike)
{

}


void BurstDetector::handleBroadcastMessage (const String& msg, const int64 messageTimeMilliseconds)
{

}


void BurstDetector::saveCustomParametersToXml(XmlElement* parentElement)
{

}


void BurstDetector::loadCustomParametersFromXml(XmlElement* parentElement)
{

}


void BurstDetector::parameterValueChanged(Parameter* param)
{
   if (param->getName().equalsIgnoreCase ("event_duration"))
   {
      eventDuration = (int) param->getValue();
   }
   else if (param->getName().equalsIgnoreCase ("timeout"))
   {
      timeout = (int) param->getValue();
   }
   else if (param->getName().equalsIgnoreCase ("minElectrodes"))
   {
      numActiveElectrodes = (int) param->getValue();
   }
   else if 


}