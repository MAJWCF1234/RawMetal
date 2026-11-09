#include "../NativeEditor.cpp"
#include <iostream>

static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
#include "AdaptiveDressing.inc"
int main(){try{
  Editor e;e.root=fs::current_path();e.backgroundTest=true;e.loadSchemas();NativeDocument::verify=true;
  auto reset=[&]{e.doc.newCampaign("Creation regression");e.doc.addRoom("Room");e.map=e.layer=0;e.history.clear();e.future.clear();};
  auto exists=[&](const std::string& line){return std::find(e.doc.lines.begin(),e.doc.lines.end(),line)!=e.doc.lines.end();};
  auto last=[&](const char* kind){return e.doc.lines[e.doc.records(kind,0).back()];};
  for(int model:{retro::model::lab_clean_bench,retro::model::lab_exec_desk})for(int side=0;side<4;++side){reset();
    float x=side<2?8.f:side==2?3.f:20.f,y=side<2?(side==0?3.f:20.f):8.f;
    if(!e.wallPiece(model,x,y,side)){std::cerr<<"model "<<model<<" side "<<side<<" size "<<retro::model::size[model][0]<<","<<retro::model::size[model][1]<<","<<retro::model::size[model][2]<<" floor "<<e.layerElevation()<<" top "<<e.creationCeiling()<<'\n';throw std::runtime_error("Wall candidate was unexpectedly refused");}
    auto f=NativeDocument::fields(last("FIXTURE"));auto b=*e.creationBox(f);
    float gap=side==0?b.y-b.halfY()-y:side==1?y-b.y-b.halfY():side==2?b.x-b.halfX()-x:x-b.x-b.halfX();
    check(gap>=.079f,"A rotated model crosses its wall");
  }
  reset();std::vector<int> cells;
  for(int y=1;y<23;++y)for(int x=1;x<23;++x)e.doc.paint(0,0,x,y,'#');
  for(int y=3;y<=8;++y)for(int x=3;x<=8;++x)if(!(x>=5&&y<=6)){cells.push_back(y*24+x);e.doc.paint(0,0,x,y,'.');}
  e.doc.add({"FIXTURE","0","188","4","7.5","0","1.18","0.56","0.81","0","1","0.65","1"});auto user=last("FIXTURE");
  e.furnishCells(cells,3);check(exists(user),"Autodress changed user furniture");
  size_t count=e.doc.records("FIXTURE",0).size();check(count>1,"Dressing placed no automatic furniture");
  auto first=e.doc.text();e.furnishCells(cells,3);
  check(e.doc.records("FIXTURE",0).size()==count,"Repeated dressing grew the furniture count");
  e.furnishCells(cells,0);check(exists(user),"Theme change altered user furniture");
  for(auto i:e.doc.records("STRUCT",0)){auto f=NativeDocument::fields(e.doc.lines[i]);
    for(int y=int(std::stof(f[3]));y<int(std::stof(f[5]));++y)for(int x=int(std::stof(f[2]));x<int(std::stof(f[4]));++x)
      check(std::find(cells.begin(),cells.end(),y*24+x)!=cells.end(),"Autodress floor escapes its room mask");}
  auto fixtures=e.doc.records("FIXTURE",0);
  for(size_t j=0;j<fixtures.size();++j){auto a=*e.creationBox(NativeDocument::fields(e.doc.lines[fixtures[j]]));
    for(size_t k=j+1;k<fixtures.size();++k)check(!creation::overlaps(a,*e.creationBox(NativeDocument::fields(e.doc.lines[fixtures[k]]))),"Dressing overlaps furniture");}
  size_t owned=0;for(auto i:fixtures)if(e.doc.automatic(i,0)){owned=i;break;}check(owned!=0,"Generated furniture lacks ownership");
  auto f=NativeDocument::fields(e.doc.lines[owned]);f[11<f.size()?11:10]="0";e.doc.replace(owned,f);auto edited=e.doc.lines[owned];
  check(!e.doc.automatic(owned),"Manual edit did not transfer ownership");e.furnishCells(cells,3);check(exists(edited),"Redressing removed a manually edited item");
  auto saved=e.doc.text();NativeDocument reopened;reopened.loadText(saved);check(reopened.text()==saved,"Ownership comments did not roundtrip");
  reset();e.autoDressingEnabled=false;e.tool=1;e.gestureNow(3,3,10,10);
  check(e.doc.records("FIXTURE",0).empty()&&e.doc.records("STRUCT",0).empty()&&e.doc.records("DOOR",0).empty()&&e.doc.records("LIGHT",0).size()==1,
    "Disabled autodressing still creates automatic content");e.autoDressingEnabled=true;
  reset();e.doc.add({"LAYER","0","1","Upper","5","0.25"});
  auto map=e.doc.records("MAP",0).front();f=NativeDocument::fields(e.doc.lines[map]);f[9]="10";e.doc.replace(map,f);
  e.doc.add({"LIGHT","0","5.5","5.5","2.5","0","0","1","8","3200"});auto lower=last("LIGHT");
  e.doc.add({"LIGHT","0","5.5","5.5","7.5","0","0","1","8","3200"});auto upper=last("LIGHT");
  e.doc.addAutomatic({"LIGHT","0","5.5","5.5","4.3","0","0","1","8","3200"},0);
  e.doc.addAutomatic({"LIGHT","0","5.5","5.5","9.3","0","0","1","8","3200"},1);auto upperAuto=last("LIGHT");
  e.lightRoom({5*24+5});check(exists(lower)&&exists(upper)&&exists(upperAuto),"Relight changed user content or another floor");
  for(int y=0;y<24;++y)for(int x=0;x<24;++x)e.doc.paint(0,0,x,y,'_');e.doc.paint(0,0,5,5,'.');
  count=e.doc.records("LIGHT",0).size();e.furnish();e.furnish();check(e.doc.records("LIGHT",0).size()==count,"Add lamps duplicated covered lights");
  reset();e.doc.add({"LAYER","0","1","Upper","5","0.25"});map=e.doc.records("MAP",0).front();f=NativeDocument::fields(e.doc.lines[map]);f[9]="10";e.doc.replace(map,f);
  e.doc.add({"FIXTURE","0","6","6.5","6.5","0","1.87","0.6","0.99","0","1"});auto hit=last("FIXTURE");
  e.doc.add({"FIXTURE","0","6","6.5","6.5","5","1.87","0.6","0.99","0","1"});auto upstairs=last("FIXTURE");
  auto undo=e.doc.text();e.tool=1;e.gestureNow(6,3,11,9);
  check(!exists(hit)&&exists(upstairs)&&e.doc.tile(0,0,6,6)=='#',"Drawn room walls did not win on the active floor only");
  check(e.history.size()==1&&e.history.back()==undo,"Room and object removal are not one undo step");e.doc.loadText(e.history.back());check(e.doc.text()==undo,"Undo did not restore removed content exactly");
  e.tool=3;e.remember();e.brushCell(6.5f,6.5f);check(!exists(hit)&&exists(upstairs),"Live wall brush did not remove intersecting content");
  reset();e.doc.add({"STRUCT","0","5","5","8","8","0","9","0","5"});
  e.clearCreationWalls({6*24+6});bool topKept=false;
  for(auto i:e.doc.records("STRUCT",0)){auto b=e.creationBox(NativeDocument::fields(e.doc.lines[i]));if(b&&b->bottom>=4.5f)topKept=true;}
  check(topKept,"Wall replacement destroyed structure above the active floor");
  // The engine, not only document checks, accepts the new ownership comments.
  e.session=e.root/L".build"/L"creation-regression";fs::create_directories(e.session);
  e.pack=e.session/L"working.rmc";e.control=e.session/L"control.txt";e.status=e.session/L"status.txt";
  check(e.validateDocument(),"Native engine rejected wall replacement output");
  verifyAdaptiveDressing(e);
  std::cout<<"PASS: 8 native-facing wall placements; irregular-room masks; no furniture overlap or repeat growth; user/legacy ownership; promotion on edit; persisted ownership; floor-scoped lighting; repeat-safe lamps; wall priority; brush parity; single-step undo; structure preservation\n";
  return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
