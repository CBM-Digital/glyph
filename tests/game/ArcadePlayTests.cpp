#include "game/GameHost.h"
#include "profile/ProfileStore.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace glyph;
using script::Value;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::string source(const std::string& slug){std::ifstream f(std::filesystem::path(GLYPH_SOURCE_DIR)/"examples/arcade"/slug/"game.glyph");std::ostringstream s;s<<f.rdbuf();return s.str();}
Value field(const Value& v,script::VM& vm,const char* name){return v.map->at(vm.interner().intern(name));}
double number(const Value& v,script::VM& vm,const char* name){return field(v,vm,name).number;}
std::string phase(game::GameHost& h){return std::string(h.vm().interner().resolve(field(h.state(),h.vm(),":phase").id));}
void tick(game::GameHost& h){h.tick(game::GameHost::fixedDt);h.audio().flush();}
void testInput(){
 const std::string code=R"((game test :title "Input" :size [100 100] :initial {:n 0 :r 0} :update update :view view)
 (defn update [dt s] (assoc s :n (+ (:n s) (if (pressed? :tap) 1 0)) :r (+ (:r s) (if (released? :tap) 1 0))))
 (defn view [s] empty))";
 for(double hz:{30.,60.,120.,144.}){
  game::GameHost h;h.loadSource(code);auto tap=h.vm().interner().intern(":tap");
  for(int i=0;i<180;++i){h.input().beginFrame();if(i==0)h.input().setActionDown(tap,true);if(i==2)h.input().setActionDown(tap,false);h.tick(1/hz);}
  require(number(h.state(),h.vm(),":n")==1,"press must be consumed once at any refresh rate");
  require(number(h.state(),h.vm(),":r")==1,"release must be consumed once at any refresh rate");
 }
 game::GameHost h;h.loadSource(code);auto tap=h.vm().interner().intern(":tap");
 for(int i=0;i<3;++i){h.input().setActionDown(tap,true);h.input().setActionDown(tap,false);}
 h.tick(game::GameHost::fixedDt*6.1);
 require(number(h.state(),h.vm(),":n")==3 && number(h.state(),h.vm(),":r")==3,"rapid taps preserve transition order");
 h.input().setActionDown(tap,true);h.setPaused(true);h.setPaused(false);tick(h);
 require(number(h.state(),h.vm(),":n")==3,"pause clears stale transitions");
}
void testProfile(){
 auto root=std::filesystem::temp_directory_path()/"glyph-expedition-profile-tests";
 std::filesystem::create_directories(root);auto path=root/"profile.glyphdata";
 std::filesystem::remove(path);std::filesystem::remove(path.string()+".bak");
 script::VM vm;auto data=vm.evalSource(R"({:message "line\nquote\"slash\\" :nested [0.123456789012345 :fish true nil]})");
 require(script::valueEquals(data,profile::decode(profile::encode(data,vm.interner()),vm.interner())),"profile round trips plain data precisely");
 {profile::ProfileStore store(path);require(store.set(":test",data,vm.interner()),"profile saves");}
 {profile::ProfileStore store(path);require(script::valueEquals(data,store.get(":test",Value::nil(),vm.interner())),"profile survives restart");}
 {std::ofstream f(path);f<<"broken data";}
 {profile::ProfileStore store(path);require(script::valueEquals(data,store.get(":test",Value::nil(),vm.interner())),"corrupt profile recovers backup");}
 std::filesystem::remove(path);
 {profile::ProfileStore store(path);require(script::valueEquals(data,store.get(":test",Value::nil(),vm.interner())),"missing primary profile recovers backup");}
 bool rejected=false;try{profile::decode("(profile/set :owned true)",vm.interner());}catch(...){rejected=true;}
 require(rejected,"profile is not executable script");
 profile::ProfileStore records;records.complete(vm.evalSource("{:game :comet :score 500 :medals 2}"),vm.interner());
 records.complete(vm.evalSource("{:game :comet :score 100 :medals 1}"),vm.interner());
 require(number(records.get(":comet-record",Value::nil(),vm.interner()),vm,":score")==500,"lower scores do not replace best");
 std::filesystem::remove_all(root);
}
void testRandom(){
 std::weak_ptr<script::Env> oldGlobals;
 {script::VM transient;transient.evalSource("(defn f [] 1)");oldGlobals=transient.globals();}
 require(oldGlobals.expired(),"unloading a cabinet releases its global closure cycle");

 script::VM a,b;a.seedRandom(123);b.seedRandom(123);
 for(int i=0;i<100;++i)require(a.randomUnit()==b.randomUnit(),"VM random streams reproduce independently");
 a.seedRandom(123);b.seedRandom(123);a.randomUnit();b.randomUnit();a.randomUnit();
 script::VM c;c.seedRandom(123);c.randomUnit();require(b.randomUnit()==c.randomUnit(),"one game does not perturb another stream");
}
struct Body{double x,y,vx,vy;};
void physics(Body& p,double gx,double gy,double force){double dx=gx-p.x,dy=gy-p.y,d=std::max(1.,std::hypot(dx,dy)),a=force/(d*d+900);p.vx+=dx/d*a/60;p.vy+=dy/d*a/60;p.x+=p.vx/60;p.y+=p.vy/60;}
void testComet(){
 game::GameHost h;h.loadSource(source("comet-courier"));auto& vm=h.vm();
 auto contracts=vm.evalSource("contracts");
 for(int index=0;index<6;++index){
  auto l=contracts.vector->at(index);
  auto n=[&](const char* key){return number(l,vm,key);};
  bool fragile=std::string(vm.interner().resolve(field(l,vm,":cargo").id))==":fragile";
  bool found=false;double foundAngle=0,foundPower=0,foundBrake=0;int ticks=0;
  for(double power=.1;power<=1.001&&!found;power+=.025)for(double angle=-85;angle<=85&&!found;angle+=1){
   for(double brakeDistance: {0.,65.,100.,150.}){
    Body p{n(":sx"),n(":sy"),std::cos(angle*3.141592653589793/180)*(150+power*220),std::sin(angle*3.141592653589793/180)*(150+power*220)};bool brakeUsed=false; bool touchedRing=false;
    for(int t=0;t<540;++t){
     if(!brakeUsed&&brakeDistance>0&&std::hypot(p.x-n(":tx"),p.y-n(":ty"))<brakeDistance){p.vx*=.46;p.vy*=.46;brakeUsed=true;}
     physics(p,n(":gx"),n(":gy"),n(":force"));
     if(std::hypot(p.x-n(":rx"),p.y-n(":ry"))<22)touchedRing=true;
     if(std::hypot(p.x-n(":gx"),p.y-n(":gy"))<n(":radius")+7)break;
     if(std::hypot(p.x-n(":tx"),p.y-n(":ty"))<25){if(touchedRing && (!fragile||std::hypot(p.vx,p.vy)<=190)){found=true;foundAngle=angle;foundPower=power;foundBrake=brakeDistance;ticks=t+1;}break;}
     if(p.x<8||p.x>472||p.y<75||p.y>315)break;
    }
    if(found)break;
   }
  }
  require(found,"each Comet contract must have a delivery trajectory through its bonus ring");
  std::ostringstream setup;setup<<"(launch (assoc (prepare (assoc initial :music true :contract "<<index<<")) :angle "<<foundAngle<<" :power "<<foundPower<<"))";
  h.setState(vm.evalSource(setup.str()));bool brakeUsed=false;auto tap=vm.interner().intern(":tap");
  for(int t=0;t<ticks+5&&phase(h)==":fly";++t){
   if(!brakeUsed&&foundBrake>0&&std::hypot(number(h.state(),vm,":x")-n(":tx"),number(h.state(),vm,":y")-n(":ty"))<foundBrake){h.input().setActionDown(tap,true);brakeUsed=true;}else h.input().setActionDown(tap,false);
   tick(h);
  }
  require(phase(h)==":delivered","solution must succeed in real Glyph update, not just the solver");
  require(number(h.state(),vm,":rings")==1,"gold delivery ring must be attainable");
  std::cout<<"Comet contract "<<index+1<<" reachable: angle="<<foundAngle<<" power="<<foundPower<<" brake-distance="<<foundBrake<<'\n';
  h.input().clear();
 }
 h.setState(vm.evalSource("(prepare initial)"));
 auto preview=field(h.state(),vm,":preview");auto p=vm.evalSource("(launched (prepare initial))");
 auto fn=*vm.globals()->lookup(vm.interner().intern("physics-step"));auto l=contracts.vector->at(0);
 p=vm.call(fn,{p,l});require(preview.vector->size()>0,"preview contains points");
 require(std::abs(preview.vector->at(0).vector->at(0).number-number(p,vm,":x"))<1e-9,"preview uses identical physics step");
}
void testAlpine(){
 game::GameHost h;h.loadSource(source("ski-slalom"));auto& vm=h.vm();h.setState(vm.evalSource("(start initial)"));
 auto move=vm.interner().intern(":move-x");
 auto gateX=*vm.globals()->lookup(vm.interner().intern("gate-x"));
 for(int t=0;t<60*95&&phase(h)==":play";++t){
  int gate=static_cast<int>(number(h.state(),vm,":gate"));
  double target=gate<24?vm.call(gateX,{Value::numberValue(gate)}).number:240;
  double steering=std::clamp((target-number(h.state(),vm,":x"))*.035-number(h.state(),vm,":vx")*.014,-1.,1.);
  auto obstacleY=*vm.globals()->lookup(vm.interner().intern("obstacle-y"));
  auto obstacleX=*vm.globals()->lookup(vm.interner().intern("obstacle-x"));
  bool jump=false;
  for(int j=std::max(0,gate-2);j<std::min(24,gate+1);++j){
    const double dy=vm.call(obstacleY,{Value::numberValue(j)}).number-number(h.state(),vm,":y");
    const double dx=std::abs(vm.call(obstacleX,{Value::numberValue(j)}).number-number(h.state(),vm,":x"));
    if(dy>25&&dy<80&&dx<65)jump=true;
  }
  h.input().setActionDown(vm.interner().intern(":confirm"),jump);
  h.input().setAxis(move,static_cast<float>(steering));tick(h);
  if(t%30==0)h.renderView();
 }
 if(phase(h)!=":win")std::cerr<<script::valueToString(h.state(),vm.interner())<<"\n";
 require(phase(h)==":win","Alpine course is finishable using real steering");
 require(number(h.state(),vm,":passed")==24,"all gates are attainable");
 require(number(h.state(),vm,":time")>=60&&number(h.state(),vm,":time")<=90,"Alpine first course lasts 60-90 seconds");
 require(number(h.state(),vm,":score")>=8000,"gold score is attainable without exploits");
 std::cout<<"Alpine course: "<<number(h.state(),vm,":time")<<" seconds, "<<number(h.state(),vm,":score")<<" points\n";
}
void testCometRunAndCheckpoint(){
 profile::ProfileStore progress;
 game::GameHost h;h.setProfile(&progress);h.loadSource(source("comet-courier"));auto& vm=h.vm();
 h.setState(vm.evalSource("(begin-run initial)"));
 const double angles[]={-34,2,-31};const double brakeDistances[]={150,0,150};
 for(int stage=0;stage<3;++stage){
  require(number(h.state(),vm,":contract")==stage,"next delivery advances to a new contract");
  auto state=*h.state().map;state[vm.interner().intern(":angle")]=Value::numberValue(angles[stage]);state[vm.interner().intern(":power")]=Value::numberValue(.1);
  h.setState(vm.call(*vm.globals()->lookup(vm.interner().intern("launch")),{Value::mapValue(std::move(state))}));
  const auto l=vm.call(*vm.globals()->lookup(vm.interner().intern("level")),{h.state()});bool used=false;
  auto tap=vm.interner().intern(":tap");
  for(int t=0;t<540 && phase(h)==":fly";++t){
   bool brake=!used && brakeDistances[stage]>0 && std::hypot(number(h.state(),vm,":x")-number(l,vm,":tx"),number(h.state(),vm,":y")-number(l,vm,":ty"))<brakeDistances[stage];
   h.input().setActionDown(tap,brake);used=used||brake;tick(h);
  }
  h.input().clear();
  if(stage<2){
   require(phase(h)==":delivered","route pauses after each successful delivery");
   // Resume through a newly loaded VM, exercising interner-independent checkpoint data.
   game::GameHost resumed;resumed.setProfile(&progress);resumed.loadSource(source("comet-courier"));
   resumed.input().setActionDown(resumed.vm().interner().intern(":secondary"),true);tick(resumed);
   require(phase(resumed)==":aim" && number(resumed.state(),resumed.vm(),":contract")==stage+1,"checkpoint resumes the next delivery in a new VM");
   require(number(resumed.state(),resumed.vm(),":rings")==stage+1,"checkpoint retains delivered bonus rings");
   h.input().setActionDown(tap,true);tick(h);h.input().setActionDown(tap,false);tick(h);
  }
 }
 require(phase(h)==":win" && number(h.state(),vm,":hull")==3,"three deliveries finish a perfect expedition run");
 require(progress.get(":comet-checkpoint",Value::nil(),vm.interner()).kind==script::ValueKind::Nil,"completed route clears its checkpoint");
 require(number(progress.get(":comet-record",Value::nil(),vm.interner()),vm,":medals")==3,"completed perfect route saves gold");
 h.input().setActionDown(vm.interner().intern(":confirm"),true);tick(h);
 require(phase(h)==":aim" && number(h.state(),vm,":stage")==0,"one action restarts a completed run in one tick");
}
void testPrototypeFixes(){
 game::GameHost h;h.loadSource(source("harbor-switchyard"));auto& vm=h.vm();
 h.setState(vm.evalSource("(assoc initial :x0 100 :x1 110 :x2 -300 :lane0 1 :lane1 1)"));
 for(int i=0;i<3;++i)tick(h);
 require(number(h.state(),vm,":strikes")==1,"one collision costs one strike");
 game::GameHost star;star.loadSource(source("starforge-spin"));
 auto x=star.vm().evalSource("[(shield-color (assoc initial :rot 0 :pa 0)) (shield-color (assoc initial :rot 0 :pa 180))]");
 require(x.vector->at(0).number!=x.vector->at(1).number,"Starforge contact angle selects the segment");
 for(int rotation:{0,17,90,359})for(int quadrant=0;quadrant<4;++quadrant)for(double edge:{-44.999,0.,44.999}){
  std::ostringstream expression;expression<<"(shield-color (assoc initial :rot "<<rotation<<" :pa "<<rotation+quadrant*90+edge<<"))";
  require(star.vm().evalSource(expression.str()).number==quadrant,"Starforge resolves all rotated segments and near-boundary angles");
 }
 require(star.vm().evalSource("(shield-color (assoc initial :rot 0 :pa 45))").number==1,"Starforge exact boundary belongs to next segment");
}
int main(){try{testInput();testProfile();testRandom();testComet();testCometRunAndCheckpoint();testAlpine();testPrototypeFixes();std::cout<<"Arcade play tests passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
