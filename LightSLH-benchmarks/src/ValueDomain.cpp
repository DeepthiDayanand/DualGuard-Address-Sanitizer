#include "AbstractDomain.h"
#include "out.h"
#include <cstdint>
#include <optional>
#include <cassert>


using namespace std;

PointerValue::PointerValue(Interval v){
    alignedRegion = false;
    alignedCell = false;
    range = v;
}

PointerValue::PointerValue(bool align, std::vector<PtrGen> vec){
    alignedRegion = true;
    alignedCell = align;
    ptr_gen_history = vec;
    range = Interval(POINTER_SIZE_IN_BITS);
}

bool PointerValue::isCellAligned() { return alignedCell; }

bool PointerValue::isRegionAligned() { return alignedRegion; }

void PointerValue::disableCellAligned() { alignedCell = false; }

void PointerValue::disableRegionAligned() {
    if(alignedRegion){
        alignedRegion = false;
        alignedCell = false;
        range = flattenPtrGen(ptr_gen_history);
    }
}

Interval& PointerValue::getRange(){
    if(alignedRegion){
        range = flattenPtrGen(ptr_gen_history);
    }
    return range;
}

vector<PtrGen> & PointerValue::getPtrGenHistory(){return ptr_gen_history;}

PtrGen& PointerValue::getLatestPtrGen(){
    return ptr_gen_history[ptr_gen_history.size() - 1];
}

void PointerValue::insertPtrGenAction(PtrGen ptr, size_t t){
    if(alignedRegion){
        ptr_gen_history.push_back(ptr);
    }else{
        range.high += t;
        range.low += t;
    }
}

void PointerValue::addPointer(Interval v, size_t offsetsize, size_t psize){
    // assert: last record in history shouldn'd be structType
    // offsetsize is useful when isalignedRegion
    // psize is useful when !isalignedRegion
    v.bitWidth = range.getSize();
    bool flag = false;
    if(alignedRegion){
        if(ptr_gen_history[ptr_gen_history.size() - 1].isStruct()){
            if(v.high == 0 && v.low == 0 ){
                return;
            }else{
                this->disableRegionAligned();
                flag = true;
            }
        }else{
            Interval offset = Interval((int64_t)offsetsize, v.bitWidth);
            v = AbstractDomain::TIME(v, offset);
            ptr_gen_history[ptr_gen_history.size() - 1] =
                PtrGen(AbstractDomain::PLUS(ptr_gen_history[ptr_gen_history.size() - 1].range, v)
                        , ptr_gen_history[ptr_gen_history.size() - 1].size);
        }
    }
    if((!alignedRegion) || flag){
        Interval poffset = Interval((int64_t)psize, v.bitWidth);
        v = AbstractDomain::TIME(v, poffset);
        range = AbstractDomain::PLUS(range, v);
    }
} 

bool hasSameStructure(PointerValue & ptr1, PointerValue &ptr2){
    if(ptr1.isRegionAligned() && ptr2.isRegionAligned() && (ptr1.getPtrGenHistory().size()==ptr2.getPtrGenHistory().size())){
        for (int i = 0; i < ptr1.getPtrGenHistory().size();i++){
            auto &gen1 = ptr1.getPtrGenHistory()[i];
            auto &gen2 = ptr2.getPtrGenHistory()[i];
            if(gen1.isStruct() != gen2.isStruct()){
                return false;
            }else if(gen1.isStruct()){
                if(gen1.member!=gen2.member){
                    return false;
                }
            }
        }
        return true;
    }else{
        return false;
    }
}

Interval flattenPtrGen(std::vector<PtrGen> &ptrgen){
    int64_t low = 0, high = 0;
    for (int i = 0; i < ptrgen.size();i++){
        if(ptrgen[i].isStruct()){
            low += ptrgen[i].size;
            high += ptrgen[i].size;
        }else{
            low += ptrgen[i].size * ptrgen[i].range.getLow();
            high += ptrgen[i].size * ptrgen[i].range.getHigh();
        }
    }
    return Interval(low, high, POINTER_SIZE_IN_BITS);
}

