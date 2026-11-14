#define NOMINMAX
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include <miniaudio.h>
#include "audio.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>
#include <vector>

namespace {
constexpr int rate=44100;
constexpr float tau=6.2831853f;
using Buffer=std::vector<float>;   // mono
float env(float t,float attack,float decay){return t<attack?t/attack:std::exp(-(t-attack)/decay);}
Buffer synth(Sfx s){
    std::mt19937 rng(int(s)*7919+1);std::uniform_real_distribution<float> n(-1,1);
    auto make=[&](float seconds){return Buffer(size_t(seconds*rate),0.f);};
    Buffer b;
    switch(s){
    case Sfx::Step:{b=make(.12f);float lp=0;for(size_t i=0;i<b.size();++i){float t=float(i)/rate;lp+=(n(rng)-lp)*.08f;b[i]=lp*env(t,.004f,.025f)*1.8f+std::sin(tau*70*t)*env(t,.002f,.03f)*.4f;}break;}
    case Sfx::Hit:case Sfx::HeroHit:{b=make(.35f);float f0=s==Sfx::Hit?95.f:70.f;float lp=0;
        for(size_t i=0;i<b.size();++i){float t=float(i)/rate;lp+=(n(rng)-lp)*.25f;
            b[i]=std::sin(tau*f0*t*(1.f-t*.8f))*env(t,.002f,.08f)*.9f+lp*env(t,.001f,.04f)*.8f;}break;}
    case Sfx::Loot:{b=make(.5f);for(size_t i=0;i<b.size();++i){float t=float(i)/rate;
        b[i]=(std::sin(tau*1568*t)*env(t,.002f,.12f)+std::sin(tau*2093*(t-.07f))*(t>.07f?env(t-.07f,.002f,.15f):0))*.35f;}break;}
    case Sfx::Chest:{b=make(.8f);for(size_t i=0;i<b.size();++i){float t=float(i)/rate;
        float creak=std::sin(tau*(180+120*std::sin(tau*9*t))*t)*(t<.45f?env(t,.05f,.3f):0)*.25f;
        float knock=t>.5f?std::sin(tau*110*(t-.5f))*env(t-.5f,.002f,.06f)*.7f:0;b[i]=creak+knock;}break;}
    case Sfx::Shrine:{b=make(2.6f);const float f[]={392,494,587,784};for(size_t i=0;i<b.size();++i){float t=float(i)/rate;float v=0;
        for(int k=0;k<4;++k){float tt=t-float(k)*.12f;if(tt>0)v+=std::sin(tau*f[k]*tt)*env(tt,.01f,.9f)*(.25f+.1f*std::sin(tau*3*tt));}b[i]=v*.35f;}break;}
    case Sfx::Click:{b=make(.06f);for(size_t i=0;i<b.size();++i){float t=float(i)/rate;b[i]=std::sin(tau*1200*t)*env(t,.001f,.012f)*.35f;}break;}
    case Sfx::LevelUp:{b=make(1.4f);const float f[]={523,659,784,1047};for(size_t i=0;i<b.size();++i){float t=float(i)/rate;float v=0;
        for(int k=0;k<4;++k){float tt=t-float(k)*.11f;if(tt>0)v+=(std::sin(tau*f[k]*tt)+.3f*std::sin(tau*f[k]*2*tt))*env(tt,.005f,.35f);}b[i]=v*.22f;}break;}
    case Sfx::Warn:{b=make(1.1f);for(size_t i=0;i<b.size();++i){float t=float(i)/rate;
        b[i]=(std::sin(tau*82*t)+.5f*std::sin(tau*123*t)+.25f*std::sin(tau*164*t))*env(t,.08f,.4f)*.35f;}break;}
    case Sfx::Magic:{b=make(.7f);float lp=0;for(size_t i=0;i<b.size();++i){float t=float(i)/rate;lp+=(n(rng)-lp)*(.05f+t*.3f);
        b[i]=lp*env(t,.05f,.2f)*1.4f+std::sin(tau*(300+600*t)*t)*env(t,.02f,.15f)*.2f;}break;}
    case Sfx::Defeat:{b=make(2.5f);for(size_t i=0;i<b.size();++i){float t=float(i)/rate;
        b[i]=(std::sin(tau*110*t)+std::sin(tau*116.5f*t)+.5f*std::sin(tau*55*t))*env(t,.2f,.9f)*.25f;}break;}
    case Sfx::Door:{b=make(.9f);float lp=0;for(size_t i=0;i<b.size();++i){float t=float(i)/rate;lp+=(n(rng)-lp)*.02f;
        b[i]=lp*3.f*env(t,.1f,.4f)+std::sin(tau*60*t)*env(t,.05f,.3f)*.4f;}break;}
    case Sfx::Victory:{b=make(2.2f);const float f[]={392,523,659,784,1047};for(size_t i=0;i<b.size();++i){float t=float(i)/rate;float v=0;
        for(int k=0;k<5;++k){float tt=t-float(k)*.15f;if(tt>0)v+=std::sin(tau*f[k]*tt)*env(tt,.01f,.6f);}b[i]=v*.2f;}break;}
    }
    return b;
}
// Music: short generated loops in natural minor/dorian; crossfaded by the mixer.
Buffer compose(Music m){
    float seconds=m==Music::Combat||m==Music::Boss?16.f:24.f;
    Buffer b(size_t(seconds*rate),0.f);std::mt19937 rng(int(m)*31+7);std::uniform_real_distribution<float> n(-1,1);
    auto note=[](int semis){return 146.83f*std::pow(2.f,float(semis)/12.f);};   // D3 root
    auto pluck=[&](float start,float freq,float amp,float decay){
        size_t s0=size_t(start*rate);for(size_t i=s0;i<b.size()&&i<s0+size_t(decay*5*rate);++i){float t=float(i-s0)/rate;
            b[i]+=amp*env(t,.004f,decay)*(std::sin(tau*freq*t)+.35f*std::sin(tau*freq*2*t)+.12f*std::sin(tau*freq*3*t));}};
    auto pad=[&](float freq,float amp){for(size_t i=0;i<b.size();++i){float t=float(i)/rate;float swell=.6f+.4f*std::sin(tau*t/seconds);
        b[i]+=amp*swell*(std::sin(tau*freq*t)+.5f*std::sin(tau*freq*1.5f*t+1.f)+.3f*std::sin(tau*freq*2.003f*t));}};
    auto drum=[&](float start,float freq,float amp){size_t s0=size_t(start*rate);for(size_t i=s0;i<b.size()&&i<s0+size_t(.5f*rate);++i){float t=float(i-s0)/rate;
        b[i]+=amp*(std::sin(tau*freq*t*(1.f-t))*env(t,.002f,.12f)+n(rng)*env(t,.001f,.02f)*.3f);}};
    const int dorian[]={0,2,3,5,7,9,10,12,14,15};
    switch(m){
    case Music::Menu:case Music::Village:{
        pad(note(-12),.05f);pad(note(-5),.03f);
        int pattern[]={0,4,7,4,2,4,8,4,0,5,7,5,3,5,9,5};
        for(int bar=0;bar<int(seconds/2);++bar)for(int k=0;k<8;++k){int idx=pattern[(bar%2)*8+k];
            pluck(float(bar)*2+float(k)*.25f,note(dorian[idx%10]),.10f,.35f);}
        break;}
    case Music::Explore:{
        float lp=0;for(size_t i=0;i<b.size();++i){float t=float(i)/rate;lp+=(n(rng)-lp)*.01f;b[i]+=lp*.6f*(.6f+.4f*std::sin(tau*t/8));}
        pad(note(-12),.035f);
        for(int k=0;k<14;++k)pluck(float(k)*1.7f+float(rng()%10)*.05f,note(dorian[rng()%8]+12),.07f,.8f);
        break;}
    case Music::Dungeon:{
        pad(note(-24),.06f);pad(note(-17),.025f);
        for(int k=0;k<10;++k){float t0=float(rng()%int(seconds*10))/10.f;pluck(t0,note(24+int(rng()%5)),.03f,.08f);}  // drips
        for(int k=0;k<6;++k)pluck(float(k)*4+1.3f,note(dorian[rng()%6]),.05f,1.4f);
        break;}
    case Music::Combat:case Music::Boss:{
        bool boss=m==Music::Boss;float beat=boss?.375f:.45f;
        pad(note(-24),.04f);
        for(int k=0;k*beat<seconds;++k){float t=float(k)*beat;drum(t,k%4==0?55.f:k%2==0?70.f:90.f,k%4==0?.5f:.25f);}
        int ost[]={0,0,3,0,5,3,7,5};
        for(int k=0;k*beat*.5f<seconds;++k)pluck(float(k)*beat*.5f,note(ost[k%8]-12+(boss&&k%16>=8?1:0)),.07f,.15f);
        break;}
    default:break;
    }
    float peak=0;for(float v:b)peak=std::max(peak,std::abs(v));
    if(peak>0)for(float& v:b)v*=.5f/peak;
    // short fade at the loop seam
    size_t f=rate/20;for(size_t i=0;i<f&&i<b.size();++i){float g=float(i)/float(f);b[i]*=g;b[b.size()-1-i]*=g;}
    return b;
}
}

