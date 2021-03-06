#include "VRSound.h"
#include "VRSoundUtils.h"
#include "core/math/path.h"
#include "VRSoundManager.h"


extern "C" {
#include <libavresample/avresample.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

#if _WIN32
#include <al.h>
#include <alc.h>
#include <alext.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

/*
not compiling?
open a terminal and type:
sudo apt-get install libfftw3-dev
*/

#include <fstream>
#include <fftw3.h>
#include <map>
#include <climits>
//#include <complex>

using namespace OSG;

struct VRSound::ALData {
    ALenum sample = 0;
    ALenum format = 0;
    ALenum layout = 0;
    ALenum state = AL_INITIAL;
    AVFormatContext* context = 0;
    AVAudioResampleContext* resampler = 0;
    AVCodecContext* codec = NULL;
    AVPacket packet;
    AVFrame* frame;
};

VRSound::VRSound() {
    VRSoundManager::get(); // this may init channel
    buffers = new uint[Nbuffers];
    al = shared_ptr<ALData>( new ALData() );
}

VRSound::~VRSound() {
    close();
    delete[] buffers;
}

VRSoundPtr VRSound::create() { return VRSoundPtr( new VRSound() ); }

int VRSound::getState() { return al->state; }
string VRSound::getPath() { return path; }
void VRSound::setPath( string p ) { path = p; }

void VRSound::setLoop(bool loop) { this->loop = loop; doUpdate = true; }
void VRSound::setPitch(float pitch) { this->pitch = pitch; doUpdate = true; }
void VRSound::setGain(float gain) { this->gain = gain; doUpdate = true; }
void VRSound::setUser(Vec3f p, Vec3f v) { pos = p; vel = v; doUpdate = true; }
bool VRSound::isRunning() { return al->state == AL_PLAYING; }
void VRSound::stop() { interrupt = true; }

void VRSound::close() {
    ALCHECK( alDeleteSources(1u, &source));
    ALCHECK( alDeleteBuffers(Nbuffers, buffers));
    if(al->context) avformat_close_input(&al->context);
    if(al->resampler) avresample_free(&al->resampler);
    al->context = 0;
    al->resampler = 0;
    init = 0;
}

void VRSound::reset() { al->state = AL_INITIAL; }

void VRSound::updateSource() {
    cout << "update source" << endl;
    ALCHECK( alSourcef(source, AL_PITCH, pitch));
    ALCHECK( alSourcef(source, AL_GAIN, gain));
    ALCHECK( alSource3f(source, AL_POSITION, pos[0], pos[1], pos[2]));
    ALCHECK( alSource3f(source, AL_VELOCITY, vel[0], vel[1], vel[2]));
    doUpdate = false;
}

bool VRSound::initiate() {
    cout << "init sound\n";
    initiated = true;

    ALCHECK( alGenBuffers(Nbuffers, buffers) );
    for (uint i=0; i<Nbuffers; i++) free_buffers.push_back(buffers[i]);

    ALCHECK( alGenSources(1u, &source) );
    updateSource();

    if (path == "") return 1;

    if (avformat_open_input(&al->context, path.c_str(), NULL, NULL) < 0) { cout << "ERROR! avformat_open_input failed\n"; return 0; }
    if (avformat_find_stream_info(al->context, NULL) < 0) { cout << "ERROR! avformat_find_stream_info failed\n"; return 0; }
    av_dump_format(al->context, 0, path.c_str(), 0);

    stream_id = av_find_best_stream(al->context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (stream_id == -1) return 0;

    al->codec = al->context->streams[stream_id]->codec;
    AVCodec* avcodec = avcodec_find_decoder(al->codec->codec_id);
    if (avcodec == 0) return 0;
    if (avcodec_open2(al->codec, avcodec, NULL) < 0) return 0;

    if (al->codec->channel_layout == 0) {
        if (al->codec->channels == 1) al->codec->channel_layout = AV_CH_LAYOUT_MONO;
        if (al->codec->channels == 2) al->codec->channel_layout = AV_CH_LAYOUT_STEREO;
        if (al->codec->channel_layout == 0) cout << "WARNING! channel_layout is 0.\n";
    }

    frequency = al->codec->sample_rate;
    al->format = AL_FORMAT_MONO16;
    AVSampleFormat sfmt = al->codec->sample_fmt;

    if (sfmt == AV_SAMPLE_FMT_NONE) cout << "unsupported format: none\n";

    if (sfmt == AV_SAMPLE_FMT_U8) al->sample = AL_UNSIGNED_BYTE_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S16) al->sample = AL_SHORT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S32) al->sample = AL_INT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_FLT) al->sample = AL_FLOAT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_DBL) al->sample = AL_DOUBLE_SOFT;

    if (sfmt == AV_SAMPLE_FMT_U8P) al->sample = AL_UNSIGNED_BYTE_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S16P) al->sample = AL_SHORT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S32P) al->sample = AL_INT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_FLTP) al->sample = AL_FLOAT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_DBLP) al->sample = AL_DOUBLE_SOFT;

    if (al->codec->channel_layout == AV_CH_LAYOUT_MONO) al->layout = AL_MONO_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_STEREO) al->layout = AL_STEREO_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_QUAD) al->layout = AL_QUAD_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_5POINT1) al->layout = AL_5POINT1_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_7POINT1) al->layout = AL_7POINT1_SOFT;

    switch(al->sample) {
        case AL_UNSIGNED_BYTE_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = AL_FORMAT_MONO8; break;
                case AL_STEREO_SOFT:  al->format = AL_FORMAT_STEREO8; break;
                case AL_QUAD_SOFT:    al->format = alGetEnumValue("AL_FORMAT_QUAD8"); break;
                case AL_5POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_51CHN8"); break;
                case AL_7POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_71CHN8"); break;
                default: cout << "OpenAL unsupported format 8\n"; break;
            } break;
        case AL_SHORT_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = AL_FORMAT_MONO16; break;
                case AL_STEREO_SOFT:  al->format = AL_FORMAT_STEREO16; break;
                case AL_QUAD_SOFT:    al->format = alGetEnumValue("AL_FORMAT_QUAD16"); break;
                case AL_5POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_51CHN16"); break;
                case AL_7POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_71CHN16"); break;
                default: cout << "OpenAL unsupported format 16\n"; break;
            } break;
        case AL_FLOAT_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32"); break;
                case AL_STEREO_SOFT:  al->format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32"); break;
                case AL_QUAD_SOFT:    al->format = alGetEnumValue("AL_FORMAT_QUAD32"); break;
                case AL_5POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_51CHN32"); break;
                case AL_7POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_71CHN32"); break;
                default: cout << "OpenAL unsupported format 32\n"; break;
            } break;
        case AL_DOUBLE_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = alGetEnumValue("AL_FORMAT_MONO_DOUBLE"); break;
                case AL_STEREO_SOFT:  al->format = alGetEnumValue("AL_FORMAT_STEREO_DOUBLE"); break;
                default: cout << "OpenAL unsupported format 64\n"; break;
            } break;
        default: cout << "OpenAL unsupported format";
    }

    if (av_sample_fmt_is_planar(al->codec->sample_fmt)) {
        int out_sample_fmt;
        switch(al->codec->sample_fmt) {
            case AV_SAMPLE_FMT_U8P:  out_sample_fmt = AV_SAMPLE_FMT_U8; break;
            case AV_SAMPLE_FMT_S16P: out_sample_fmt = AV_SAMPLE_FMT_S16; break;
            case AV_SAMPLE_FMT_S32P: out_sample_fmt = AV_SAMPLE_FMT_S32; break;
            case AV_SAMPLE_FMT_DBLP: out_sample_fmt = AV_SAMPLE_FMT_DBL; break;
            case AV_SAMPLE_FMT_FLTP:
            default: out_sample_fmt = AV_SAMPLE_FMT_FLT;
        }

        al->resampler = avresample_alloc_context();
        av_opt_set_int(al->resampler, "in_channel_layout",  al->codec->channel_layout, 0);
        av_opt_set_int(al->resampler, "in_sample_fmt",      al->codec->sample_fmt,     0);
        av_opt_set_int(al->resampler, "in_sample_rate",     al->codec->sample_rate,    0);
        av_opt_set_int(al->resampler, "out_channel_layout", al->codec->channel_layout, 0);
        av_opt_set_int(al->resampler, "out_sample_fmt",     out_sample_fmt,        0);
        av_opt_set_int(al->resampler, "out_sample_rate",    al->codec->sample_rate,    0);
        avresample_open(al->resampler);
    }

    return true;
}

