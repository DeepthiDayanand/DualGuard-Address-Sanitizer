#include "AbstractDomain.h"
#include <cstdint>
#include <optional>
#include <cassert>

uint64_t uInterval::getMinLimit(size_t size){
    return 0;
}

uint64_t uInterval::getMaxLimit(size_t size){
    if(size ==64){
        return (uint64_t)(-1);
    }
    return ((uint64_t)(1) << size)- 1;
}

uInterval::uInterval(){
    bitWidth = DEFAULT_BIT_WIDTH;
    low = uInterval::getMaxLimit(bitWidth);
    high = uInterval::getMinLimit(bitWidth);
    is_bot = true;
};

uInterval::uInterval(uint64_t x, size_t size){
    if(size ==64){
        low = x;
        high = x;
    }else{
        low = x & (((uint64_t)1 << size) - 1);
        high = x & (((uint64_t)1 << size) - 1);
    }
    bitWidth = size;
    is_bot = false;
}

uInterval::uInterval(uint64_t x, uint64_t y, size_t size){
    if(x>=uInterval::getMinLimit(size)&& y<=uInterval::getMaxLimit(size)){
        low = x;
        high = y;
    }else{
        low = uInterval::getMinLimit(size);
        high = uInterval::getMaxLimit(size); 
    }
    is_bot = low > high;
    bitWidth = size;
};

uInterval::uInterval(bool flag,size_t size){
    low = uInterval::getMinLimit(size);
    high = uInterval::getMaxLimit(size); 
    is_bot = !flag;
    bitWidth = size;
};

uInterval::uInterval(size_t size){
    low = uInterval::getMaxLimit(size);
    high = uInterval::getMinLimit(size); 
    is_bot = true;
    bitWidth = size;
};

uint64_t uInterval::getHigh() { return high; }
uint64_t uInterval::getLow() { return low; }

template<>
uInterval AbstractDomain::greatest_lower_bound(uInterval& a,uInterval& b){
    assert(a.getSize() == b.getSize());
    if (a.isBot() || b.isBot()){
        return uInterval(false,a.getSize());
    }
    return uInterval(std::max(a.getLow(), b.getLow()), 
                std::min(a.getHigh(), b.getHigh()),a.getSize());
}

template<>
uInterval AbstractDomain::greatest_lower_bound(std::set<uInterval*>& elements){
    assert(!(elements.empty()));
    size_t size = (*(elements.begin()))->getSize();
    uint64_t low = uInterval::getMinLimit(size);
    uint64_t high = uInterval::getMaxLimit(size);
    for(auto element :elements){
        assert(size == element->getSize());
        if(element->isBot()){
            return uInterval(false,size);
        }
        low = std::max(low, element->getLow());
        high = std::min(high, element->getHigh());
    }
    return uInterval(low, high, size);
}

template<>
uInterval AbstractDomain::least_upper_bound(uInterval& a,uInterval& b){
    assert(a.getSize() == b.getSize());
    if(a.isBot()){
        return b;
    }
    if(b.isBot()){
        return a;
    }
    return uInterval(std::min(a.getLow(), b.getLow()), 
                std::max(a.getHigh(), b.getHigh()), a.getSize());
}

template<>
uInterval AbstractDomain::least_upper_bound(std::set<uInterval*>& elements){
    size_t size = (*(elements.begin()))->getSize();
    uint64_t low = uInterval::getMaxLimit(size);
    uint64_t high  = uInterval::getMinLimit(size);
    for(auto element :elements){
        assert(size == element->getSize());
        if(element->isBot()){
            continue;
        }
        low = std::min(low, element->getLow());
        high = std::max(high, element->getHigh());
    }
    return uInterval(low, high, size);
}

template<>
uInterval AbstractDomain::NOT(uInterval& src1){
   // x + ~x = max
   if(src1.isBot()){
       return uInterval(false,src1.getSize());
   }
   return uInterval(uInterval::getMaxLimit(src1.getSize()) - src1.getHigh(),
                    uInterval::getMaxLimit(src1.getSize()) - src1.getLow(), src1.getSize());
}

std::optional<uint64_t> bounduPlus(uint64_t src1, uint64_t src2, size_t size){
    //return {} when overflow
    if (uInterval::getMaxLimit(size) - src1 < src2){
        return {};
    }
    return src1 + src2;
}

