
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
instantiate(const LADSPA_Descriptor * Descriptor,
			   unsigned long             SampleRate) {

  unsigned long lMinimumBufferSize;
  SimpleDelayLine * psDelayLine;

  psDelayLine 
    = (SimpleDelayLine *)malloc(sizeof(SimpleDelayLine));

  if (psDelayLine == NULL) 
    return NULL;
  
  psDelayLine->SampleRate = (LADSPA_Data)SampleRate;

  /* Buffer size is a power of two bigger than max delay time. */
  lMinimumBufferSize = (unsigned long)((LADSPA_Data)SampleRate * MAX_DELAY);
  psDelayLine->BufferSize = 1;
  while (psDelayLine->BufferSize < lMinimumBufferSize)
    psDelayLine->BufferSize <<= 1;
  
  psDelayLine->Buffer 
    = (LADSPA_Data *)calloc(psDelayLine->BufferSize, sizeof(LADSPA_Data));
  if (psDelayLine->Buffer == NULL) {
    free(psDelayLine);
    return NULL;
  }

  psDelayLine->WritePointer = 0;
  
  return psDelayLine;
}

/*****************************************************************************/

/* Initialise and activate a plugin instance. */
static void
activate(LADSPA_Handle Instance) {

  SimpleDelayLine * psSimpleDelayLine;
  psSimpleDelayLine = (SimpleDelayLine *)Instance;

  /* Need to reset the delay history in this function rather than
     instantiate() in case deactivate() followed by activate() have
     been called to reinitialise a delay line. */
  memset(psSimpleDelayLine->Buffer, 0, 
	 sizeof(LADSPA_Data) * psSimpleDelayLine->BufferSize);
}

/*****************************************************************************/

/* Connect a port to a data location. */
static void 
connectPort(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data * DataLocation) {

  SimpleDelayLine * psSimpleDelayLine;

  psSimpleDelayLine = (SimpleDelayLine *)Instance;
  switch (Port) {
  case SDL_DELAY_LENGTH:
    psSimpleDelayLine->Delay = DataLocation;
    break;
  case SDL_DRY_WET:
    psSimpleDelayLine->DryWet = DataLocation;
    break;
  case SDL_INPUT:
    psSimpleDelayLine->Input = DataLocation;
    break;
  case SDL_OUTPUT:
    psSimpleDelayLine->Output = DataLocation;
    break;
  }
}

/*****************************************************************************/

/* Run a delay line instance for a block of SampleCount samples. */
static void
run(LADSPA_Handle Instance, unsigned long SampleCount) {
  
  LADSPA_Data * pfBuffer;
  LADSPA_Data * pfInput;
  LADSPA_Data * pfOutput;
  LADSPA_Data fDry;
  LADSPA_Data fInputSample;
  LADSPA_Data fWet;
  SimpleDelayLine * psSimpleDelayLine;
  unsigned long lBufferReadOffset;
  unsigned long lBufferSizeMinusOne;
  unsigned long lBufferWriteOffset;
  unsigned long lDelay;
  unsigned long lSampleIndex;

  psSimpleDelayLine = (SimpleDelayLine *)Instance;

  lBufferSizeMinusOne = psSimpleDelayLine->BufferSize - 1;
  lDelay = (unsigned long)
    (CONSTRAIN(*(psSimpleDelayLine->Delay), 0, MAX_DELAY) 
     * psSimpleDelayLine->SampleRate);

  pfInput = psSimpleDelayLine->Input;
  pfOutput = psSimpleDelayLine->Output;
  pfBuffer = psSimpleDelayLine->Buffer;
  lBufferWriteOffset = psSimpleDelayLine->WritePointer;
  lBufferReadOffset
    = lBufferWriteOffset + psSimpleDelayLine->BufferSize - lDelay;
  fWet = CONSTRAIN(*(psSimpleDelayLine->DryWet), 0, 1);
  fDry = 1 - fWet;

  for (lSampleIndex = 0;
       lSampleIndex < SampleCount;
       lSampleIndex++) {
    fInputSample = *(pfInput++);
    *(pfOutput++) = (fDry * fInputSample
		     + fWet * pfBuffer[((lSampleIndex + lBufferReadOffset)
					& lBufferSizeMinusOne)]);
    pfBuffer[((lSampleIndex + lBufferWriteOffset)
	      & lBufferSizeMinusOne)] = fInputSample;
  }

  psSimpleDelayLine->WritePointer
    = ((psSimpleDelayLine->WritePointer + SampleCount)
       & lBufferSizeMinusOne);
}

