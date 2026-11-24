#include "EchoFX.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* msg) {
    if (!condition) throw std::runtime_error(msg);
}

int main() {
    try {
        constexpr int sr=44100,n=256;
        EchoInsert insert(sr);
        std::vector<float> x(2*n),dry;
        for(int i=0;i<2*n;++i)x[i]=.2f*std::sin(.13f*i);
        dry=x;
        insert.process(x.data(),n);
        require(x==dry,"default insert must be transparent");
        EchoReturns returns(sr);
        std::vector<float> e(2*n),r(2*n),out(2*n);
        returns.process(e.data(),r.data(),out.data(),n);
        require(std::all_of(out.begin(),out.end(),[](float v){return v==0;}),"silent return must be silent");
        EchoReturnParams p;p.time=.02f;p.feedback=.92f;p.ping=1;p.shimmer=1;p.size=1;p.damp=0;
        returns.set(p);
        double delayL=0,delayR=0;
        for(int b=0;b<sr*5/n;++b) {
            std::fill(out.begin(),out.end(),0);
            std::fill(e.begin(),e.end(),0);
            if(b==0)e[0]=e[1]=.5f;
            returns.process(e.data(),r.data(),out.data(),n);
            for(int i=0;i<n;++i){
                require(std::isfinite(out[2*i])&&std::isfinite(out[2*i+1]),"nonfinite delay");
                delayL+=out[2*i]*out[2*i];delayR+=out[2*i+1]*out[2*i+1];
            }
        }
        require(delayL>1e-6&&delayR>1e-6,"ping-pong must reach both channels");
        EchoReturns room(sr);room.set(p);
        double peak=0,early=0,late=0;
        for(int b=0;b<sr*25/n;++b){
            std::fill(out.begin(),out.end(),0);
            std::fill(r.begin(),r.end(),0);
            if(b<sr/n)for(int i=0;i<n;++i)r[2*i]=r[2*i+1]=.2f*std::sin(.025f*(b*n+i))+.05f;
            room.process(e.data(),r.data(),out.data(),n);
            for(float v:out){
                require(std::isfinite(v),"nonfinite shimmer feedback");
                peak=std::max(peak,double(std::abs(v)));
                if(b<sr*5/n)early+=v*v;
                if(b>sr*20/n)late+=v*v;
            }
        }
        require(early>1e-6&&peak<8&&late<early,"shimmer tail must decay, remain bounded");
        EchoInsertParams ip;ip.filterMix=1;ip.cutoff=.2f;ip.resonance=.4f;ip.tape=.4f;ip.hiss=.2f;
        insert.set(ip);
        for(int b=0;b<sr/n;++b){x=dry;insert.process(x.data(),n);}
        require(x!=dry,"filter/tape must change signal");
        for(float v:x)require(std::isfinite(v),"nonfinite insert");
        std::puts("ECHOFIELD FX tests passed: transparent bypass, ping-pong, bounded shimmer, insert");
        return 0;
    } catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
