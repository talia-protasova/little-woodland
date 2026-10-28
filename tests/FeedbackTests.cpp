#include "presentation/BoardPlayback.h"
#include "presentation/FrameTiming.h"
#include "audio/Mixer.h"
#include "rendering/BoardLayout.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace woodland;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
/**
 * Derives expected cue counts from the fixed demo's actual trace, then advances
 * through it with long and partial updates. Each crossed stage must emit once,
 * while idle result frames emit nothing and advance only the character clock.
 * Restart and a one-move defeat check that neither stale time nor victory audio
 * survives into the wrong presentation state.
 */
void cuesAndClock(){
    Level level(LevelDefinition::demo());BoardPlayback p;
    p.click(level,Cell{4,6});
    check(p.takeSoundCues()==std::vector<SoundCue>{SoundCue::Selection},"Selection not emitted exactly once");
    p.click(level,Cell{4,6});check(p.takeSoundCues().empty(),"Deselection emitted sound");
    p.click(level,Cell{1,3});check(p.takeSoundCues().empty(),"Letter selection emitted sound");
    for(auto pair:{std::pair<Cell,Cell>{{4,6},{5,6}},{{5,3},{6,3}}}){
        Board::Trace trace;check(level.trySwap(pair.first,pair.second,&trace).valid,"Cue fixture invalid");
        int matches=0,falls=0,deliveries=0;double beforeResult=0;
        for(const auto& step:trace){
            matches+=step.stage==Board::Stage::Removal;falls+=step.stage==Board::Stage::Gravity;
            if(step.stage==Board::Stage::Delivery)deliveries+=step.matched.count();
            if(step.stage!=Board::Stage::Victory)beforeResult+=BoardPlayback::duration(step.stage);
        }
        p.play(trace);
        const bool won=trace.back().stage==Board::Stage::Victory;
        if(won){
            // Result artwork appears after a 200ms outgoing fade; its authored
            // frame clock must exclude all earlier gameplay and that fade interval.
            p.update(level,beforeResult+0.19);
            check(p.resultSeconds()==0 && !p.showingResult(level),"Character clock started before result page");
            p.update(level,0.02);
            check(std::abs(p.resultSeconds()-0.01)<1e-6,"Character clock did not start at page appearance");
        }
        p.update(level,100);
        const auto events=p.takeSoundCues();
        check(std::count(events.begin(),events.end(),SoundCue::Match)==matches,"Match cue count wrong");
        check(std::count(events.begin(),events.end(),SoundCue::Falling)==falls,"Falling cue count wrong");
        check(std::count(events.begin(),events.end(),SoundCue::Delivery)==deliveries,"Delivery cue count wrong");
        check(std::count(events.begin(),events.end(),SoundCue::Victory)==(won?1:0),"Victory cue repeated or early");
        p.update(level,1);check(p.takeSoundCues().empty(),"Idle/result repeated a cue");
        if(!won)check(p.resultSeconds()==0,"Character clock running during gameplay");
    }
    const auto time=p.resultSeconds();p.update(level,0.25);check(std::abs(p.resultSeconds()-time-.25)<1e-6,"Result animation stopped prematurely");
    level.restart();p={};check(p.resultSeconds()==0 && p.takeSoundCues().empty(),"Restart left sound/animation state");
    auto loss=LevelDefinition::demo();loss.moveLimit=1;Level lost(loss);
    p.click(lost,Cell{4,6});p.click(lost,Cell{5,6});p.update(lost,100);
    const auto events=p.takeSoundCues();check(std::count(events.begin(),events.end(),SoundCue::Victory)==0,"Defeat reused victory audio");
}
/**
 * Tests unequal authored frame delays, exact wrap boundaries, finite-loop holding
 * and invalid duration lists without GPU textures. Also checks that the session
 * sound-control rectangle stays usable across gameplay layout sizes.
 */
void clocks(){
    FrameTiming loop({.03,.04,.03});
    check(loop.frame(0)==0 && loop.frame(.03)==1 && loop.frame(.07)==2 && loop.frame(.1)==0,"Frame boundaries/loop wrong");
    FrameTiming finite({.03,.04,.03},1);check(finite.frame(100)==2,"Finite loop did not hold last frame");
    // Floating-point accumulation and direct elapsed time should choose the same
    // boundary frame, including when an infinite sequence wraps to its beginning.
    double partitioned=0;for(int i=0;i<100;++i)partitioned+=.013;
    check(loop.frame(partitioned)==loop.frame(1.3),"Frame timing depends on update partition");
    for(const auto durations:{std::vector<double>{},std::vector<double>{0},std::vector<double>{-.1}}){
        bool failed=false;try{FrameTiming invalid(durations);}catch(const std::invalid_argument&){failed=true;}check(failed,"Invalid timeline accepted");
    }
    for(auto size:{Cell{480,800},Cell{800,480},Cell{960,1600}}){
        const auto button=soundToggleBounds(size.row,size.column,false);
        check(button.x>=0 && button.y>=0 && button.x+button.width<=size.row && button.y+button.height<=size.column,"Sound toggle clipped");
        check(button.contains(button.x+button.width/2,button.y+button.height/2),"Sound toggle hit area incorrect");
    }
}
/**
 * Uses constant stereo samples to measure gain and priority numerically. A burst
 * of requests checks bounded output; mute must immediately clear active cursors
 * and unmute must not replay the discarded tails. No audio device is involved.
 */
void mixing(){
    // Constant unit samples make expected gains measurable without audio hardware.
    // Repeated requests stress voice limits; this is not an audible quality test.
    Mixer m;for(auto& clip:m.clips)clip.assign(44100*2,1.f);
    for(int i=0;i<100;++i)m.play(SoundCue::Match);
    float output[32];m.render(output,16);
    check(std::all_of(output,output+32,[](float v){return std::isfinite(v)&&std::abs(v)<=.7f;}),"Mixer clipped/unbounded");
    m.mute(true);m.render(output,16);check(std::all_of(output,output+32,[](float v){return v==0;}),"Mute not immediate");
    m.mute(false);m.render(output,16);check(output[0]==0,"Unmute resumed stale effects");
    // Victory replaces the earlier wave/fall voices. Unit input multiplied by
    // its 0.20 cue gain and 0.60 master gain must leave exactly 0.12 at output.
    m.play(SoundCue::Match);m.play(SoundCue::Falling);m.play(SoundCue::Victory);m.render(output,16);
    check(std::abs(output[0]-.12f)<1e-6,"Victory did not take priority");
    m.stop();m.play(SoundCue::Selection);m.render(output,16);check(output[0]>0,"No mixed samples");
    m.mute(true);m.stop();check(m.muted(),"Restart stop lost mute setting");
}
int main(){try{cuesAndClock();clocks();mixing();std::cout<<"Sound event counts, character clock, mixer limits/mute and animation timing passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
