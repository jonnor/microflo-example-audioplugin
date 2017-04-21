
#include <stdlib.h>
#include <string.h>

/*****************************************************************************/

#include "ladspa.h"

/*****************************************************************************/

/* The maximum delay valid for the delay line (in seconds). If you
   change this, remember that the label is currently "delay_5s". */
static const int MAX_DELAY = 5;

/*****************************************************************************/

/* The port numbers for the plugin: */

enum PLUGIN_PORTS {
    SDL_DELAY_LENGTH = 0,
    SDL_DRY_WET,
    SDL_INPUT,
    SDL_OUTPUT,
    PLUGIN_PORTS_N
};

/*****************************************************************************/

#define CONSTRAIN(x, min, max)          \
(((x) < min) ? min : (((x) > max) ? max : (x)))

/*****************************************************************************/

/* Instance data for the simple delay line plugin. */
typedef struct {

  LADSPA_Data SampleRate;

  LADSPA_Data * Buffer;

  /* Buffer size, a power of two. */
  unsigned long BufferSize;

  /* Write pointer in buffer. */
  unsigned long WritePointer;

  /* Ports:
     ------ */

  /* Delay control, in seconds. Accepted between 0 and 1 (only 1 sec
     of buffer is allocated by this crude delay line). */
  LADSPA_Data * Delay;

  /* Dry/wet control. 0 for entirely dry, 1 for entirely wet. */
  LADSPA_Data * DryWet;

  /* Input audio port data location. */
  LADSPA_Data * Input;

  /* Output audio port data location. */
  LADSPA_Data * Output;

} SimpleDelayLine;

/*****************************************************************************/

/* Construct a new plugin instance. */
static LADSPA_Handle 
instantiate(const LADSPA_Descriptor * Descriptor, unsigned long SampleRate)
{
  SimpleDelayLine *self = (SimpleDelayLine *)malloc(sizeof(SimpleDelayLine));
  if (self == NULL) 
    return NULL;
  
  self->SampleRate = (LADSPA_Data)SampleRate;

  /* Buffer size is a power of two bigger than max delay time. */
  const unsigned long lMinimumBufferSize = (unsigned long)((LADSPA_Data)SampleRate * MAX_DELAY);
  self->BufferSize = 1;
  while (self->BufferSize < lMinimumBufferSize)
    self->BufferSize <<= 1;
  
  self->Buffer = (LADSPA_Data *)calloc(self->BufferSize, sizeof(LADSPA_Data));
  if (self->Buffer == NULL) {
    free(self);
    return NULL;
  }

  self->WritePointer = 0;

  return self;
}

/*****************************************************************************/

/* Initialise and activate a plugin instance. */
static void
activate(LADSPA_Handle Instance) {

  SimpleDelayLine * self;
  self = (SimpleDelayLine *)Instance;

  /* Need to reset the delay history in this function rather than
     instantiate() in case deactivate() followed by activate() have
     been called to reinitialise a delay line. */
  memset(self->Buffer, 0, sizeof(LADSPA_Data) * self->BufferSize);
}

/*****************************************************************************/

/* Connect a port to a data location. */
static void 
connectPort(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data * DataLocation) {

  SimpleDelayLine * self;

  self = (SimpleDelayLine *)Instance;
  switch (Port) {
  case SDL_DELAY_LENGTH:
    self->Delay = DataLocation;
    break;
  case SDL_DRY_WET:
    self->DryWet = DataLocation;
    break;
  case SDL_INPUT:
    self->Input = DataLocation;
    break;
  case SDL_OUTPUT:
    self->Output = DataLocation;
    break;
  }
}

/*****************************************************************************/

