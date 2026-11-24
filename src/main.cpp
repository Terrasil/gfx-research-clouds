#include <gfx/research/application.hpp>
#include <gfx/research/framebuffer.hpp>
#include <gfx/research/fullscreen_triangle.hpp>
#include "png_writer.hpp"
#include <gfx/research/shader.hpp>
#include "atmosphere.hpp"
#include "cloud_noise.hpp"
#include "shader_source.hpp"
#include "volume_settings.hpp"
#include "experiment_gpu_timer.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>
#include <utility>
#include <glm/vec2.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
namespace {
constexpr const char* methods[]={"M0 Simplified Nubis","M1 Nubis","M2 Nubis + OPT16","M3 Nubis + FST25","M4 Nubis + OPT16 + FST25","M5 Nubis + AAL","M6 Nubis + OPT16 + AAL","M7 Nubis + FST25 + AAL","M8 Nubis + OPT16 + FST25 + AAL","REF High-Precision Reference","REF2 Convergence Reference"};
constexpr const char* ids[]={"m0","m1","m2","m3","m4","m5","m6","m7","m8","ref","ref2"};
constexpr float pi=3.14159265358979323846f;
float rad(float x){return x*pi/180.f;}
struct TimePreset{const char* name;float elevation,azimuth;};
constexpr TimePreset times[]={{"Dawn",6.f,20.f},{"Morning",25.f,55.f},{"Noon",65.f,90.f},{"Sunset",4.f,160.f},{"Twilight",-5.f,165.f},{"Night",-22.f,220.f}};
struct Case{int method,time,pitch;float coverage;int budget;int w,h;};
struct Options{int profile=0;fs::path output;};
std::string pathLabel(const Case& c){return std::string(times[c.time].name)+"-p"+std::to_string(c.pitch)+"-c"+std::to_string(int(std::lround(c.coverage*100)))+"-b"+std::to_string(c.budget)+"-"+std::to_string(c.w)+"x"+std::to_string(c.h);}
std::string glString(GLenum e){auto* s=glGetString(e);return s?reinterpret_cast<const char*>(s):"unknown";}
class Clouds final:public gfx::research::Application{
public:
 explicit Clouds(Options o):Application({.title="gfx-research-clouds",.width=1600,.height=900,.vsync=false}),options_(std::move(o)){}
 int execute(){int r=run();if(running_){running_=false;std::ofstream state(output_/"status.txt");state<<"incomplete\ncompleted_cases="<<caseIndex_<<"\nplanned_cases="<<cases_.size()<<'\n';return r==0?2:r;}return r==0&&failed_?2:r;}
private:
 Options options_;fs::path root_=GFX_RESEARCH_SOURCE_DIR,output_;
 gfx::research::Shader shader_,accumShader_;gfx::research::FullscreenTriangle triangle_;gfx::research::Framebuffer target_;
 std::array<gfx::research::Framebuffer,2> history_;gfx::research::Framebuffer* display_=&target_;
 bool temporal_=false,historyReady_=false;int historyW_=0,historyH_=0,historyIndex_=0,historyCount_=0,sequence_=0;std::vector<float> previous_;
 CloudNoise noise_;VolumeSettings vol_;ExperimentGpuTimer timer_;
 int mode_=0,time_=2,optSteps_=16,refSteps_=512,refLight_=32,jitter_=0,phase_=0;
 float pitch_=30,yaw_=45,fov_=60,sun_=65,azimuth_=90,clock_=12,aal_=.10f;
 bool animate_=false,visible_=true,capture_=false,running_=false,failed_=false,fullscreenActive_=false;
 int windowedX_=0,windowedY_=0,windowedWidth_=1600,windowedHeight_=900;
 int width_=1600,height_=900,caseFrame_=0,warm_=5,samples_=20;size_t caseIndex_=0;
 std::vector<Case> cases_;std::vector<double> durations_;std::vector<float> reference_;std::ofstream raw_,summary_,quality_;
 std::string status_="Ready";
 bool on_init() override{
  const gfx::research::ShaderSource sources[]={{gfx::research::ShaderStage::Vertex,shaderSource(root_/"shaders/fullscreen.vert")},{gfx::research::ShaderStage::Fragment,shaderSource(root_/"shaders/clouds.frag")}};
  const gfx::research::ShaderSource accumulation[]={{gfx::research::ShaderStage::Vertex,shaderSource(root_/"shaders/fullscreen.vert")},{gfx::research::ShaderStage::Fragment,shaderSource(root_/"shaders/accumulate.frag")}};
  if(!shader_.load(sources)||!accumShader_.load(accumulation)||!noise_.create(root_/"shaders/cloud_noise.comp"))return false;
  const gfx::research::ColorAttachmentDesc desc[]={{GL_RGBA8,GL_LINEAR,GL_LINEAR},{GL_RGBA32F,GL_NEAREST,GL_NEAREST}};
  if(!target_.create(width_,height_,desc,false))return false;
  glDisable(GL_DEPTH_TEST);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
  configureGui(); if(options_.profile)start(options_.profile);return true;
 }
 void on_resize(int w,int h) override{if(!running_&&w>0&&h>0){width_=w;height_=h;target_.resize(w,h);}}
 void setTime(int i){time_=i;sun_=times[i].elevation;azimuth_=times[i].azimuth;}
 bool temporalActive()const{return temporal_&&!running_&&!animate_;}
 static float halton(int i,int base){float f=1.,r=0.;while(i>0){f/=float(base);r+=f*float(i%base);i/=base;}return r;}
 void prepareHistory(){
  if(!temporalActive()){historyCount_=0;sequence_=0;previous_.clear();return;}
  std::vector<float> key={float(mode_),pitch_,yaw_,fov_,sun_,azimuth_,clock_,aal_,float(optSteps_),float(refSteps_),float(refLight_),float(jitter_),float(phase_),float(vol_.cloudSamples),float(vol_.lightSamples),vol_.coverage,vol_.density,vol_.scale,vol_.depth,vol_.detail,vol_.type,vol_.weather,vol_.selfShadow,vol_.anisotropy,vol_.multipleScattering,vol_.powder,vol_.turbidity,vol_.atmosphereExposure,float(width_),float(height_)};
  if(key!=previous_){historyCount_=0;sequence_=0;previous_=std::move(key);}
  if(!historyReady_){const gfx::research::ColorAttachmentDesc desc[]={{GL_RGBA8,GL_LINEAR,GL_LINEAR},{GL_RGBA32F,GL_NEAREST,GL_NEAREST}};for(auto& h:history_)if(!h.create(width_,height_,desc,false))throw std::runtime_error("Cannot create temporal history");historyReady_=true;historyW_=width_;historyH_=height_;}
  else if(width_!=historyW_||height_!=historyH_){for(auto& h:history_)h.resize(width_,height_);historyW_=width_;historyH_=height_;historyCount_=0;sequence_=0;}
 }
 void accumulate(){
  if(!temporalActive())return;
  int next=1-historyIndex_;history_[next].bind();glViewport(0,0,width_,height_);accumShader_.bind();glBindTextureUnit(0,target_.color(1));glBindTextureUnit(1,history_[historyIndex_].color(1));accumShader_.set_int("uCurrent",0);accumShader_.set_int("uHistory",1);accumShader_.set_int("uCount",historyCount_);triangle_.draw();historyIndex_=next;historyCount_=std::min(historyCount_+1,64);sequence_=(sequence_+1)%65536;display_=&history_[historyIndex_];
 }
 void bind(float t){
  shader_.bind();noise_.bind();shader_.set_int("uCloudShape",4);shader_.set_int("uCloudDetail",5);shader_.set_int("uBounds",6);
  shader_.set_vec2("uResolution",{float(width_),float(height_)});shader_.set_float("uPitch",rad(pitch_));shader_.set_float("uYaw",rad(yaw_));shader_.set_float("uFov",rad(fov_));
  shader_.set_float("uSunElevation",rad(sun_));shader_.set_float("uSunAzimuth",rad(azimuth_));shader_.set_float("uTime",t);
  shader_.set_float("uFieldScaleRadius",300.f);shader_.set_vec3("uSkyCenter",{0,0,0});
  shader_.set_int("uMode",mode_);shader_.set_int("uOptSteps",optSteps_);shader_.set_int("uReferenceSteps",mode_==10?refSteps_*2:refSteps_);shader_.set_int("uReferenceLight",mode_==10?refLight_*2:refLight_);shader_.set_int("uPhaseIndex",phase_+(temporalActive()?sequence_:0));shader_.set_int("uJitterMode",jitter_);shader_.set_int("uTemporal",temporalActive()?1:0);shader_.set_vec2("uPixelJitter",temporalActive()?glm::vec2(halton(sequence_+1,2)-.5f,halton(sequence_+1,3)-.5f):glm::vec2(0));shader_.set_float("uAalTolerance",aal_);
  glUniform4i(glGetUniformLocation(shader_.id(),"uVolumeBudget"),vol_.cloudSamples,vol_.lightSamples,vol_.emptyStep,vol_.auroraSamples);
  shader_.set_vec4("uVolumeField",{vol_.scale,vol_.coverage,vol_.density,vol_.depth});shader_.set_vec4("uVolumeShape",{vol_.detail,vol_.softness,vol_.selfShadow,vol_.type});shader_.set_vec4("uVolumeOptics",{vol_.anisotropy,vol_.multipleScattering,vol_.powder,vol_.detailScale});shader_.set_vec4("uVolumeMotion",{12.f,4.f,.8f,vol_.weather});shader_.set_vec2("uVolumeSampling",{vol_.lightCone,vol_.jitter});
  auto a=atmosphereConstants(std::sin(rad(sun_)),vol_.turbidity,vol_.atmosphereExposure);shader_.set_vec4("uAtmosphere",a.zenith);shader_.set_vec4("uAtmosphereNorm",a.normalization);
 }
 void on_frame(const gfx::research::FrameInfo& frame) override{
  if(frame.width<=0||frame.height<=0)return;
  if(!running_&&(frame.width!=width_||frame.height!=height_)){width_=frame.width;height_=frame.height;target_.resize(width_,height_);}
  prepareHistory();display_=&target_;target_.bind();glViewport(0,0,width_,height_);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_FRAMEBUFFER_SRGB);
  bind(animate_?clock_+float(frame.time_seconds):clock_);
  if(running_){
   GLuint q=0;glGenQueries(1,&q);glBeginQuery(GL_TIME_ELAPSED,q);triangle_.draw();glEndQuery(GL_TIME_ELAPSED);GLuint64 ns=0;glGetQueryObjectui64v(q,GL_QUERY_RESULT,&ns);glDeleteQueries(1,&q);
   if(caseFrame_>=warm_){double ms=double(ns)/1e6;durations_.push_back(ms);const auto& c=cases_[caseIndex_];raw_<<pathLabel(c)<<','<<ids[c.method]<<','<<durations_.size()-1<<','<<ms<<'\n';}
   ++caseFrame_;
  }else{timer_.begin();triangle_.draw();timer_.end();}
  accumulate();
  if(capture_){fs::create_directories("results/captures");captureFiles(fs::path("results/captures")/(std::string(ids[mode_])+"-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())));capture_=false;}
  gfx::research::Framebuffer::bind_default();glNamedFramebufferReadBuffer(display_->id(),GL_COLOR_ATTACHMENT0);glBlitNamedFramebuffer(display_->id(),0,0,0,width_,height_,0,0,frame.width,frame.height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
  if(running_&&caseFrame_>=warm_+samples_)finishCase();
 }
 std::vector<float> readLinear(){target_.bind();glReadBuffer(GL_COLOR_ATTACHMENT1);std::vector<float> pixels(size_t(width_)*height_*3);glReadPixels(0,0,width_,height_,GL_RGB,GL_FLOAT,pixels.data());glReadBuffer(GL_COLOR_ATTACHMENT0);return pixels;}
 void captureFiles(const fs::path& stem){(running_?target_:*display_).bind();glReadBuffer(GL_COLOR_ATTACHMENT0);cloud_png::save(stem.string()+".png",width_,height_);gfx::research::Framebuffer::bind_default();}
 void start(int profile){
  if(running_)return;cases_.clear();reference_.clear();vol_={};optSteps_=16;refSteps_=512;refLight_=32;jitter_=0;phase_=0;clock_=12;animate_=false;fov_=60;yaw_=45;aal_=.10f;
  warm_=profile==1?2:5;samples_=profile==1?5:profile==2?30:60;
  std::vector<int> angles=profile==1?std::vector<int>{30}:std::vector<int>{0,30,60,90};
  std::vector<int> t=profile==1?std::vector<int>{2}:profile==2?std::vector<int>{0,2,3,5}:std::vector<int>{0,1,2,3,4,5};
  std::vector<float> cover=profile==3?std::vector<float>{.30f,.55f,.78f}:std::vector<float>{.55f};
  std::vector<int> budgets=profile==3?std::vector<int>{64,128}:std::vector<int>{64};
  int w=profile==1?640:profile==2?1280:1920,h=profile==1?360:profile==2?720:1080;
  for(int ti:t)for(int p:angles)for(float cov:cover)for(int b:budgets)for(int m:{9,10,0,1,2,3,4,5,6,7,8})cases_.push_back({m,ti,p,cov,b,w,h});
  output_=options_.output.empty()?fs::path("results")/("benchmark-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())):options_.output;
  if(fs::exists(output_/"timing_raw.csv"))throw std::runtime_error("Output already exists: "+output_.string());
  fs::create_directories(output_/"captures");{std::ofstream state(output_/"status.txt");state<<"incomplete\n";}raw_.open(output_/"timing_raw.csv");summary_.open(output_/"timing_summary.csv");quality_.open(output_/"quality.csv");
  if(!raw_||!summary_||!quality_)throw std::runtime_error("Cannot open results");
  raw_<<"case,method,sample,gpu_ms\n";summary_<<"case,method,width,height,samples,mean_ms,median_ms,p95_ms,pass_fps\n";quality_<<"case,method,reference,linear_rmse,display_psnr_db,nonfinite_values\n";
  raw_<<std::setprecision(10);summary_<<std::setprecision(10);quality_<<std::setprecision(10);
  std::ofstream manifest(output_/"manifest.txt");manifest<<"version=0.1.0\nprofile="<<profile<<"\nGL_VENDOR="<<glString(GL_VENDOR)<<"\nGL_RENDERER="<<glString(GL_RENDERER)<<"\nGL_VERSION="<<glString(GL_VERSION)<<"\ncompiler="<<GFX_RESEARCH_COMPILER<<"\nbuild="<<GFX_RESEARCH_BUILD_TYPE<<"\nview_base=64_or_128\nlight_base=6\nopt_steps=16\nreference=512x32\nreference2=1024x64\naal_tolerance=0.10\njitter=off\ncoverage=see_case\nscale=1.25\ndensity=2\ndepth=0.28\nweather=0.2\nfov=60\nyaw=45\ntime=12\nnoise=deterministic_128_RG8\nwarmup="<<warm_<<"\nsamples="<<samples_<<"\n";
  caseIndex_=0;running_=true;status_="Running";applyCase();
 }
 void applyCase(){const auto& c=cases_[caseIndex_];mode_=c.method;pitch_=float(c.pitch);setTime(c.time);vol_.coverage=c.coverage;vol_.cloudSamples=c.budget;vol_.lightSamples=6;width_=c.w;height_=c.h;target_.resize(width_,height_);caseFrame_=0;durations_.clear();if(mode_==9)reference_.clear();glFinish();}
 void stop(bool completed){running_=false;raw_.close();summary_.close();quality_.close();status_=completed?"Completed: "+output_.string():"Stopped: "+output_.string();std::ofstream state(output_/"status.txt");state<<(completed?"complete":"incomplete")<<"\ncompleted_cases="<<caseIndex_<<"\nplanned_cases="<<cases_.size()<<'\n';if(options_.profile){if(!completed)failed_=true;close();}}
 static double display(double x){x=std::max(0.,x);return std::pow(x/(1.+x),1./2.2);}
 void finishCase(){
  const auto c=cases_[caseIndex_];const std::string label=pathLabel(c),method=ids[c.method];auto image=readLinear();
  if(mode_==9)reference_=image;
  double mse=0.,dmse=0.;size_t invalid=0;
  for(size_t i=0;i<image.size();++i){double a=image[i],b=reference_[i];if(!std::isfinite(a)||!std::isfinite(b)){++invalid;continue;}mse+=(a-b)*(a-b);double d=display(a)-display(b);dmse+=d*d;}
  if(invalid)failed_=true;
  double rmse=std::sqrt(mse/std::max<size_t>(1,image.size()-invalid));double psnr=dmse==0.?INFINITY:10.*std::log10(double(std::max<size_t>(1,image.size()-invalid))/dmse);
  quality_<<label<<','<<method<<",ref,"<<rmse<<','<<psnr<<','<<invalid<<'\n';
  captureFiles(output_/"captures"/(label+"-"+method));
  // Preserve unquantized radiance for quality analysis; full profile can be large.
  if(c.method==9||c.method==10){std::ofstream hdr(output_/"captures"/(label+"-"+method+".rgbf"),std::ios::binary);hdr.write(reinterpret_cast<const char*>(image.data()),std::streamsize(image.size()*sizeof(float)));}
  auto sorted=durations_;std::sort(sorted.begin(),sorted.end());double mean=std::accumulate(sorted.begin(),sorted.end(),0.)/sorted.size();double median=sorted.size()%2?sorted[sorted.size()/2]:.5*(sorted[sorted.size()/2-1]+sorted[sorted.size()/2]);double p95=sorted[std::min(sorted.size()-1,size_t(std::ceil(.95*sorted.size())-1))];
  summary_<<label<<','<<method<<','<<c.w<<','<<c.h<<','<<sorted.size()<<','<<mean<<','<<median<<','<<p95<<','<<1000./std::max(mean,1e-9)<<'\n';raw_.flush();summary_.flush();quality_.flush();
  std::cout<<label<<' '<<method<<' '<<mean<<" ms ("<<caseIndex_+1<<'/'<<cases_.size()<<")\n";
  ++caseIndex_;if(caseIndex_==cases_.size())stop(true);else applyCase();
 }
        static void configureGui() {
            auto& style=ImGui::GetStyle();
            style.WindowPadding={9,8}; style.FramePadding={5,3}; style.ItemSpacing={7,4};
            style.WindowRounding=2; style.FrameRounding=1; style.GrabRounding=1;
            style.WindowBorderSize=1; style.FrameBorderSize=0; style.ScrollbarSize=11;
            style.Colors[ImGuiCol_WindowBg]={0.035f,0.043f,0.055f,0.97f};
            style.Colors[ImGuiCol_TitleBg]={0.09f,0.16f,0.25f,1};
            style.Colors[ImGuiCol_TitleBgActive]={0.16f,0.28f,0.43f,1};
            style.Colors[ImGuiCol_FrameBg]={0.10f,0.16f,0.24f,1};
            style.Colors[ImGuiCol_FrameBgHovered]={0.16f,0.26f,0.38f,1};
            style.Colors[ImGuiCol_FrameBgActive]={0.20f,0.33f,0.49f,1};
            style.Colors[ImGuiCol_SliderGrab]={0.24f,0.53f,0.85f,1};
            style.Colors[ImGuiCol_CheckMark]={0.40f,0.72f,1,1};
            style.Colors[ImGuiCol_Header]={0.12f,0.23f,0.36f,1};
            style.Colors[ImGuiCol_HeaderHovered]={0.20f,0.36f,0.54f,1};
        }

        static bool parameterTable(const char* id) {
            if (!ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchProp)) return false;
            ImGui::TableSetupColumn("Parameter",ImGuiTableColumnFlags_WidthStretch,0.42f);
            ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,0.58f);
            return true;
        }

        template<class Control> static void parameter(const char* label,Control&& control) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1); ImGui::PushID(label);
            ImGui::SetNextItemWidth(-1.0f); control(); ImGui::PopID();
        }

