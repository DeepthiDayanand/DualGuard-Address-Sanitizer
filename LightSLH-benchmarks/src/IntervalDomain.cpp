#include "AbstractDomain.h"
#include <cstdint>
#include <optional>
#include <cassert>



int64_t Interval::getMinLimit(size_t size){
    return (int64_t)(-1) << (size - 1);
}

int64_t Interval::getMaxLimit(size_t size){
    return (int64_t)(((uint64_t)(1) << (size - 1))- 1);
}

Interval::Interval(){
    bitWidth = DEFAULT_BIT_WIDTH;
    low = Interval::getMaxLimit(bitWidth);
    high = Interval::getMinLimit(bitWidth);
    is_bot = true;
};

Interval::Interval(int64_t x, size_t size){
    bitWidth = size;
    if(x>=Interval::getMinLimit(size)&& x<=Interval::getMaxLimit(size)){
        low = x;
        high = x;
    }else{
        low = Interval::getMinLimit(size);
        high = Interval::getMaxLimit(size); 
    }
    is_bot = false;
}

Interval::Interval(int64_t x, int64_t y, size_t size){
    if(x>=Interval::getMinLimit(size)&& y<=Interval::getMaxLimit(size)){
        low = x;
        high = y;
    }else{
        low = Interval::getMinLimit(size);
        high = Interval::getMaxLimit(size); 
    }
    is_bot = low > high;
    bitWidth = size;
};

Interval::Interval(bool flag,size_t size){
    low = Interval::getMinLimit(size);
    high = Interval::getMaxLimit(size); 
    is_bot = !flag;
    bitWidth = size;
};

Interval::Interval(size_t size){
    low = Interval::getMaxLimit(size);
    high = Interval::getMinLimit(size); 
    is_bot = true;
    bitWidth = size;
};

int64_t Interval::getHigh() { return high; }
int64_t Interval::getLow() { return low; }

template<>
Interval AbstractDomain::greatest_lower_bound(Interval& a,Interval& b){
    assert(a.getSize() == b.getSize());
    if (a.isBot() || b.isBot())
    {
        return Interval(false,a.getSize());
    }
    return Interval(std::max(a.getLow(), b.getLow()), std::min(a.getHigh(), b.getHigh()),a.getSize());
}

template<>
Interval AbstractDomain::greatest_lower_bound(std::set<Interval*>& elements){
    assert(!(elements.empty()));
    size_t size = (*(elements.begin()))->getSize();
    int64_t low = Interval::getMinLimit(size);
    int64_t high = Interval::getMaxLimit(size);
    for(auto element :elements){
        assert(size == element->getSize());
        if(element->isBot()){
            return Interval(false,size);
        }
        low = std::max(low, element->getLow());
        high = std::min(high, element->getHigh());
    }
    return Interval(low, high, size);
}

template<>
Interval AbstractDomain::least_upper_bound(Interval& a,Interval& b){
    assert(a.getSize() == b.getSize());
    if(a.isBot()){
        return b;
    }
    if(b.isBot()){
        return a;
    }
    return Interval(std::min(a.getLow(), b.getLow()), std::max(a.getHigh(), b.getHigh()), a.getSize());
}

template<>
Interval AbstractDomain::least_upper_bound(std::set<Interval*>& elements){
    size_t size = (*(elements.begin()))->getSize();
    int64_t low = Interval::getMaxLimit(size);
    int64_t high  = Interval::getMinLimit(size);
    for(auto element :elements){
        assert(size == element->getSize());
        if(element->isBot()){
            continue;
        }
        low = std::min(low, element->getLow());
        high = std::max(high, element->getHigh());
    }
    return Interval(low, high, size);
}

template<>
Interval AbstractDomain::NOT(Interval& src1){
   // x + ~x = -1
   if(src1.isBot()){
       return Interval(false,src1.getSize());
   }
   return Interval(-1 - src1.getHigh(), -1 - src1.getLow(), src1.getSize());
}

bool inInterval(int64_t x, Interval& interval){
    return !interval.isBot() && (x >= interval.getLow()) && (x <= interval.getHigh());
}


std::optional<int64_t> boundPlus(int64_t src1, int64_t src2, size_t size){
    //return {} when overflow
    if ((src1 > 0) && (Interval::getMaxLimit(size) - src1 < src2)){
        return {};
    }
    if((src1 < 0) && (src1 - Interval::getMinLimit(size) < -src2)){
        return {};
    }
    return src1 + src2;
}