void VRSound::playFrame() {
    cout << "play frame " << endl;
    if (al->state == AL_INITIAL) {
        cout << "reset sound " << endl;
        if (!initiated) initiate();
        if (!al->context) return;
        al->frame = avcodec_alloc_frame();
        av_seek_frame(al->context, stream_id, 0,  AVSEEK_FLAG_FRAME);
        al->state = AL_PLAYING;
    }

    int len;
    if (al->state == AL_PLAYING) {
        if (doUpdate) updateSource();
        auto avrf = av_read_frame(al->context, &al->packet);
        if (interrupt || avrf < 0) {
            if (al->packet.data) {
                cout << "  free packet" << endl;
                av_free_packet(&al->packet);
            }
            cout << "  free frame" << endl;
            av_free(al->frame);
            al->state = loop ? AL_INITIAL : AL_STOPPED;
            return;
        } // End of stream. Done decoding.

        if (al->packet.stream_index != stream_id) { cout << "skip non audio\n"; return; } // Skip non audio packets

        while (al->packet.size > 0) { // Decodes audio data from `packet` into the frame
            if (interrupt) { cout << "interrupt sound\n"; break; }

            int finishedFrame = 0;
            len = avcodec_decode_audio4(al->codec, al->frame, &finishedFrame, &al->packet);
            if (len < 0) { cout << "decoding error\n"; break; }

            if (finishedFrame) {
                if (interrupt) { cout << "interrupt sound\n"; break; }

                // Decoded data is now available in frame->data[0]
                int linesize;
                int data_size = av_samples_get_buffer_size(&linesize, al->codec->channels, al->frame->nb_samples, al->codec->sample_fmt, 0);

                ALbyte* frameData;
                if (al->resampler != 0) {
                    frameData = (ALbyte *)av_malloc(data_size*sizeof(uint8_t));
                    avresample_convert( al->resampler, (uint8_t **)&frameData, linesize, al->frame->nb_samples, (uint8_t **)al->frame->data, al->frame->linesize[0], al->frame->nb_samples);
                } else frameData = (ALbyte*)al->frame->data[0];

                ALint val = -1;
                ALuint bufid = 0;

                do { ALCHECK_BREAK( alGetSourcei(source, AL_BUFFERS_PROCESSED, &val) ); } // recycle buffers
                while (val <= 0 && free_buffers.size() == 0);
                if (val <= 0 && free_buffers.size() == 0) { al->state = AL_STOPPED; return; } // no available buffer, stop!
                for(; val > 0; --val) {
                    ALCHECK( alSourceUnqueueBuffers(source, 1, &bufid));
                    free_buffers.push_back(bufid);
                    queuedBuffers = max(0,queuedBuffers-1);
                }

                bufid = free_buffers.front();
                free_buffers.pop_front();

                queuedBuffers += 1;
                ALCHECK( alBufferData(bufid, al->format, frameData, data_size, frequency));
                ALCHECK( alSourceQueueBuffers(source, 1, &bufid));
                ALCHECK( alGetSourcei(source, AL_SOURCE_STATE, &val));
                if (val != AL_PLAYING) ALCHECK( alSourcePlay(source));
            }

            //There may be more than one frame of audio data inside the packet.
            al->packet.size -= len;
            al->packet.data += len;
        } // while packet.size > 0
    } // while more packets exist inside container.
}