ValueDomain::ValueDomain(bool top, size_t size){
    bitWidth = size;
    range = Interval(true, size);
    urange = uInterval(true, size);
    is_top = top; 
}

ValueDomain::ValueDomain(Interval interval, uInterval uinterval){
    bitWidth = interval.getSize();
    range = interval;
    urange = uinterval;
    is_top = false;
}

ValueDomain::ValueDomain(Interval interval, uInterval uinterval, bool top){
    bitWidth = interval.getSize();
    urange = uinterval;
    range = interval;
    is_top = top;
}

ValueDomain::ValueDomain(llvm::Value *v, size_t size){
    bitWidth = POINTER_SIZE_IN_BITS;
    range = Interval(true, POINTER_SIZE_IN_BITS);
    urange = uInterval(true, POINTER_SIZE_IN_BITS);
    pointer_map.insert(std::make_pair(v, PointerValue(true, {PtrGen(Interval((int64_t)0, POINTER_SIZE_IN_BITS), size)})));
    is_top = false;
}

ValueDomain::ValueDomain(std::map<llvm::Value *, PointerValue> vmap){
    bitWidth = POINTER_SIZE_IN_BITS;
    range = Interval(true, bitWidth);
    urange = uInterval(true, bitWidth);
    pointer_map = vmap;
    is_top = false;
}

ValueDomain::ValueDomain(Interval interval, uInterval uinterval, std::map<llvm::Value *, PointerValue> vmap){
    bitWidth = interval.getSize();
    range = interval;
    urange = uinterval;
    pointer_map = vmap;
    is_top = false;
}

ValueDomain::ValueDomain(int64_t n, size_t size){
    bitWidth = size;
    range = Interval((int64_t)n, bitWidth);
    urange = uInterval((uint64_t)n, bitWidth);
    is_top = false;
}

ValueDomain::ValueDomain(size_t size){
    bitWidth = size;
    range = Interval(size);
    urange = uInterval(size);
    is_top = false;
}


map<llvm::Value *, PointerValue> &ValueDomain::getPointerMap() { return pointer_map; }

Interval &ValueDomain::getRange() { return range; }

uInterval &ValueDomain::getuRange() { return urange; }

void ValueDomain::disableRegionAligned(){
    if(is_top){
        return;
    }
    for(auto& [membase, pointervalue]: pointer_map){
        pointervalue.disableRegionAligned();
    }
}

void ValueDomain::insertPtrGenAction(PtrGen ptrgen, size_t t){
    if(is_top){
        return;
    }
    for(auto& [membase, pointervalue]: pointer_map){
        pointervalue.insertPtrGenAction(ptrgen, t);
    }
}

void ValueDomain::addPointer(Interval v, size_t offsetsize, size_t tsize){
    if(is_top){
        return;
    }
    for(auto& [membase, pointervalue]: pointer_map){
        pointervalue.addPointer(v, offsetsize, tsize);
    }
}

bool ValueDomain::isTop() { return is_top; }

bool ValueDomain::isBot(){
    if(range.isBot() || urange.isBot()){
        return true;
    }
    for(auto& [membase, pv] : pointer_map){
        if(pv.isRegionAligned()){
            for(auto ptrgen: pv.getPtrGenHistory()){
                if(!ptrgen.isStruct() && ptrgen.range.isBot()){
                    return true;
                }
            }
        }else{
            if(pv.getRange().isBot()){
                return true;
            }
        }
    }
    return false;
}

void ValueDomain::setTop() { is_top = true; }

bool ValueDomain::isPointer(){
    // to tell whether the value can be seen as a Pointer
    return is_top || !pointer_map.empty();
}

size_t ValueDomain::getSize() { 
    return bitWidth; 
}

