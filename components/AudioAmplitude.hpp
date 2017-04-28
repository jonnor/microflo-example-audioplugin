/* microflo_component yaml
name: AudioAmplitude
description: Scale the amplitude of an Audio::Buffer
inports:
  in:
    type: AudioBuffer
    description: ""
  amplitude:
    type: number
    description: ""
outports:
  out:
    type: all
    description: ""
  amplitudechanged:
    type: number
    description: ""
  error:
    type: all
    description: ""
microflo_component */
class AudioAmplitude : public SingleOutputComponent {
public:
    virtual void process(Packet in, MicroFlo::PortId port) {
        using namespace AudioAmplitudePorts;
        if (port == InPorts::amplitude) {
            amp = in.asFloat();
            send(in, OutPorts::amplitudechanged);
        } else if (port == InPorts::in) {
            Audio::Buffer *buf = (Audio::Buffer *)in.asPointer(Audio::BufferType);
            if (!buf) {
                send(Packet(ErrorUnsupportedType), OutPorts::error);
                return;
            }
            for (size_t i=0; i<buf->n_samples; i++) {
                buf->data[i] *= amp; 
            }
            send(in);
        } else {
            
        }
    }
private:
    float amp = 0.3;
};
