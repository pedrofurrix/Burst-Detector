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
    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
                    "event_duration", // parameter name
                    "TTL Duration", // display name
                    "TTL Event Duration", // parameter description
                    "ms", // unit
                    100, // default value
                    0, // minimum value
                    2000, // maximum value
                    1); // step size
                    
    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
                    "timeout", // parameter name
                    "Timeout", // display name
                    "Timeout for event generation (0 = off)", // parameter description
                    "ms", // unit
                    100, // default value
                    0, // minimum value
                    2000, // maximum value
                    1); // step size

    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
            "min_electrodes", // parameter name
            "Minimum Electrodes", // display name
            "Minimum number of active electrodes required for event generation", // parameter description
            "", // unit
            1, // default value
            1, // minimum value
            1000, // maximum value
            1); // step size

    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
        "max_isi_start", // parameter name
        "Maximum ISI Start", // display name
        "Maximum interspike interval for event generation", // parameter description
        "", // unit
        1, // default value
        1, // minimum value
        1000, // maximum value
        1); // step size
    
    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
        "max_isi_end", // parameter name
        "Maximum ISI End", // display name
        "Maximum interspike interval for event generation", // parameter description
        "", // unit
        1, // default value
        1, // minimum value
        1000, // maximum value
        1); // step size

    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
        "min_duration", // parameter name
        "Minimum Duration", // display name
        "Minimum duration for event generation", // parameter description
        "", // unit
        1, // default value
        1, // minimum value
        1000, // maximum value
        1); // step size
    
    addIntParameter (Parameter::PROCESSOR_SCOPE, // parameter scope
        "min_spikes", // parameter name
        "Minimum Spikes", // display name
        "Minimum number of spikes required for event generation", // parameter description
        "", // unit
        1, // default value
        1, // minimum value
        1000, // maximum value
        1); // step size

    
    // CHECK IF REQUIRED
    addSelectedChannelsParameter (Parameter::STREAM_SCOPE, 
        "Output", 
        "Output", 
        OUTPUT_TOOLTIP, 
        1);


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
   else if (param->getName().equalsIgnoreCase ("min_duration"))
   {
      minDuration = (int) param->getValue();
   }
   else if (param->getName().equalsIgnoreCase ("max_isi_start"))
   {
      maxISIStart = (int) param->getValue();
   }
   else if (param->getName().equalsIgnoreCase ("max_isi_end"))
   {
      maxISIEnd = (int) param->getValue();
   }
   else if (param->getName().equalsIgnoreCase ("min_spikes"))
   {
      minSpikes = (int) param->getValue();
   }
   
//    else if (param->getName().equalsIgnoreCase ("time_constant"))
//    {
//       timeConstant = param->getValue();
//    }
//    else if (param->getName().equalsIgnoreCase ("output_gain"))
//    {
//       outputGain = param->getValue();
//    }


}