void VRSound::play() {
    if (!initiated) initiate();
    if (!al->context) return;
    if (doUpdate) updateSource();

    al->frame = avcodec_alloc_frame();
    av_seek_frame(al->context, stream_id, 0,  AVSEEK_FLAG_FRAME);

    while (av_read_frame(al->context, &al->packet) >= 0) {
        if (al->packet.stream_index != stream_id) { cout << "skip non audio\n"; return; } // Skip non audio packets

        while (al->packet.size > 0) { // Decodes audio data from `packet` into the frame
            if (interrupt) { cout << "interrupt sound\n"; break; }

            int finishedFrame = 0;
            int len = avcodec_decode_audio4(al->codec, al->frame, &finishedFrame, &al->packet);
            if (len < 0) { cout << "decoding error\n"; break; }

            if (finishedFrame) {
                if (interrupt) { cout << "interrupt sound\n"; break; }

                // Decoded data is now available in frame->data[0]
                int linesize;
                int data_size = av_samples_get_buffer_size(&linesize, al->codec->channels, al->frame->nb_samples, al->codec->sample_fmt, 0);

                ALbyte* frameData;
                if (al->resampler != 0) {
                    frameData = (ALbyte *)av_malloc(data_size*sizeof(uint8_t));
                    avresample_convert( al->resampler, (uint8_t **)&frameData, linesize, al->frame->nb_samples, (uint8_t **)al->frame->data, al->frame->linesize[0], al->frame->nb_samples);
                } else frameData = (ALbyte*)al->frame->data[0];

                ALint val = -1;
                ALuint bufid = 0;

                do { ALCHECK_BREAK( alGetSourcei(source, AL_BUFFERS_PROCESSED, &val) ); } // recycle buffers
                while (val <= 0 && free_buffers.size() == 0);
                if (val <= 0 && free_buffers.size() == 0) { al->state = AL_STOPPED; return; } // no available buffer, stop!
                for(; val > 0; --val) {
                    ALCHECK( alSourceUnqueueBuffers(source, 1, &bufid));
                    free_buffers.push_back(bufid);
                    queuedBuffers = max(0,queuedBuffers-1);
                }

                bufid = free_buffers.front();
                free_buffers.pop_front();

                queuedBuffers += 1;
                ALCHECK( alBufferData(bufid, al->format, frameData, data_size, frequency));
                ALCHECK( alSourceQueueBuffers(source, 1, &bufid));
                ALCHECK( alGetSourcei(source, AL_SOURCE_STATE, &val));
                if (val != AL_PLAYING) ALCHECK( alSourcePlay(source));
            }

            //There may be more than one frame of audio data inside the packet.
            al->packet.size -= len;
            al->packet.data += len;
        } // while packet.size > 0
    }

    if (al->packet.data) av_free_packet(&al->packet);
    av_free(al->frame);
}