struct Audio::Impl {
    ma_device device{};bool ready=false;std::mutex lock;
    std::array<Buffer,13> sfx;std::array<Buffer,7> tracks;
    struct Voice{const Buffer* buffer;size_t pos;float gain;};
    std::vector<Voice> voices;
    int current=0,next=0;size_t pos=0,nextPos=0;float fade=1;
    float master=.8f,musicVol=.6f,sfxVol=.8f;
    static void callback(ma_device* d,void* out,const void*,ma_uint32 frames){
        auto* self=static_cast<Impl*>(d->pUserData);auto* o=static_cast<float*>(out);
        std::lock_guard<std::mutex> g(self->lock);
        for(ma_uint32 i=0;i<frames;++i){
            float v=0;
            const Buffer& a=self->tracks[size_t(self->current)];
            if(!a.empty()){v+=a[self->pos%a.size()]*self->fade*self->musicVol;++self->pos;}
            if(self->next!=self->current){
                const Buffer& c=self->tracks[size_t(self->next)];
                if(!c.empty()){v+=c[self->nextPos%c.size()]*(1-self->fade)*self->musicVol;++self->nextPos;}
                self->fade-=1.f/(rate*1.5f);
                if(self->fade<=0){self->current=self->next;self->pos=self->nextPos;self->fade=1;}
            }
            for(auto& voice:self->voices)if(voice.pos<voice.buffer->size())v+=(*voice.buffer)[voice.pos++]*voice.gain*self->sfxVol;
            v=std::tanh(v*self->master*1.2f);
            o[i*2]=v;o[i*2+1]=v;
        }
        self->voices.erase(std::remove_if(self->voices.begin(),self->voices.end(),[](const Voice& x){return x.pos>=x.buffer->size();}),self->voices.end());
    }
};
Audio::Audio():impl(std::make_unique<Impl>()){
    for(int i=0;i<13;++i)impl->sfx[size_t(i)]=synth(Sfx(i));
    for(int i=1;i<7;++i)impl->tracks[size_t(i)]=compose(Music(i));
    ma_device_config config=ma_device_config_init(ma_device_type_playback);
    config.playback.format=ma_format_f32;config.playback.channels=2;config.sampleRate=rate;
    config.dataCallback=Impl::callback;config.pUserData=impl.get();
    if(ma_device_init(nullptr,&config,&impl->device)!=MA_SUCCESS){std::cerr<<"Audio: no playback device; continuing without sound\n";return;}
    if(ma_device_start(&impl->device)!=MA_SUCCESS){std::cerr<<"Audio: device failed to start; continuing without sound\n";ma_device_uninit(&impl->device);return;}
    impl->ready=true;
}
Audio::~Audio(){if(impl->ready)ma_device_uninit(&impl->device);}
bool Audio::ok() const{return impl->ready;}
void Audio::play(Sfx s,float volume){
    if(!impl->ready)return;std::lock_guard<std::mutex> g(impl->lock);
    if(impl->voices.size()>24)impl->voices.erase(impl->voices.begin());
    impl->voices.push_back({&impl->sfx[size_t(s)],0,volume});
}
void Audio::music(Music m){
    if(!impl->ready)return;std::lock_guard<std::mutex> g(impl->lock);
    if(int(m)==impl->next)return;
    impl->next=int(m);impl->nextPos=0;impl->fade=1;
    if(impl->current==impl->next){impl->fade=1;}
}
void Audio::volumes(float master,float music,float sfx){
    if(!impl->ready)return;std::lock_guard<std::mutex> g(impl->lock);impl->master=master;impl->musicVol=music;impl->sfxVol=sfx;
}