template<>
uInterval AbstractDomain::PLUS(uInterval& src1, uInterval& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return uInterval(false,src1.getSize());
    }
    auto low = bounduPlus(src1.getLow(), src2.getLow(), src1.getSize());
    auto high = bounduPlus(src1.getHigh(), src2.getHigh(), src1.getSize());
    if(low&&high){
        return uInterval(*low, *high, src1.getSize());
    }
    return uInterval(true, src1.getSize());
}

std::optional<uint64_t> bounduMinus(uint64_t src1, uint64_t src2, size_t size){
    //return {} when overflow
    if(src1<src2){
        return {};
    }
    return src1 - src2;
}


template<>
uInterval AbstractDomain::MINUS(uInterval& src1, uInterval& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return uInterval(false, src1.getSize());
    }
    auto low = bounduMinus(src1.getLow(), src2.getHigh(), src1.getSize());
    auto high = bounduMinus(src1.getHigh(), src2.getLow(), src1.getSize());
    if(low&&high){
        return uInterval(*low, *high, src1.getSize());
    }
    return uInterval(true, src1.getSize());
}


std::optional<uint64_t> bounduTime(uint64_t a, uint64_t b, size_t size) {
    uint64_t result = a * b;
    if (a != 0 && ((result / a) != b))
    {
        // if(((a^b)&((uint64_t)1<<63))==0){
        //     return {};
        // } else{
        //     return {};
        // }
        return {};
    }
    if(result>uInterval::getMaxLimit(size)||result<uInterval::getMinLimit(size)){
        return {};
    }    
    return result;
}

template<>
uInterval AbstractDomain::TIME(uInterval& src1, uInterval& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return uInterval(false, src1.getSize()); 
    }
    auto high = bounduTime(src1.getHigh(), src2.getHigh(), src1.getSize());
    auto low = bounduTime(src1.getLow(), src2.getLow(), src1.getSize());
    if(high&&low){
        return uInterval(*low, *high, src1.getSize());
    }
    return uInterval(true, src1.getSize());
}

template<>
uInterval AbstractDomain::AND(uInterval& src1, uInterval& src2){
    // not precise, set low to min or 0, set high to one of the bound
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return uInterval(false, src1.getSize());
    }
    uint64_t high = std::min(src1.getHigh(), src2.getHigh());
    return uInterval(0, high, src1.getSize());
}

template<>
uInterval AbstractDomain::OR(uInterval& src1, uInterval& src2){
    // not precise, set high to -1 or max, set low to one of the bound
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return uInterval(false, src1.getSize());
    }
    uint64_t high = uInterval::getMaxLimit(src1.getSize());
    uint64_t low = std::max(src1.getLow(), src2.getLow());
    return uInterval(low, high, src1.getSize());
}

template<>
uInterval AbstractDomain::XOR(uInterval& src1, uInterval& src2){
    assert(src1.getSize()==src2.getSize());
    // TODO: not precise, return (true)
    if(src1.isBot() || src2.isBot()){
        return uInterval(false, src1.getSize());
    }
    return uInterval(true, src1.getSize());
}

//HERE
template<>
uInterval AbstractDomain::LSHIFT(uInterval& src1, uInterval& src2){
    // src1, src2 should be positive
    assert(src1.getSize()==src2.getSize());
    if(src1.isBot() || src2.isBot() ){
        return uInterval(false, src1.getSize());
    }
    int first_one = 0;
    for (int i = src1.getSize(); i >= 0;i--){
        if((src1.getHigh() & ((uint64_t)1<<i))!=0){
            first_one = i;
            break;
        }
    }
    if(first_one + src2.getHigh() >= src1.getSize()){
        //overflow
        return uInterval(true, src1.getSize());
    }
    return uInterval(src1.getLow() << src2.getLow(), 
                src1.getHigh() << src2.getHigh(), src1.getSize());
}

template<>
uInterval AbstractDomain::LOGIC_RSHIFT(uInterval& src1, uInterval& src2){
    assert(src1.getSize()==src2.getSize());
    // src1, src2 should be positive
    if(src1.isBot() || src2.isBot()){
        return uInterval(false, src1.getSize());
    }
    return uInterval(src1.getLow() >> src2.getHigh(), 
                src1.getHigh() >> src2.getLow(), src1.getSize());
}