template<>
Interval AbstractDomain::PLUS(Interval& src1, Interval& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return Interval(false,src1.getSize());
    }
    auto low = boundPlus(src1.getLow(), src2.getLow(), src1.getSize());
    auto high = boundPlus(src1.getHigh(), src2.getHigh(), src1.getSize());
    if(low&&high){
        return Interval(*low, *high, src1.getSize());
    }
    return Interval(true, src1.getSize());
}

std::optional<int64_t> boundMinus(int64_t src1, int64_t src2, size_t size){
    //return {} when overflow
    if((src1 > 0)&&(Interval::getMinLimit(size)==src2)){
        // -std::numeric_limits<int64_t>::min() is 0
        return {};
    }
    if ((src1 > 0) && (Interval::getMaxLimit(size) - src1 < - src2)){
        return {};
    }
    if((src1 < 0) && (src1 - Interval::getMinLimit(size) <  src2)){
        return {};
    }
    return src1 - src2;
}


template<>
Interval AbstractDomain::MINUS(Interval& src1, Interval& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize());
    }
    auto low = boundMinus(src1.getLow(), src2.getHigh(), src1.getSize());
    auto high = boundMinus(src1.getHigh(), src2.getLow(), src1.getSize());
    if(low&&high){
        return Interval(*low, *high, src1.getSize());
    }
    return Interval(true, src1.getSize());
}


std::optional<int64_t> boundTime(int64_t a, int64_t b, size_t size) {
    int64_t result = a * b;
    if (a != 0 && ((result / a) != b))
    {
        // if(((a^b)&((uint64_t)1<<63))==0){
        //     return {};
        // } else{
        //     return {};
        // }
        return {};
    }
    if(result>Interval::getMaxLimit(size)||result<Interval::getMinLimit(size)){
        return {};
    }
    return result;
}

template<>
Interval AbstractDomain::TIME(Interval& src1, Interval& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize()); 
    }
    auto side1 = boundTime(src1.getLow(), src2.getLow(), src1.getSize());
    auto side2 = boundTime(src1.getLow(), src2.getHigh(), src1.getSize());
    auto side3 = boundTime(src1.getHigh(), src2.getLow(), src1.getSize());
    auto side4 = boundTime(src1.getHigh(), src2.getHigh(), src1.getSize());
    if(side1&&side2&&side3&&side4){
        return Interval(
        std::min({*side1, *side2, *side3, *side4}),
        std::max({*side1, *side2, *side3, *side4}), src1.getSize());
    }
    return Interval(true, src1.getSize());
}

template<>
Interval AbstractDomain::AND(Interval& src1, Interval& src2){
    // not precise, set low to min or 0, set high to one of the bound
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize());
    }
    int64_t low = 0;
    if (src1.getLow() < 0 && src2.getLow() < 0){
        // if both can be negtive, set low to min;
        low = Interval::getMinLimit(src1.getSize());
    }
    // ignore sign bit, a & b <= min(a,b)
    auto size = src1.getSize();
    bool flag1 = (src1.getLow() < 0 && src1.getHigh() >= 0);                          // -1 in src1
    bool flag2 = (src2.getLow() < 0 && src2.getHigh() >= 0);                          // -1 in src2
    bool negative = (src1.getHigh() < 0) && (src2.getHigh() < 0); // high is negtive when src1 and src2 are both negative
    int64_t high; 
    int64_t high1 = (int64_t)(src1.getHigh() & (((uint64_t)1 << (size - 1)) - 1));
    int64_t high2 = (int64_t)(src2.getHigh() & (((uint64_t)1 << (size - 1)) - 1));
    if(flag1 || flag2){
        high = std::max(flag2 ? high1 : std::min(high1, high2), flag1 ? high2 : std::min(high1, high2));
    }else{
        if(negative){
            high = std::min(src1.getHigh(), src2.getHigh());
        } else{
            high = std::min(high1, high2);
        }
    }
    return Interval(low, high, size);
}

