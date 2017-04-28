
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <array>

#include <ladspa.h>

/* The port numbers for the plugin: */
enum PLUGIN_PORTS {
    PORT_VALUE = 0,
    PORT_DRY_WET,
    PORT_INPUT,
    PORT_OUTPUT,
    PLUGIN_PORTS_N
};

#define CONSTRAIN(x, min, max) (((x) < min) ? min : (((x) > max) ? max : (x)))

/* Instance data */
typedef struct {
  LADSPA_Data SampleRate;
  LADSPA_Data *portData[PLUGIN_PORTS_N];
} InstanceData;

/* Construct a new plugin instance. */
static LADSPA_Handle 
instantiate(const LADSPA_Descriptor * Descriptor, unsigned long SampleRate)
{
  InstanceData *self = new InstanceData;
  if (self == NULL) 
    return NULL;
  
  self->SampleRate = (LADSPA_Data)SampleRate;

  return self;
}

/* Initialise and activate a plugin instance. */
static void
activate(LADSPA_Handle Instance) {
  //InstanceData * self = (InstanceData *)Instance;
}

/* Connect a port to a data location. */
static void 
connectPort(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data * DataLocation)
{
  InstanceData * self = (InstanceData *)Instance;
  if (Port >= 0 and Port < PLUGIN_PORTS_N) {
    self->portData[Port] = DataLocation;
  } else {
    fprintf(stderr, "ERROR: unsupported port %lu", Port);
  }
}

/* Run a delay line instance for a block of SampleCount samples. */
static void
run(LADSPA_Handle Instance, unsigned long SampleCount) {
  InstanceData * self = (InstanceData *)Instance;

  const float amp = CONSTRAIN(*(self->portData[PORT_VALUE]), 0, 1);
  //const float fWet = CONSTRAIN(*(self->portData[PORT_DRY_WET]), 0, 1);
  //const float fDry = 1 - fWet;  

  LADSPA_Data *Input = self->portData[PORT_INPUT];
  LADSPA_Data *Output = self->portData[PORT_OUTPUT];

  for (unsigned long index = 0; index < SampleCount; index++) {
    const LADSPA_Data in = *(Input++);
    *(Output++) = (in * amp);
  }
}

/* Cleanup instance data */
static void
cleanup(LADSPA_Handle Instance)
{
  InstanceData * self = (InstanceData *)Instance;
  delete self;
}

struct Port {
    std::string name;
    LADSPA_PortDescriptor descriptor;
    LADSPA_PortRangeHint range;
};

struct Plugin {
    LADSPA_Descriptor descriptor;
    Port ports[PLUGIN_PORTS_N];

    const char *portNames[PLUGIN_PORTS_N];
    LADSPA_PortDescriptor portDescriptors[PLUGIN_PORTS_N];
    LADSPA_PortRangeHint portRangeHints[PLUGIN_PORTS_N];

    static void initialize(Plugin &plugin) {
        plugin.descriptor.PortNames = plugin.portNames;
        plugin.descriptor.PortDescriptors = plugin.portDescriptors;
        plugin.descriptor.PortRangeHints = plugin.portRangeHints;

        for (int i=0; i<PLUGIN_PORTS_N; i++) {
            Port p = plugin.ports[i];
            // don't know why, but both LMMS and analyseplugin needs copied name to detect the ports
            plugin.portNames[i] = strdup(p.name.c_str());
            plugin.portDescriptors[i] = p.descriptor;
            plugin.portRangeHints[i] = p.range;
        }
    }
    static void destroy(Plugin &plugin) {
        for (int i=0; i<PLUGIN_PORTS_N; i++) {
            free((char *)plugin.portNames[i]);
        }
    }
};

static Plugin plugin = {
    descriptor: {
        // metadata
        UniqueID: 1049,
        Label: "delay_5s",
        Properties: LADSPA_PROPERTY_HARD_RT_CAPABLE,
        Name: "MicroFlo example plugin",
        Maker: "Jon Nordby",
        Copyright: "None",
        PortCount: PLUGIN_PORTS_N,
        PortDescriptors: NULL,
        PortNames: NULL,
        PortRangeHints: NULL,
        ImplementationData: NULL,
        // function pointers
        instantiate: instantiate,
        connect_port: connectPort,
        activate: activate,
        run: run,
        run_adding: NULL,
        set_run_adding_gain: NULL,
        deactivate: NULL,
        cleanup: cleanup,
    },
    ports: {
        { "Delay (Seconds)", LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
            {
                HintDescriptor: LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE,
                LowerBound: 0.0,
                UpperBound: 1.0,
            }
        },
        { "Dry/Wet Balance", LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
            {
                HintDescriptor: LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE,
                LowerBound: 0.0,
                UpperBound: 1.0,
            }
        },
        { "Input", LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO, { HintDescriptor: 0 } },
        { "Output", LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO, { HintDescriptor: 0 } },
    },
    portNames: {},
    portDescriptors: {},
    portRangeHints: {},
};

__attribute__ ((destructor))
static void destroy() {
  Plugin::destroy(plugin);
}

/* Return a descriptor of the requested plugin type.
Only one plugin type is available in this library. */
extern "C" {

const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {

  Plugin::initialize(plugin);

  if (Index == 0) {
    LADSPA_Descriptor *d = &(plugin.descriptor);
    return d;
  } else {
    return NULL;
  }
}

} // end extern "C"
