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

BurstDetectorEditor::BurstDetectorEditor(GenericProcessor* parentNode) 
    : GenericEditor(parentNode)
{
    desiredWidth = 300;

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "event_duration", 15, 35);
    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "timeout", 115, 35);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "max_isi_start", 15, 70);
    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "max_isi_end", 115, 70);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "min_duration", 15, 105);
    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "min_spikes", 115, 105);

    addBoundedValueParameterEditor(Parameter::PROCESSOR_SCOPE, "min_electrodes", 15, 140);

    addTtlLineParameterEditor(Parameter::STREAM_SCOPE, "single_burst_line", 15, 175);
    addTtlLineParameterEditor(Parameter::STREAM_SCOPE, "network_burst_line", 115, 175);

}