/*****************************************************************************/

/* Throw away a simple delay line. */
static void
cleanup(LADSPA_Handle Instance) {

  SimpleDelayLine * psSimpleDelayLine;
  psSimpleDelayLine = (SimpleDelayLine *)Instance;

  free(psSimpleDelayLine->Buffer);
  free(psSimpleDelayLine);
}

/*****************************************************************************/

LADSPA_Descriptor * g_psDescriptor = NULL;

/*****************************************************************************/

/* init() is called automatically when the plugin library is first loaded. */
__attribute__ ((constructor))
static void
init() {

  char ** pcPortNames;
  LADSPA_PortDescriptor * piPortDescriptors;
  LADSPA_PortRangeHint * psPortRangeHints;

  g_psDescriptor
    = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
  if (g_psDescriptor) {
    // Plugin info
    g_psDescriptor->UniqueID = 1043;
    g_psDescriptor->Label = strdup("delay_5s");
    g_psDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    g_psDescriptor->Name = strdup("Simple Delay Line");
    g_psDescriptor->Maker = strdup("Richard Furse (LADSPA example plugins)");
    g_psDescriptor->Copyright = strdup("None");
    g_psDescriptor->PortCount = PLUGIN_PORTS_N;

    // Port types
    piPortDescriptors = (LADSPA_PortDescriptor *)calloc(PLUGIN_PORTS_N, sizeof(LADSPA_PortDescriptor));
    g_psDescriptor->PortDescriptors = (const LADSPA_PortDescriptor *)piPortDescriptors;
    piPortDescriptors[SDL_DELAY_LENGTH] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_DRY_WET] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_INPUT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_OUTPUT] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    pcPortNames = (char **)calloc(PLUGIN_PORTS_N, sizeof(char *));

    // Port names
    g_psDescriptor->PortNames = (const char **)pcPortNames;
    pcPortNames[SDL_DELAY_LENGTH] = strdup("Delay (Seconds)");
    pcPortNames[SDL_DRY_WET] = strdup("Dry/Wet Balance");
    pcPortNames[SDL_INPUT] = strdup("Input");
    pcPortNames[SDL_OUTPUT] = strdup("Output");

    // Port ranges
    psPortRangeHints = ((LADSPA_PortRangeHint *)calloc(PLUGIN_PORTS_N, sizeof(LADSPA_PortRangeHint)));
    g_psDescriptor->PortRangeHints = (const LADSPA_PortRangeHint *)psPortRangeHints;
    psPortRangeHints[SDL_DELAY_LENGTH].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_DELAY_LENGTH].LowerBound = 0;
    psPortRangeHints[SDL_DELAY_LENGTH].UpperBound = (LADSPA_Data)MAX_DELAY;
    psPortRangeHints[SDL_DRY_WET].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
    psPortRangeHints[SDL_DRY_WET].LowerBound = 0;
    psPortRangeHints[SDL_DRY_WET].UpperBound = 1;
    psPortRangeHints[SDL_INPUT].HintDescriptor = 0;
    psPortRangeHints[SDL_OUTPUT].HintDescriptor = 0;

    g_psDescriptor->instantiate = instantiate;
    g_psDescriptor->cleanup = cleanup;

    g_psDescriptor->activate = activate;
    g_psDescriptor->deactivate = NULL;

    g_psDescriptor->connect_port = connectPort;

    g_psDescriptor->run = run;
    g_psDescriptor->run_adding = NULL;
    g_psDescriptor->set_run_adding_gain = NULL;
  }
}

/*****************************************************************************/

/* fini() is called automatically when the library is unloaded. */
__attribute__ ((destructor))
static void
fini() {
  if (g_psDescriptor) {
    free((char *)g_psDescriptor->Label);
    free((char *)g_psDescriptor->Name);
    free((char *)g_psDescriptor->Maker);
    free((char *)g_psDescriptor->Copyright);
    free((LADSPA_PortDescriptor *)g_psDescriptor->PortDescriptors);
    for (unsigned int lIndex = 0; lIndex < g_psDescriptor->PortCount; lIndex++)
      free((char *)(g_psDescriptor->PortNames[lIndex]));
    free((char **)g_psDescriptor->PortNames);
    free((LADSPA_PortRangeHint *)g_psDescriptor->PortRangeHints);
    free(g_psDescriptor);
  }
}

/*****************************************************************************/

/* Return a descriptor of the requested plugin type. Only one plugin
   type is available in this library. */

extern "C" {

const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
  if (Index == 0)
    return g_psDescriptor;
  else
    return NULL;

}

}