template <>
uInterval AbstractDomain::extract(uInterval &src, uint offset1, uint offset2){
    assert((offset1 >= 0) && (offset2 >= offset1) && (offset2 <= src.getSize()));
    if(offset1==0 && offset2 == src.getSize()){
        return src;
    }
    // size < 64
    uint size = offset2 - offset1;
    uint64_t high = src.getHigh() >> offset1;
    uint64_t low = src.getLow() >> offset1;
    uint64_t common = high ^ low;
    if(common > (((uint64_t)1 << size) - 1)  ){
        return uInterval(true, size);
    }else{
        low = (low & (((uint64_t)1 << size) - 1));
        high = (high & (((uint64_t)1 << size) - 1));
        return uInterval(low, high, size);
    }
    

    // // TOO SLOW!!!
    // for (int i = 63; i >= 0;i++){
    //     if((common & ((uint64_t)1 << (63 - 1)))!=0){
    //         lowestOne = i;
    //     }
    // }
    // if(lowestOne == -1){
    //     low = (low & (((uint64_t)1 << size) - 1));
    //     high = (high & (((uint64_t)1 << size) - 1));
    //     return uInterval(low, high, size);
    // }else if(lowestOne >= size){
    //     return uInterval(true, size);
    // }else{
    //     // size < 64
    //     low = (low & (((uint64_t)1 << size) - 1)) & (~(((uint64_t)1 << (lowestOne + 1)) - 1));
    //     high = low + (((uint64_t)1 << (lowestOne + 1)) - 1);
    //     return uInterval(low, high, size);
    // }
}

template <> 
uInterval AbstractDomain::concatenation(std::vector<uInterval*>& vector){
    uint size = 0;
    uint64_t high = 0;
    uint64_t low = 0;
    for (auto element : vector){
        high += element->getHigh() << size;
        low += element->getLow() << size;
        size += element->getSize();
    }

    return uInterval(high, low, size);
}

template<>
uInterval AbstractDomain::ZEXT(uInterval& src, uint size){
    return uInterval(src.getLow(), src.getHigh(), size);
}

template <>
uInterval AbstractDomain::upLift(uInterval &src1, uInterval& src2, int64_t strict){
    // set src1's high to src2's high
    if(src1.isBot() || src2.isBot()){
        return src1;
    }
    return uInterval(src1.getLow(), src2.getHigh() - (uint64_t)strict, src1.getSize());
}

template <>
uInterval AbstractDomain::downLift(uInterval &src1, uInterval& src2, int64_t strict){
    // set src1's low to src2's low
    if(src1.isBot() || src2.isBot()){
        return src1;
    }
    return uInterval(src2.getLow() + (uint64_t)strict, src1.getHigh(), src1.getSize());
}

bool uInterval::operator==(const uInterval& other) const{
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


Interval u2sInterval(uInterval & urange){
    size_t size = urange.getSize();
    if(urange.isBot()){
        return Interval(false, size);
    }
    if(urange.getLow() >= ((uint64_t)1 << (size - 1))){
        uint64_t mask = ~(((uint64_t)1 << (size - 1)) - 1);
        return Interval((int64_t)(urange.getLow() | mask), (int64_t)(urange.getHigh() | mask), size);
    }
    if(urange.getHigh() >= ((uint64_t)1 << (size - 1))){
        return Interval(true, size);
    }
    return Interval(urange.getLow(), urange.getHigh(), size);
}

uInterval s2uInterval(Interval & range){
    size_t size = range.getSize();
    if(range.isBot()){
        return uInterval(false, size);
    }
    if(range.getHigh() < 0){
        if(size<64){
            uint64_t mask = ((uint64_t)1 << size) - 1;
            return uInterval(range.getLow() & mask, range.getHigh() & mask, size);
        }else{
            return uInterval((uint64_t)(range.getLow()), (uint64_t)(range.getHigh()), size);
        }
    }
    if(range.getLow() < 0){
        return uInterval(true, size);
    }
    return uInterval(range.getLow(), range.getHigh(), size);
}