/* Run a delay line instance for a block of SampleCount samples. */
static void
run(LADSPA_Handle Instance, unsigned long SampleCount) {
  SimpleDelayLine * self = (SimpleDelayLine *)Instance;

  const unsigned long lDelay = (unsigned long)(CONSTRAIN(*(self->Delay), 0, MAX_DELAY)*self->SampleRate);
  const float fWet = CONSTRAIN(*(self->DryWet), 0, 1);
  const float fDry = 1 - fWet;  

  LADSPA_Data *Input = self->Input;
  LADSPA_Data *Output = self->Output;

  const unsigned long lBufferWriteOffset = self->WritePointer;
  const unsigned long lBufferReadOffset = lBufferWriteOffset + self->BufferSize - lDelay;
  for (unsigned long lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
    LADSPA_Data fInputSample = *(Input++);
    *(Output++) = (fDry * fInputSample
		     + fWet * self->Buffer[((lSampleIndex + lBufferReadOffset) & (self->BufferSize-1))]);
    self->Buffer[((lSampleIndex + lBufferWriteOffset) & (self->BufferSize-1))] = fInputSample;
  }

  self->WritePointer = ((self->WritePointer + SampleCount) & (self->BufferSize-1));
}

/*****************************************************************************/

/* Throw away a simple delay line. */
static void
cleanup(LADSPA_Handle Instance) {

  SimpleDelayLine * self;
  self = (SimpleDelayLine *)Instance;

  free(self->Buffer);
  free(self);
}

/*****************************************************************************/
LADSPA_Descriptor pluginDescriptor = {
    // metadata
    UniqueID: 1043,
    Label: "delay_5s",
    Properties: LADSPA_PROPERTY_HARD_RT_CAPABLE,
    Name: "Simple Delay Line",
    Maker: "Richard Furse (LADSPA example plugins)",
    Copyright: "None",
    PortCount: PLUGIN_PORTS_N
};

/*****************************************************************************/

/* init() is called automatically when the plugin library is first loaded. */
__attribute__ ((constructor))
static void
init() {

    // Port types
    LADSPA_PortDescriptor *portDescriptors = (LADSPA_PortDescriptor *)calloc(PLUGIN_PORTS_N, sizeof(LADSPA_PortDescriptor));
    pluginDescriptor.PortDescriptors = (const LADSPA_PortDescriptor *)portDescriptors;
    portDescriptors[SDL_DELAY_LENGTH] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    portDescriptors[SDL_DRY_WET] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    portDescriptors[SDL_INPUT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    portDescriptors[SDL_OUTPUT] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    char ** portNames = (char **)calloc(PLUGIN_PORTS_N, sizeof(char *));

    // Port names
    pluginDescriptor.PortNames = (const char **)portNames;
    portNames[SDL_DELAY_LENGTH] = strdup("Delay (Seconds)");
    portNames[SDL_DRY_WET] = strdup("Dry/Wet Balance");
    portNames[SDL_INPUT] = strdup("Input");
    portNames[SDL_OUTPUT] = strdup("Output");

    // Port ranges
    LADSPA_PortRangeHint *portRangeHints = ((LADSPA_PortRangeHint *)calloc(PLUGIN_PORTS_N, sizeof(LADSPA_PortRangeHint)));
    pluginDescriptor.PortRangeHints = (const LADSPA_PortRangeHint *)portRangeHints;
    portRangeHints[SDL_DELAY_LENGTH].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    portRangeHints[SDL_DELAY_LENGTH].LowerBound = 0;
    portRangeHints[SDL_DELAY_LENGTH].UpperBound = (LADSPA_Data)MAX_DELAY;
    portRangeHints[SDL_DRY_WET].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    portRangeHints[SDL_DRY_WET].LowerBound = 0;
    portRangeHints[SDL_DRY_WET].UpperBound = 1;
    portRangeHints[SDL_INPUT].HintDescriptor = 0;
    portRangeHints[SDL_OUTPUT].HintDescriptor = 0;


    // functions
    pluginDescriptor.instantiate = instantiate;
    pluginDescriptor.cleanup = cleanup;

    pluginDescriptor.activate = activate;
    pluginDescriptor.deactivate = NULL;

    pluginDescriptor.connect_port = connectPort;

    pluginDescriptor.run = run;
    pluginDescriptor.run_adding = NULL;
    pluginDescriptor.set_run_adding_gain = NULL;
}

/* Return a descriptor of the requested plugin type. Only one plugin
   type is available in this library. */

extern "C" {

const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
  if (Index == 0)
    return &pluginDescriptor;
  else
    return NULL;

}

}