 void on_gui(const gfx::research::FrameInfo&) override{
  const auto& io=ImGui::GetIO();if(!io.WantTextInput&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_H,false))visible_=!visible_;
  if(!io.WantTextInput&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F,false)){
   if(!fullscreenActive_){glfwGetWindowPos(window(),&windowedX_,&windowedY_);glfwGetWindowSize(window(),&windowedWidth_,&windowedHeight_);auto* monitor=glfwGetPrimaryMonitor();const auto* vm=monitor?glfwGetVideoMode(monitor):nullptr;if(vm){glfwSetWindowMonitor(window(),monitor,0,0,vm->width,vm->height,vm->refreshRate);fullscreenActive_=true;}}
   else{glfwSetWindowMonitor(window(),nullptr,windowedX_,windowedY_,windowedWidth_,windowedHeight_,0);fullscreenActive_=false;}
  }
  if(!visible_)return;auto* v=ImGui::GetMainViewport();ImGui::SetNextWindowPos({v->WorkPos.x+12,v->WorkPos.y+12},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize({448,std::min(800.f,v->WorkSize.y-24)},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSizeConstraints({380,320},{650,std::max(320.f,v->WorkSize.y-24)});
  if(!ImGui::Begin("Cloud Research")){ImGui::End();return;}
  if(parameterTable("status")){parameter("Frame rate",[&]{ImGui::Text("%.0f FPS (%.2f ms)",fps(),cpu_frame_milliseconds());});parameter("GPU cloud pass",[&]{if(running_&&!durations_.empty())ImGui::Text("%.3f ms",durations_.back());else ImGui::Text("%.3f ms",timer_.milliseconds());});parameter("Render target",[&]{ImGui::Text("%d x %d",width_,height_);});ImGui::EndTable();}
  if(running_){ImGui::SeparatorText("Benchmark running");ImGui::Text("Case %zu / %zu",caseIndex_+1,cases_.size());ImGui::TextWrapped("%s | %s",pathLabel(cases_[caseIndex_]).c_str(),methods[mode_]);ImGui::ProgressBar(float(caseIndex_)/float(cases_.size()),{-1,0});if(ImGui::Button("Stop benchmark",{-1,0}))stop(false);ImGui::End();return;}
  if(ImGui::BeginTabBar("tabs")){
   if(ImGui::BeginTabItem("Clouds")){
    if(parameterTable("clouds")){
     parameter("Method",[&]{ImGui::Combo("##method",&mode_,methods,11);});
     parameter("Coverage",[&]{ImGui::SliderFloat("##v",&vol_.coverage,0.f,.95f);});
     parameter("Density",[&]{ImGui::SliderFloat("##v",&vol_.density,.1f,4.f);});
     parameter("Scale",[&]{ImGui::SliderFloat("##v",&vol_.scale,.25f,4.f);});
     parameter("Thickness",[&]{ImGui::SliderFloat("##v",&vol_.depth,.08f,.6f);});
     parameter("Erosion",[&]{ImGui::SliderFloat("##v",&vol_.detail,0.f,1.f);});
     parameter("Cloud type",[&]{ImGui::SliderFloat("##v",&vol_.type,0.f,1.f);});
     parameter("Weather",[&]{ImGui::SliderFloat("##v",&vol_.weather,0.f,.5f);});
     parameter("Self shadow",[&]{ImGui::SliderFloat("##v",&vol_.selfShadow,0.f,2.f);});
     parameter("Anisotropy",[&]{ImGui::SliderFloat("##v",&vol_.anisotropy,0.f,.9f);});
     parameter("Multiple scattering",[&]{ImGui::SliderFloat("##v",&vol_.multipleScattering,0.f,1.f);});
     parameter("Powder",[&]{ImGui::SliderFloat("##v",&vol_.powder,0.f,1.f);});
     parameter("View steps",[&]{ImGui::SliderInt("##v",&vol_.cloudSamples,8,256);});
     parameter("Light steps",[&]{ImGui::SliderInt("##v",&vol_.lightSamples,1,16);});
     parameter("OPT16 steps",[&]{ImGui::SliderInt("##v",&optSteps_,4,128);});
     parameter("OPT16 jitter",[&]{bool j=jitter_!=0;if(ImGui::Checkbox("##v",&j))jitter_=j?1:0;});
     parameter("Static temporal",[&]{ImGui::Checkbox("##v",&temporal_);});
     parameter("History samples",[&]{ImGui::Text("%d / 64",historyCount_);});
     parameter("Jitter phase",[&]{ImGui::SliderInt("##v",&phase_,0,63);});
     parameter("AAL tolerance",[&]{ImGui::SliderFloat("##v",&aal_,0.f,.4f);});
     parameter("Reference steps",[&]{ImGui::SliderInt("##v",&refSteps_,128,1024);});
     parameter("Reference light",[&]{ImGui::SliderInt("##v",&refLight_,16,64);});
     ImGui::EndTable();}
    ImGui::TextWrapped("Nubis = research reconstruction; FST25 = conservative texture-bound adaptation; OPT16 = low-step exact integration; AAL = experimental lighting. Static temporal accumulates only frozen views; no motion reprojection.");
    if(ImGui::Button("Reset cloud parameters",{-1,0})){vol_={};optSteps_=16;refSteps_=512;refLight_=32;aal_=.1f;jitter_=0;}
    ImGui::EndTabItem();}
   if(ImGui::BeginTabItem("Sky")){
    const char* names[]={"Dawn","Morning","Noon","Sunset","Twilight","Night"};
    if(parameterTable("sky")){
     parameter("Time of day",[&]{int i=time_;if(ImGui::Combo("##v",&i,names,6))setTime(i);});
     parameter("Sun elevation",[&]{ImGui::SliderFloat("##v",&sun_,-30.f,89.f);});
     parameter("Sun azimuth",[&]{ImGui::SliderFloat("##v",&azimuth_,0.f,360.f);});
     parameter("Turbidity",[&]{ImGui::SliderFloat("##v",&vol_.turbidity,2.f,8.f);});
     parameter("Exposure",[&]{ImGui::SliderFloat("##v",&vol_.atmosphereExposure,.005f,.15f);});
     parameter("Cloud time",[&]{ImGui::SliderFloat("##v",&clock_,0.f,60.f);});
     parameter("Animate clouds",[&]{ImGui::Checkbox("##v",&animate_);});ImGui::EndTable();}ImGui::EndTabItem();}
   if(ImGui::BeginTabItem("Camera")){
    if(parameterTable("camera")){parameter("Elevation",[&]{ImGui::SliderFloat("##v",&pitch_,0.f,90.f);});parameter("Azimuth",[&]{ImGui::SliderFloat("##v",&yaw_,0.f,360.f);});parameter("Vertical FOV",[&]{ImGui::SliderFloat("##v",&fov_,30.f,100.f);});ImGui::EndTable();}
    for(int p:{0,30,60,90}){if(p)ImGui::SameLine();std::string label=std::to_string(p)+" deg";if(ImGui::Button(label.c_str()))pitch_=float(p);}
    ImGui::TextWrapped("Fixed observer at ground level; no meshes or terrain. 0 degrees = horizon, 90 degrees = zenith.");ImGui::EndTabItem();}
   if(ImGui::BeginTabItem("Tests")){
    if(ImGui::Button("Capture current frame",{-1,0}))capture_=true;
    if(ImGui::Button("Smoke benchmark (640 x 360)",{-1,0}))start(1);
    if(ImGui::Button("Quick benchmark (1280 x 720)",{-1,0}))start(2);
    if(ImGui::Button("Full benchmark (1920 x 1080)",{-1,0}))start(3);
    ImGui::TextWrapped("%s",status_.c_str());ImGui::TextWrapped("All tests freeze time, disable temporal accumulation and jitter and write timings, quality metrics, PNG captures and reference radiance to results/. Full profile is large and can take substantial GPU time.");ImGui::EndTabItem();}
   ImGui::EndTabBar();}
  ImGui::End();
 }
};
Options parse(int argc,char** argv){Options o;for(int i=1;i<argc;++i){std::string s=argv[i];if(s=="--smoke")o.profile=1;else if(s=="--quick")o.profile=2;else if(s=="--full")o.profile=3;else if(s=="--output"&&i+1<argc)o.output=argv[++i];else throw std::runtime_error("Usage: gfx-research-clouds [--smoke|--quick|--full] [--output PATH]");}return o;}
}
int main(int argc,char** argv){try{return Clouds(parse(argc,argv)).execute();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