void VRSound::playBuffer(vector<short>& buffer, int sample_rate) {
    recycleBuffer();

    ALint val = -1;
    ALuint buf;
    alGenBuffers(1, &buf);
    alBufferData(buf, AL_FORMAT_MONO16, &buffer[0], buffer.size()*sizeof(short), sample_rate);

    queuedBuffers += 1;
    ALCHECK( alSourceQueueBuffers(source, 1, &buf));
    ALCHECK( alGetSourcei(source, AL_SOURCE_STATE, &val));
    if (val != AL_PLAYING) ALCHECK( alSourcePlay(source));
}

// carrier amplitude, carrier frequency, carrier phase, modulation amplitude, modulation frequency, modulation phase, packet duration
void VRSound::synthesize(float Ac, float wc, float pc, float Am, float wm, float pm, float duration) {
    if (!initiated) initiate();

    int sample_rate = 22050;
    size_t buf_size = duration * sample_rate;
    buf_size += buf_size%2;
    vector<short> samples(buf_size);

    for(uint i=0; i<buf_size; i++) {
        float t = i*2*Pi/sample_rate;
        samples[i] = Ac * sin( wc*t + pc + Am*sin(wm*t + pm) );
    }

    playBuffer(samples, sample_rate);
}

vector<short> VRSound::synthesizeSpectrum(vector<double> spectrum, uint sample_rate, float duration, float fade_factor, bool returnBuffer) {
    if (!initiated) initiate();

    /* --- fade in/out curve ---
    ::path c;
    c.addPoint(Vec3f(0,0,0), Vec3f(1,0,0));
    c.addPoint(Vec3f(1,1,0), Vec3f(1,0,0));
    c.compute(sample_rate);
    */

    //ALuint buf;
    //alGenBuffers(1, &buf);
    size_t buf_size = duration * sample_rate;
    uint fade = min(fade_factor * sample_rate, duration * sample_rate); // number of samples to fade at beginning and end

    // transform spectrum back to time domain using fftw3
    vector<double> out(sample_rate);
    //vector<double> out(buf_size);
    // create plan
    fftw_plan ifft;
    //out = (double *) malloc(size*sizeof(double));

    ifft = fftw_plan_r2r_1d(sample_rate, &spectrum[0], &out[0], FFTW_DHT, FFTW_ESTIMATE);   //Setup fftw plan for ifft
    //ifft = fftw_plan_r2r_1d(buf_size, &spectrum[0], &out[0], FFTW_DHT, FFTW_ESTIMATE);
    fftw_execute(ifft); // is output normalized?
    fftw_destroy_plan(ifft);

    vector<short> samples(buf_size);
    for(uint i=0; i<buf_size; ++i) {
        //samples[i] = (double)(SHRT_MAX - 1) * out[i] / (sample_rate * maxVal); // for fftw normalization
        samples[i] = 0.5 * SHRT_MAX * out[i]; // for fftw normalization
    }

    //uint flat = fade / 10;
    /*uint flat = fade / 2;

    for (uint i=0; i < fade; ++i) {
        if (i < flat) {
            samples[i] = 0;
            samples[buf_size-i-1] = 0;
        } else {
            samples[i] *= (float)(i - flat)/(fade - 1 - flat);
            samples[buf_size-i-1] *= (float)(i - flat)/(fade - 1 - flat);
        }
    }*/

    auto calcFade = [](double& t) {
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        // P3*t³ + P2*3*t²*(1-t) + P1*3*t*(1-t)² + P0*(1-t)³
        // P0(0,0) P1(0.5,0) P2(0.5,1) P3(1,1)
        // P0(0,0) P1(0.5,0.1) P2(0.5,1) P3(1,1)
        double s = 1-t;
        return t*t*(3*s + t);
    };

    for (uint i=0; i < fade; ++i) {
        double t = double(i)/(fade-1);
        //double y = c.getPosition(t)[1];
        double y = calcFade(t);
        samples[i] *= y;
        samples[buf_size-i-1] *= y;
    }

    //float flat_samples = 100;
    //uint flat = min((float)fade / 2, flat_samples);
    /*uint flat_samples = 1000;
    uint flat = min(flat_samples, fade);

    for (uint i=0; i < fade; ++i) {
        if (i < flat) {
            if (i > 100 && i < 200){
                //samples[i] = 0.5 * SHRT_MAX;
                samples[buf_size-i-1] = 0.5 * SHRT_MAX;
            } else {
                //samples[i] = 0;
                samples[buf_size-i-1] = 0;
            }
        } else {
            double t = double(i - flat)/(fade - 1 - flat);
            //samples[i] *= calcFade(t);
            samples[buf_size-i-1] *= calcFade(t);
        }
    }*/

    /*for (uint i=0; i < fade*2; ++i) {
        double t = (double(i)-double(fade))/(fade-1);
        //double y = c.getPosition(t)[1];
        double y = calcFade(t);
        //samples[i] *= y;
        samples[buf_size-i-1] *= y;
    }*/

    playBuffer(samples, sample_rate);
    return returnBuffer ? samples : vector<short>();
}

