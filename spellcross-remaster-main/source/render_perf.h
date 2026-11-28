#pragma once
// Temporary tactical renderer profiler. No polling threads or extra paints.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#include <wx/dc.h>
#include <wx/font.h>
#include <wx/window.h>

namespace spellcross_perf
{
    using Clock=std::chrono::steady_clock;
    using Stamp=Clock::time_point;
    inline double Millis(Stamp a,Stamp b)
    {
        return std::chrono::duration<double,std::milli>(b-a).count();
    }
    inline double ProcessCpuSeconds()
    {
#ifdef _WIN32
        FILETIME created{},exited{},kernel{},user{};
        if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)) return -1;
        const auto ticks=[](FILETIME t) {
            return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;
        };
        return (ticks(kernel)+ticks(user))*1e-7;
#else
        const auto cpu=std::clock();
        return cpu==static_cast<std::clock_t>(-1)?-1:double(cpu)/CLOCKS_PER_SEC;
#endif
    }
    inline double ResidentMiB()
    {
#ifdef _WIN32
        // Kernel32 export on Windows 7+; no extra linker dependency.
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb=sizeof(counters);
        if(K32GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters)))
            return double(counters.WorkingSetSize)/1048576.0;
#elif defined(__linux__)
        std::ifstream status("/proc/self/status");
        std::string key;
        while(status>>key)
        {
            if(key=="VmRSS:")
            {
                double kb=0;
                if(status>>kb) return kb/1024.0;
                break;
            }
            std::string rest;
            std::getline(status,rest);
        }
#endif
        return -1;
    }
    struct State
    {
        bool enabled=false;
        Stamp window{},lastFrame{};
        unsigned frames=0;
        double total=0,peak=0,mapTotal=0,rgbTotal=0;
        double fps=0,avg=0,maxMs=0,mapMs=0,rgbMs=0,lastMs=0;
        double cpu=-1,ram=-1,cpuStart=-1;
        double pendingMap=0,pendingRGB=0;
        bool haveSample=false;
        void Toggle()
        {
            const bool next=!enabled;
            *this=State{};
            enabled=next;
            if(enabled) { window=Clock::now(); cpuStart=ProcessCpuSeconds(); ram=ResidentMiB(); }
        }
        void Record(Stamp begin,Stamp end)
        {
            if(!enabled) return;
            // A long gap (e.g. paused modal/idle) is not a render stall.
            // Start a fresh interval after returning to the canvas.
            if(lastFrame!=Stamp{} && Millis(lastFrame,begin)>2000)
            {
                window=begin; frames=0; total=peak=mapTotal=rgbTotal=0;
                cpuStart=ProcessCpuSeconds(); haveSample=false;
            }
            lastFrame=end;
            lastMs=Millis(begin,end);
            ++frames; total+=lastMs; peak=(std::max)(peak,lastMs);
            mapTotal+=pendingMap; rgbTotal+=pendingRGB;
            const double elapsed=Millis(window,end);
            if(elapsed<1000) return;
            fps=1000.0*frames/elapsed;
            avg=total/frames; maxMs=peak; mapMs=mapTotal/frames; rgbMs=rgbTotal/frames;
            const double nowCpu=ProcessCpuSeconds();
            const auto cores=(std::max)(1u,std::thread::hardware_concurrency());
            cpu=nowCpu>=0 && cpuStart>=0 && nowCpu>=cpuStart?
                std::clamp((nowCpu-cpuStart)*100000.0/(elapsed*cores),0.0,100.0):-1;
            ram=ResidentMiB(); cpuStart=nowCpu;
            haveSample=true; window=end; frames=0; total=peak=mapTotal=rgbTotal=0;
        }
    };
    inline State& Current() { static State state; return state; }
    inline void Draw(wxDC& dc,wxWindow* window)
    {
        const auto& s=Current();
        if(!s.enabled) return;
        const wxString fps=s.haveSample?wxString::Format("%.1f FPS",s.fps):wxString("FPS: sampling...");
        const wxString render=s.haveSample?
            wxString::Format("Render %.1f ms | avg %.1f | max %.1f",s.lastMs,s.avg,s.maxMs):
            wxString::Format("Render %.1f ms",s.lastMs);
        const wxString phase=s.haveSample?
            wxString::Format("Map %.1f ms | RGB %.1f ms",s.mapMs,s.rgbMs):wxString("Map / RGB: sampling...");
        const wxString cpu=s.cpu>=0?wxString::Format("%.1f%%",s.cpu):wxString("N/A");
        const wxString ram=s.ram>=0?wxString::Format("%.0f MiB",s.ram):wxString("N/A");
        const wxString resources="CPU (process) "+cpu+" | RAM "+ram;
        const wxString lines[]={fps,render,phase,resources};
        const auto oldFont=dc.GetFont();
        const auto oldPen=dc.GetPen();
        const auto oldBrush=dc.GetBrush();
        const auto oldText=dc.GetTextForeground();
        dc.SetFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
        int width=0,height=0;
        for(const auto& line:lines)
        {
            const auto size=dc.GetTextExtent(line);
            width=(std::max)(width,size.x); height=(std::max)(height,size.y);
        }
        const int pad=window->FromDIP(6),x=window->FromDIP(8),y=x;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(15,24,17)));
        dc.DrawRectangle(x,y,width+2*pad,4*height+2*pad);
        dc.SetTextForeground(wxColour(195,246,170));
        for(int i=0;i<4;++i) dc.DrawText(lines[i],x+pad,y+pad+i*height);
        dc.SetFont(oldFont); dc.SetPen(oldPen); dc.SetBrush(oldBrush); dc.SetTextForeground(oldText);
    }
}