template<>
ValueDomain AbstractDomain::greatest_lower_bound(ValueDomain& a, ValueDomain& b){
    assert(a.getSize() == b.getSize());
    auto glb_range = AbstractDomain::greatest_lower_bound(a.getRange(), b.getRange());
    auto glb_urange = AbstractDomain::greatest_lower_bound(a.getuRange(), b.getuRange());
    if(a.isTop() && b.isTop()){
        return ValueDomain(glb_range, glb_urange, true);
    }else if(a.isTop() && !b.isTop()){
        return ValueDomain(glb_range, glb_urange, b.getPointerMap());
    }else if(!a.isTop() && b.isTop()){
        return ValueDomain(glb_range, glb_urange, a.getPointerMap());
    }
    
    std::map<llvm::Value *, PointerValue> glb;
    for (auto& [a_membase, a_pointer] : a.getPointerMap()) {
        auto b_pair = b.getPointerMap().find(a_membase);
        if(b_pair!=b.getPointerMap().end()){
            auto& b_pointer = b_pair->second;
            if(a_pointer.isRegionAligned() && b_pointer.isRegionAligned()){
                if(hasSameStructure(a_pointer,b_pointer)){
                    vector<PtrGen> ptr_gen_vec;
                    for (int i = 0; i < a_pointer.getPtrGenHistory().size();i++){
                        if(a_pointer.getPtrGenHistory()[i].isStruct()){
                            ptr_gen_vec.push_back(a_pointer.getPtrGenHistory()[i]);
                        }else{
                            ptr_gen_vec.push_back(PtrGen(AbstractDomain::greatest_lower_bound(
                                a_pointer.getPtrGenHistory()[i].range, b_pointer.getPtrGenHistory()[i].range)
                                ,a_pointer.getPtrGenHistory()[i].size));
                        }
                    }
                    bool align = a_pointer.isCellAligned() || b_pointer.isCellAligned();
                    glb.insert(std::make_pair(a_membase, PointerValue(align, ptr_gen_vec)));
                } // else: can't align, indicates two pointer don't point to same cell
            }
            else{
                glb.insert(std::make_pair(a_membase, PointerValue(
                    AbstractDomain::greatest_lower_bound(a_pointer.getRange(), b_pointer.getRange())
                )));// over-approxiamation
            }
        }
    }
    return ValueDomain(glb_range, glb_urange, glb);
}


template<>
ValueDomain AbstractDomain::greatest_lower_bound(std::set<ValueDomain*>& elements){
    assert(!(elements.empty()));
    size_t size = (*(elements.begin()))->getSize();
    ValueDomain glb = ValueDomain(Interval(true, size), uInterval(true, size), true);
    for (auto element : elements){
        glb = AbstractDomain::greatest_lower_bound(glb, *element);
    }
    return glb;
}

template<> 
ValueDomain AbstractDomain::least_upper_bound(ValueDomain& a, ValueDomain& b){
    Interval lub_range = AbstractDomain::least_upper_bound(a.getRange(), b.getRange());
    uInterval lub_urange = AbstractDomain::least_upper_bound(a.getuRange(), b.getuRange());
    if(a.isTop() || b.isTop()){
        return ValueDomain(lub_range, lub_urange, true);
    }
    std::map<llvm::Value *, PointerValue> lub = a.getPointerMap();
    for(auto& [membase, b_pointer]: b.getPointerMap()){
        if(lub.find(membase)==lub.end()){
            lub.insert({membase, b_pointer});
        }else{
            auto& a_pointer = lub[membase];
            if(a_pointer.isRegionAligned() && b_pointer.isRegionAligned() && hasSameStructure(a_pointer,b_pointer)){
                vector<PtrGen> ptr_gen_vec;
                for (int i = 0; i < a_pointer.getPtrGenHistory().size();i++){
                    if(a_pointer.getPtrGenHistory()[i].isStruct()){
                            ptr_gen_vec.push_back(a_pointer.getPtrGenHistory()[i]);
                        }else{
                            ptr_gen_vec.push_back(PtrGen(AbstractDomain::least_upper_bound(
                                a_pointer.getPtrGenHistory()[i].range, b_pointer.getPtrGenHistory()[i].range)
                                , a_pointer.getPtrGenHistory()[i].size));
                        }
                }
                bool align = a_pointer.isCellAligned() && b_pointer.isCellAligned();
                lub[membase] =  PointerValue(align, ptr_gen_vec);
            }else{
                lub[membase] = PointerValue(AbstractDomain::greatest_lower_bound(a_pointer.getRange(), b_pointer.getRange())); 
                // over-approxiamation
            }
        }
    }
    return ValueDomain(lub_range, lub_urange, lub);
}