vector<short> VRSound::synthBuffer(vector<Vec2d> freqs1, vector<Vec2d> freqs2, float duration) {
    if (!initiated) initiate();
    // play sound
    int sample_rate = 22050;
    size_t buf_size = duration * sample_rate;
    vector<short> samples(buf_size);
    double Ni = 1.0/freqs1.size();
    double T = 2*Pi/sample_rate;
    static map<int,complex<double>> phasors;
    for (uint i=0; i<buf_size; i++) {
        double k = double(i)/(buf_size-1);
        samples[i] = 0;
        for (int j=0; j<freqs1.size(); j++) {
            double A = freqs1[j][1]*(1.0-k) + freqs2[j][1]*k;
            double f = freqs1[j][0]*(1.0-k) + freqs2[j][0]*k;

            if (!phasors.count(j)) phasors[j] = complex<double>(1,0);
            phasors[j] *= exp( complex<double>(0,T*f) );
            samples[i] += A*Ni*phasors[j].imag();
        }
    }
    playBuffer(samples, sample_rate);
    if (true) return samples;
    return vector<short>();
}

int VRSound::getQueuedBuffer() { return queuedBuffers; }

void VRSound::recycleBuffer() {
    ALint val = -1;
    ALuint bufid = 0; // TODO: not working properly!!
    do { ALCHECK_BREAK( alGetSourcei(source, AL_BUFFERS_PROCESSED, &val) ); // recycle buffers
        for(; val > 0; --val) {
            ALCHECK( alSourceUnqueueBuffers(source, 1, &bufid));
            if ( queuedBuffers > 0 ) queuedBuffers -= 1;
        }
    } while (val > 0);
}





