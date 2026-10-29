#pragma once
// Aircraft families for visually distinct on-map symbols.
// Stateless: never guess an aircraft photograph or airframe identity.
#include <cstring>
namespace AircraftSymbol {
enum class Kind : unsigned char { Airliner, BusinessJet, Light, Helicopter };
inline bool in(const char* type, const char* const* arr, int length) {
    if(!type)return false;
    for(int i=0;i<length;++i)if(std::strcmp(type,arr[i])==0)return true;
    return false;
}
inline Kind fromType(const char* t) {
    if(!t)return Kind::Airliner;
    const char* const heli[]={"A109","A139","A169","A189","AS50","EC35","EC45",
        "R22","R44","R66","B06","B407","B429","S76","S92"};
    if(in(t,heli,sizeof(heli)/sizeof(heli[0])))return Kind::Helicopter;
    const char* const small[]={"C152","C172","C177","C182","C206","C208",
        "C210","DA40","DA42","DA62","SR20","SR22","PA28","P28R","PA31",
        "PA34","PA46","PC12","DHC6"};
    if(in(t,small,sizeof(small)/sizeof(small[0])))return Kind::Light;
    const char* const biz[]={"C25A","C25B","C25C","C510","C525","C550",
        "C560","C56X","C650","C680","C68A","C700","GLF4","GLF5","GLF6",
        "GLF7","GL5T","GL7T","GLEX","CL35","CL60","LJ35","LJ45","LJ60",
        "H25B","E35L","E50P","E55P","E550","PC24"};
    if(in(t,biz,sizeof(biz)/sizeof(biz[0])))return Kind::BusinessJet;
    return Kind::Airliner;
}
inline float scale(Kind kind,int zoom,bool selected) {
    // Keep planes readable yet less cluttered on zoomed-out UK maps.
    float base=zoom<=7?0.64f:zoom<=9?0.76f:0.86f;
    if(kind==Kind::Helicopter||kind==Kind::Light)base+=0.04f;
    if(selected)base+=0.26f;
    return base;
}
}