template<>
ValueDomain AbstractDomain::least_upper_bound(std::set<ValueDomain*>& elements){
    assert(!(elements.empty()));
    size_t size = (*(elements.begin()))->getSize();
    ValueDomain lub = ValueDomain(Interval(false, size), uInterval(false, size), false);
    for (auto element : elements){
        lub = AbstractDomain::least_upper_bound(lub, *element);
    }
    return lub;
}

template<>
ValueDomain AbstractDomain::NOT(ValueDomain& src){
    ValueDomain dst = ValueDomain(AbstractDomain::NOT(src.getRange())
                                , AbstractDomain::NOT(src.getuRange()));
    dst.setTop();
    return dst;
}

template<>
ValueDomain AbstractDomain::AND(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::AND(src1.getRange(), src2.getRange())
                                , AbstractDomain::AND(src1.getuRange(), src2.getuRange()));

    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    // }else if(src1.isPointer()){
    //     dst.getPointerMap() = src1.getPointerMap();
    //     dst.disableRegionAligned();
    //     int64_t low = std::min(-(int64_t)(~src2.getuRange().getLow()), (int64_t)0);
    //     dst.addPointer(Interval(low, 0, POINTER_SIZE_IN_BITS), 1, 1);
    // }else if(src2.isPointer()){
    //     dst.getPointerMap() = src2.getPointerMap();
    //     dst.disableRegionAligned();
    //     int64_t low = std::min(-(int64_t)(~src1.getuRange().getLow()), (int64_t)0);
    //     dst.addPointer(Interval(low, 0, POINTER_SIZE_IN_BITS), 1, 1);
    // }
    return dst;
}

