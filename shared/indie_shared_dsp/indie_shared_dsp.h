/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 indie_shared_dsp
  vendor:             Dr Fix Audio
  version:            1.0.0
  name:               Indie Shared DSP
  description:        Reusable DSP building blocks shared across the Indie plugin suite.
  dependencies:       juce_audio_basics, juce_dsp

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/PitchShifter.h"
#include "dsp/Diffuser.h"
#include "dsp/OnsetDetector.h"
