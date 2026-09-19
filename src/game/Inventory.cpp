#include "Game.h"
#include <fstream>
namespace retro {
void Game::updateInventory(const InputState& input){
 bool click=input.fire&&!m_inventoryClick,use=(input.use||input.menuAccept)&&!m_inventoryUse;
 m_inventoryClick=input.fire;m_inventoryUse=input.use||input.menuAccept;
 auto exists=[&](int item){return item==0|| (item==1?m_player.ammo>0:m_medkits>0);};
 auto activate=[&](){
  if(m_selectedItem==0){m_weaponEquipped=!m_weaponEquipped;m_holster=m_weaponEquipped&&m_player.ammo>0?0.f:1.f;}
  if(m_selectedItem==2&&m_medkits>0&&m_player.health<100){--m_medkits;m_player.health=std::min(100.f,m_player.health+35);sound(Sound::Pickup,.6f);}
 };
 if(use)activate();
 if(!click)return;
 int x=input.pointerX,y=input.pointerY;
 if(x>=480&&x<586&&y>=36&&y<58){m_inventoryOpen=false;m_suppressFire=true;return;}
 if(x>=350&&x<574&&y>=280&&y<310){activate();return;}
 if(x>=62&&x<322&&y>=64&&y<136){m_selectedItem=0;if(!m_weaponEquipped)activate();return;}
 if(x>=62&&x<322&&y>=144&&y<216){m_weaponEquipped=false;m_holster=1;m_selectedItem=0;return;}
 if(x<350||x>=554||y<94||y>=239)return;
 int cell=(y-94)/29*6+(x-350)/34;
 auto width=[](int i){return i==0?4:i==1?1:2;};
 auto contains=[&](int item,int c){int p=m_itemCells[item];return c%6>=p%6&&c%6<p%6+width(item)&&c/6>=p/6&&c/6<p/6+2;};
 for(int i=0;i<3;++i)if(exists(i)&&!(i==0&&m_weaponEquipped)&&contains(i,cell)){m_selectedItem=i;return;}
 if(m_selectedItem<0||!exists(m_selectedItem))return;
 int item=m_selectedItem,w=width(item);
 if(cell%6+w>6||cell/6+2>5)return;
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<w;++dx)for(int i=0;i<3;++i)
  if(i!=item&&exists(i)&&!(i==0&&m_weaponEquipped)&&contains(i,cell+dy*6+dx))return;
 m_itemCells[item]=cell;if(item==0){m_weaponEquipped=false;m_holster=1;}
}
bool Game::testInventory(){
 auto g=validationScene(Enemy::Kind::Huntsman);InputState i{};i.inventory=true;g.update(i,.02f);
 if(!g.inventoryOpen())return false;auto pos=g.player().pos;int ammo=g.player().ammo;
 g.update(i,.02f);if(!g.inventoryOpen())return false;
 i={};i.forward=true;i.fire=true;i.mouseDx=100;g.update(i,.02f);
 if(length(g.player().pos-pos)>0||g.player().ammo!=ammo)return false;
 auto click=[&](int x,int y){g.update({},.02f);InputState c{};c.fire=true;c.pointerX=x;c.pointerY=y;g.update(c,.02f);};
 click(80,90);click(352,155);if(g.weaponEquipped()||!g.unarmed())return false;
 click(80,90);if(!g.weaponEquipped()||g.unarmed())return false;
 g.m_medkits=1;g.m_player.health=40;click(420,100);i={};i.use=true;g.update(i,.02f);
 if(g.medkits()!=0||g.player().health!=75)return false;
 i={};i.escape=true;g.update(i,.02f);if(g.inventoryOpen()||g.paused())return false;
 std::ofstream("inventory-test.txt")<<"Toggle debounce, frozen simulation, equip/stow, grid placement, first aid consumption and Escape dismissal: PASS\n";return true;
}
}