template<>
ValueDomain AbstractDomain::OR(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::OR(src1.getRange(), src2.getRange())
                                , AbstractDomain::OR(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    return dst;
}


template<>
ValueDomain AbstractDomain::XOR(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::XOR(src1.getRange(), src2.getRange())
                                , AbstractDomain::XOR(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::PLUS(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::PLUS(src1.getRange(), src2.getRange())
                                , AbstractDomain::PLUS(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }else if(src1.isPointer()){
        dst.getPointerMap() = src1.getPointerMap();
        dst.disableRegionAligned();
        dst.addPointer(src2.getRange(), 1, 1);
    }else if(src2.isPointer()){
        dst.getPointerMap() = src2.getPointerMap();
        dst.disableRegionAligned();
        dst.addPointer(src1.getRange(), 1, 1);
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::MINUS(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::MINUS(src1.getRange(), src2.getRange())
                                , AbstractDomain::MINUS(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }else if(src1.isPointer()){
        dst.getPointerMap() = src1.getPointerMap();
        dst.disableRegionAligned();
        dst.addPointer(Interval(-src2.getRange().high, -src2.getRange().low, POINTER_SIZE_IN_BITS), 1, 1);
    }else if(src2.isPointer()){
        dst.getPointerMap() = src2.getPointerMap();
        dst.disableRegionAligned();
        dst.addPointer(Interval(-src1.getRange().high, -src1.getRange().low, POINTER_SIZE_IN_BITS), 1, 1);
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::TIME(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::TIME(src1.getRange(), src2.getRange())
                                , AbstractDomain::TIME(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::LSHIFT(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::LSHIFT(src1.getRange(), src2.getRange())
                                , AbstractDomain::LSHIFT(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::LOGIC_RSHIFT(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::LOGIC_RSHIFT(src1.getRange(), src2.getRange())
                                , AbstractDomain::LOGIC_RSHIFT(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::ARITH_RSHIFT(ValueDomain& src1, ValueDomain& src2){
    assert(src1.getSize() == src2.getSize());
    ValueDomain dst = ValueDomain(AbstractDomain::ARITH_RSHIFT(src1.getRange(), src2.getRange())
                                , AbstractDomain::LOGIC_RSHIFT(src1.getuRange(), src2.getuRange()));
    if((src1.isPointer() && src2.isPointer()) || src1.isTop() || src2.isTop()){
        dst.setTop();
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::extract(ValueDomain& src, uint offset1, uint offset2){
    if(offset1==0&& offset2==src.getSize()){
        return src;
    }
    auto vd = ValueDomain(AbstractDomain::extract(src.getRange(), offset1, offset2)
                        , AbstractDomain::extract(src.getuRange(), offset1, offset2));
    if(src.isPointer()){
        vd.setTop();
    }
    return vd;
}

template<>
ValueDomain AbstractDomain::ZEXT(ValueDomain& src, uint size){
    uInterval u = AbstractDomain::ZEXT(src.getuRange(), size);
    Interval s = u2sInterval(u);
    auto dst =  ValueDomain(s, u);
    if(src.isPointer()){
        dst.setTop();
    }
    return dst;
}

template<>
ValueDomain AbstractDomain::SEXT(ValueDomain& src, uint size){
    Interval s = AbstractDomain::SEXT(src.getRange(), size);
    uInterval u = s2uInterval(s);
    auto dst =  ValueDomain(s, u);
    if(src.isPointer()){
        dst.setTop();
    }
    return dst;
}

template <> 
ValueDomain AbstractDomain::concatenation(std::vector<ValueDomain*>& vector){
    uint size = 0;
    std::vector<Interval *> range_vec;
    std::vector<uInterval *> urange_vec;
    bool topFlag = false;
    for (auto vd : vector){
        urange_vec.push_back(&(vd->getuRange()));
        if(vd->isPointer()){
            topFlag = true;
        }
    }
    uInterval urange = AbstractDomain::concatenation(urange_vec);
    return ValueDomain(u2sInterval(urange), urange, topFlag);
}

template <>
ValueDomain AbstractDomain::upLift(ValueDomain &src1, ValueDomain& src2, int64_t strict){
    // set src1's high to src2.high
    Interval range = AbstractDomain::upLift(src1.getRange(), src2.getRange(), strict);
    uInterval urange = AbstractDomain::upLift(src1.getuRange(), src2.getuRange(), strict);
    if(src2.isTop() || src1.isTop()){
        return ValueDomain(range, urange, src1.isTop());
    }
    auto dst_pointer_map = src1.getPointerMap();
    for(auto& [membase, pv] : src2.getPointerMap()){
        if(dst_pointer_map.find(membase)==dst_pointer_map.end()){
            continue;
        }
        if(!pv.isRegionAligned()){
            dst_pointer_map[membase].disableRegionAligned();
            dst_pointer_map[membase] = PointerValue(AbstractDomain::upLift(dst_pointer_map[membase].getRange(), pv.getRange(), strict));
        }else{
            if(!hasSameStructure(dst_pointer_map[membase], pv)){
                continue;
            }
            // pv is aligned
            std::vector<PtrGen> ptr_gen_vec;

            for (int i = 0;i<pv.getPtrGenHistory().size();i++){
                if(pv.getPtrGenHistory()[i].isStruct()){
                    ptr_gen_vec.push_back(pv.getPtrGenHistory()[i]);
                }else{
                    ptr_gen_vec.push_back(
                        PtrGen(AbstractDomain::upLift(
                            dst_pointer_map[membase].getPtrGenHistory()[i].range, pv.getPtrGenHistory()[i].range, strict), 
                            pv.getPtrGenHistory()[i].size));
                }
            }
            dst_pointer_map[membase] = PointerValue(dst_pointer_map[membase].isCellAligned(), ptr_gen_vec);
        }
    }
    return ValueDomain(range, urange, dst_pointer_map);
}

template <>
ValueDomain AbstractDomain::downLift(ValueDomain &src1, ValueDomain& src2, int64_t strict){
    // set src1's high to src2.high
    Interval range = AbstractDomain::downLift(src1.getRange(), src2.getRange(), strict);
    uInterval urange = AbstractDomain::downLift(src1.getuRange(), src2.getuRange(), strict);
    if(src2.isTop() || src1.isTop()){
        return ValueDomain(range, urange, src1.isTop());
    }
    auto dst_pointer_map = src1.getPointerMap();
    for(auto& [membase, pv] : src2.getPointerMap()){
        if(dst_pointer_map.find(membase)==dst_pointer_map.end()){
            continue;
        }
        if(!pv.isRegionAligned()){
            dst_pointer_map[membase].disableRegionAligned();
            dst_pointer_map[membase] = PointerValue(AbstractDomain::downLift(dst_pointer_map[membase].getRange(), pv.getRange(), strict));
        }else{
            if(!hasSameStructure(dst_pointer_map[membase], pv)){
                continue;
            }
            // pv is aligned
            std::vector<PtrGen> ptr_gen_vec;

            for (int i = 0;i<pv.getPtrGenHistory().size();i++){
                if(pv.getPtrGenHistory()[i].isStruct()){
                    ptr_gen_vec.push_back(pv.getPtrGenHistory()[i]);
                }else{
                    ptr_gen_vec.push_back(
                        PtrGen(AbstractDomain::downLift(
                            dst_pointer_map[membase].getPtrGenHistory()[i].range, pv.getPtrGenHistory()[i].range, strict), 
                            pv.getPtrGenHistory()[i].size));
                }
            }
            dst_pointer_map[membase] = PointerValue(dst_pointer_map[membase].isCellAligned(), ptr_gen_vec);
        }
    }
    return ValueDomain(range, urange, dst_pointer_map);
}

bool PtrGen::operator==(const PtrGen &other) const{
    if(is_struct!=other.is_struct){
        return false;
    }else if(is_struct){
        return member == other.member;
    }else{
        return range == other.range;
    }
}

bool PointerValue::operator==(const PointerValue &other) const{
    if(alignedRegion != other.alignedRegion){
        return false;
    }else if(!alignedRegion){
        return range == other.range;
    }else{
        return (ptr_gen_history == other.ptr_gen_history) && (alignedCell == other.alignedCell);
    }
}

bool ValueDomain::operator==(const ValueDomain& other) const{
    if(bitWidth != other.bitWidth){
        return false;
    }
    if(!(range ==other.range)){
        return false;
    }
    if(!(urange ==other.urange)){
        return false;
    }
    if(is_top && other.is_top){
        return true;
    }
    if(!is_top && !other.is_top){
        return pointer_map == other.pointer_map;
    }
    return false;
}

std::ostream &operator<<(std::ostream &os, const PtrGen &p){
    if(p.is_struct){
        return os << p.member;
    }else{
        return os << p.range;
    }
}

std::ostream &operator<<(std::ostream &os, const PointerValue &p){
    if(!p.alignedRegion){
        return os <<  "not aligned Regieon" << p.range;
    }else{
        os<< "aligned:" << (int)p.alignedCell << " ";
        for (int i = 0; i < p.ptr_gen_history.size();i++){
            os << p.ptr_gen_history[i] << " ";
        }
        return os;
    }
}

std::ostream &operator<<(std::ostream &os, const ValueDomain &p){
    os << "range: (" << p.range <<")  " ;
    os << "urange: (" << p.urange <<")  " ;
    if(p.is_top){
        return os << "isTop.  " ;
    }
    for (auto& pair:p.pointer_map){
        os << pair.first <<": "<< pair.second;
    }
    return os ;
}