template<>
Interval AbstractDomain::OR(Interval& src1, Interval& src2){
    // not precise, set high to -1 or max, set low to one of the bound
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize());
    }
    int64_t high = Interval::getMaxLimit(src1.getSize());
    size_t size = src1.getSize();
    if(src1.getHigh() <0 || src2.getHigh() <0){
        high = -1;
    }
    // ignore sign bit, a | b >= max(a,b)
    bool flag1 = (src1.getLow() < 0 && src1.getHigh() >= 0);                          // 0 in src1
    bool flag2 = (src2.getLow() < 0 && src2.getHigh() >= 0);                          // 0 in src2
    bool positive = (src1.getLow() >= 0) && (src2.getLow() >= 0); // high is negtive when src1 and src2 are both negative
    int64_t low; 
    int64_t low1 = (int64_t)(src1.getLow() | ~((((uint64_t)(1)) << (size - 1)) - 1));
    int64_t low2 = (int64_t)(src2.getLow() | ~((((uint64_t)(1)) << (size - 1)) - 1));
    if(flag1 || flag2){
        low = std::min(flag2 ? low1 : std::max(low1, low2), flag1 ? low2 : std::max(low1, low2));
    }else{
        if(positive){
            low = std::max(src1.getLow(), src2.getLow());
        }else{
            
            low = std::max(low1, low2);
        }
    }
    return Interval(low,high, size);
}

template<>
Interval AbstractDomain::XOR(Interval& src1, Interval& src2){
    assert(src1.getSize()==src2.getSize());
    // TODO: not precise, return (true)
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize());
    }
    return Interval(true, src1.getSize());
}


template<>
Interval AbstractDomain::LSHIFT(Interval& src1, Interval& src2){
    // src1, src2 should be positive
    assert(src1.getSize()==src2.getSize());
    if(src1.isBot() || src2.isBot() ){
        return Interval(false, src1.getSize());
    }
    if(src1.getLow() <0 || src2.getLow() <0){
        return Interval(true, src1.getSize());
    }
    int first_one = 0;
    for (int i = src1.getSize() - 2; i >= 0;i--){
        if((src1.getHigh() & ((uint64_t)1<<i))!=0){
            first_one = i;
            break;
        }
    }
    if(first_one + src2.getHigh() > src1.getSize() - 1){
        //overflow
        return Interval(true, src1.getSize());
    }
    return Interval(src1.getLow() << src2.getLow(), src1.getHigh() << src2.getHigh(), src1.getSize());
}

template<>
Interval AbstractDomain::LOGIC_RSHIFT(Interval& src1, Interval& src2){
    assert(src1.getSize()==src2.getSize());
    // src1, src2 should be positive
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize());
    }
    if(src1.getLow() <0 || src2.getLow() <0){
        return Interval(true, src1.getSize());
    }
    return Interval(src1.getLow() >> src2.getHigh(), src1.getHigh() >> src2.getLow(), src1.getSize());
}

template<>
Interval AbstractDomain::ARITH_RSHIFT(Interval& src1, Interval& src2){
    // src1, src2 should be positive
    assert(src1.getSize()==src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return Interval(false, src1.getSize());
    }
    if(src1.getLow() <0 || src2.getLow() <0){
        return Interval(true, src1.getSize());
    }
    return Interval(src1.getLow() >> src2.getHigh(), src1.getHigh() >> src2.getLow(), src1.getSize());
}

template <>
Interval AbstractDomain::extract(Interval &src, uint offset1, uint offset2){
    assert((offset1 >= 0) && (offset2 >= offset1) && (offset2 <= src.getSize()));
    if(offset1==0 && offset2 == src.getSize()){
        return src;
    }
    uint size = offset2 - offset1;
    return Interval(true, size);
}

template <> 
Interval AbstractDomain::concatenation(std::vector<Interval*>& vector){
    uint size = 0;
    for (auto element : vector){
        size += element->getSize();
    }
    return Interval(true,size);
}

template <>
Interval AbstractDomain::SEXT(Interval &src, uint size){
    return Interval(src.getLow(), src.getHigh(), size);
}

template <>
Interval AbstractDomain::upLift(Interval &src1, Interval& src2, int64_t strict){
    // set src1's high to src2.high
    return Interval(src1.getLow(), src2.getHigh() - (int64_t)strict, src1.getSize());
}

template <>
Interval AbstractDomain::downLift(Interval &src1, Interval& src2, int64_t strict){
    // set src1's low to src2's low
    return Interval(src2.getLow() + (int64_t)strict, src1.getHigh(), src1.getSize());
}

bool Interval::operator==(const Interval& other) const{
    if(bitWidth != other.bitWidth){
        return false;
    }
    if(is_bot && other.is_bot){
        return true;
    }
    if((!is_bot) &&(!other.is_bot)){
        return (low == other.low) && (high == other.high);
    }
    return false;
}
