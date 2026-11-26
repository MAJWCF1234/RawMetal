// Used by the game and the builder's independent renderer translation units.
void Game::advanceDebugNavigation(ai::NodeGraph& graph) const {
 if(graph.complete)return;
 const World& world=chunkWorld(level());
 const std::vector<char> everyDoor(world.doors().size(),1);
 World::DoorPlan open(world,everyDoor);
 if(!graph.building){
  std::vector<ai::AuthoredHint> authored;
  const auto map=size_t(level());
  if(m_worldId==WorldId::Custom&&m_customCampaign&&map<m_customCampaign->maps.size()&&m_customCampaign->maps[map])
   for(const auto& hint:m_customCampaign->maps[map]->navHints)
    if(hint.hint>=0&&hint.hint<=1)
     authored.push_back({hint.position,hint.z,hint.hint?ai::NodeHint::Cover:ai::NodeHint::Path});
  ai::beginNodeGraph(graph,world,Player::StandingHeight,authored);
 }
 const auto walk=[&](Vec2 from,float feet,Vec2 to,float height,float* end){
  return aiWalkSegment(world,from,feet,to,height,end);
 };
 constexpr size_t pairBudget=1024;
 ai::advanceNodeGraph(graph,walk,pairBudget);
}

void SoftwareRenderer::prepareMapDiagnostics(const Game& game) {
 using namespace debuglayer;
 const unsigned layers=(m_environmentInspection?0u:visible)|(m_editorMapDiagnostics?MapDiagnostics:0u);
 if(!(layers&(Navigation|Issues))||game.titleScreen()||game.loading())return;
 const unsigned revision=game.staticRevision(game.level());
 if(m_diagnosticsGame!=game.renderSerial()||m_diagnosticsRevision!=revision||m_diagnosticsLevel!=game.level()){
  m_diagnosticsGame=game.renderSerial();
  m_diagnosticsRevision=revision;
  m_diagnosticsLevel=game.level();
  m_diagnosticsAudited=false;
  m_debugNavigation={};
  m_mapIssues.clear();
 }
 if((layers&Issues)&&!m_diagnosticsAudited){
  m_mapIssues=inspectMapHealth(game);
  m_diagnosticsAudited=true;
 }
 if((layers&Navigation)&&!m_debugNavigation.complete)game.advanceDebugNavigation(m_debugNavigation);
}
