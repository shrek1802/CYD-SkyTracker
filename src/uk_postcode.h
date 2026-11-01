#pragma once
// UK full-postcode syntax and canonical formatting. No Arduino dependency.
#include <string>
#include <cctype>
namespace UkPostcode {
inline std::string compact(const std::string& input) {
    std::string out;
    for(unsigned char ch: input) {
        if(ch==' ' || ch=='\t' || ch=='\n' || ch=='\r')continue;
        if(ch>='a' && ch<='z') ch=static_cast<unsigned char>(ch-'a'+'A');
        out.push_back(char(ch));
    }
    return out;
}
inline bool validCompact(const std::string& s) {
    if(s=="GIR0AA")return true; // historical special postcode
    if(s.size()<5 || s.size()>7)return false;
    const auto letter=[](char c){return c>='A'&&c<='Z';};
    const auto digit=[](char c){return c>='0'&&c<='9';};
    const size_t n=s.size(), out=n-3;
    if(!digit(s[n-3])||!letter(s[n-2])||!letter(s[n-1]))return false;
    if(!letter(s[0]))return false;
    // Outward code: A9, A99, A9A, AA9, AA99, AA9A.
    size_t pos=1;
    if(pos<out && letter(s[pos]))++pos;
    if(pos>=out || !digit(s[pos]))return false;
    ++pos;
    if(pos<out && (digit(s[pos]) || letter(s[pos])))++pos;
    return pos==out;
}
inline std::string format(const std::string& input) {
    const std::string s=compact(input);
    if(!validCompact(s))return "";
    return s.substr(0,s.size()-3)+" "+s.substr(s.size()-3);
}
}
