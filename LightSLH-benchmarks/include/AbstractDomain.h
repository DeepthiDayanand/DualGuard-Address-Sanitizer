#ifndef ABSTRACT_DOMAIN_H
#define ABSTRACT_DOMAIN_H

#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <utility>
#include <cstdint>
#include <vector>
#include <array>
#include <set>
#include <algorithm>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <optional>

#define POINTER_SIZE_IN_BITS 64
#define REGISTERLEN 64
#define DEFAULT_BIT_WIDTH 64
#define MAX_REGISTER_LEN 64

//AbstractDomain is the father class of ValueDomainn and TaintDomain
class AbstractDomain{
    public:
        template <typename T> T static greatest_lower_bound(T& a, T& b);
        template <typename T> T static greatest_lower_bound(std::set<T*>& elements);
        template <typename T> T static least_upper_bound(T& a, T& b);
        template <typename T> T static least_upper_bound(std::set<T*>& elements);

        template <typename T> T static NOT(T& src1);
        template <typename T> T static AND(T& src1, T& src2);
        template <typename T> T static OR(T& src1, T& src2);
        template <typename T> T static XOR(T& src1, T& src2);

        template <typename T> T static PLUS(T& src1, T& src2);
        template <typename T> T static MINUS(T& src1, T& src2);
        template <typename T> T static TIME(T& src1, T& src2);

        template <typename T> T static LSHIFT(T& src1, T& src2);
        template <typename T> T static LOGIC_RSHIFT(T& src1, T& src2);
        template <typename T> T static ARITH_RSHIFT(T& src1, T& src2);

        template <typename T>T static extract(T &src, uint offset1, uint offset2); // from offset1 to (offset2 -1)
        template <typename T>T static concatenation(std::vector<T*>&);

        template <typename T>T static ZEXT(T& src1, uint size);
        template <typename T>T static SEXT(T& src1, uint size);

        template <typename T>T static upLift(T& src1, T& src2, int64_t strict);
        template <typename T>T static downLift(T& src1, T& src2, int64_t strict);
};



class TaintDomain: public AbstractDomain{
    public:
        enum bitTaintLable{
            ZERO = 0,
            ONE = 1,
            LOW = 2,
            HIGH = 3,
            BOT = -1
        };
        TaintDomain(TaintDomain::bitTaintLable,size_t);
        TaintDomain(std::vector<TaintDomain::bitTaintLable>&);
        TaintDomain(int64_t,size_t);
        TaintDomain(size_t);
        TaintDomain(){}

        std::vector<TaintDomain::bitTaintLable>& getTaintLVector();
        void update_information();  //when update taint_vector, update first_one/low/high and contain_bot
        int get_first_ge_one() { return first_ge_one; }
        int get_first_ge_low() { return first_ge_low; }
        int get_first_high() { return first_high; }
        bool isBot() { return contain_bot; }
        bool containHigh() { return first_high < taint_vector.size(); }
        int getSize() { return taint_vector.size(); }
        void maskLowBit(uint64_t mask);
        void maskHighBit(uint64_t bound);
        bool operator==(const TaintDomain &) const;
        friend std::ostream& operator<<(std::ostream& os, const TaintDomain& p) {
            for (int i = 0; i < p.taint_vector.size();i++){
                os << (int)p.taint_vector[i] << " ";
            }
            return os << "is_bot: " << (int)p.contain_bot << ", first_ge_one: " << (int)p.first_ge_one
                    << ", first_ge_low: " << (int)p.first_ge_low << ", first_high: " << (int)p.first_high ;
        }

    private:
        std::vector<TaintDomain::bitTaintLable> taint_vector;
        bool contain_bot;           // true if there is a bot in vector
        int first_ge_one;       //least significant bit which is ONE/OR/HIGH, if there is no ONE, set its value to size
        int first_ge_low;       //least significant bit which is LOW/HIGH, if there is no ONE, set its value to size
        int first_high;         //least significant bit which is HIGH, if there is no ONE, set its value to size
};

bool isValue(TaintDomain::bitTaintLable);

struct OverTaintDetector{
    enum DetectionState{
        Wait,
        DoneMask,
        DoneSub,
        DoneAdd //trigger OverTaintDetector
    };

    DetectionState ds = DetectionState::Wait;
    std::set<llvm::Value *> waitlist;
    llvm::Value *complement = nullptr;
    llvm::Value *masked_val = nullptr; 
    uint mask = 0;

    std::optional<uint> acceptNewVal(llvm::Instruction *);
    //when detect overtaint, return the maskLoc, otherwise {}
};

class Interval : public AbstractDomain
{
public:
    int64_t low;
    int64_t high;
    bool is_bot;
    size_t bitWidth;

    Interval(int64_t, size_t);
    Interval(int64_t, int64_t, size_t);
    Interval(bool, size_t);
    Interval(size_t);
    Interval();

    bool isBot() const { return is_bot; }
    size_t getSize() const { return bitWidth; }
    static int64_t getMinLimit(size_t);
    static int64_t getMaxLimit(size_t);
    int64_t getHigh();
    int64_t getLow();
    // static Interval u_AND(Interval &a, Interval &b);// unsigned
    static bool inInterval(int64_t, Interval &);
    bool operator==(const Interval &) const;
    friend std::ostream &operator<<(std::ostream &os, const Interval &p)
    {
        return os << "(size:" << p.bitWidth
                  << ", is_bot: " << (int)p.is_bot << ", low: " << (int64_t)p.low
                  << ", high: " << (int64_t)p.high << ")";
    }
};

class uInterval: public AbstractDomain{
    public:
        uint64_t low;
        uint64_t high;
        bool is_bot;
        size_t bitWidth;

    
        uInterval(uint64_t ,size_t);
        uInterval(uint64_t, uint64_t, size_t);
        uInterval(bool,size_t);
        uInterval(size_t);
        uInterval();

        bool isBot() const{ return is_bot; }
        size_t getSize() const { return  bitWidth; }
        static uint64_t getMinLimit(size_t);
        static uint64_t getMaxLimit(size_t);
        uint64_t getHigh();
        uint64_t getLow();
        bool operator==(const uInterval&) const;
        friend std::ostream& operator<<(std::ostream& os, const uInterval& p) {
            return os << "(size:" << p.bitWidth
                    << ", is_bot: " << (int)p.is_bot << ", low: " << (uint64_t)p.low
                    << ", high: " << (uint64_t)p.high <<")";
        }
};

Interval u2sInterval(uInterval &);
uInterval s2uInterval(Interval &);



struct PtrGen{
    bool is_struct = true;
    Interval range;
    int64_t member = 0;
    size_t size = 0;

    PtrGen(int64_t n, size_t s){
        is_struct = true;
        member = n;
        size = s;
    }
    PtrGen(Interval v, size_t s){
        is_struct = false;
        range = v;
        size = s;
    }
    bool isStruct() { return is_struct; }
    bool operator==(const PtrGen &other) const;
    friend std::ostream &operator<<(std::ostream &os, const PtrGen &p);
};

class PointerValue{
    private:
        bool alignedRegion = true;
        bool alignedCell = true;
        std::vector<PtrGen> ptr_gen_history;
                                // efficient only when alignedRegion is true, it record how
                                // this pointer is derived from a pointers
                                // two pointer with different history will result an unaligned lub/glb
                                // following element is the member variable id

        Interval range;         // efficient only when alignedRegion is false
                                // record the range of pointer, when use whis pointer, the 
                                // value read is not aligned


    public:
        PointerValue(bool, std::vector<PtrGen>);
        PointerValue(Interval);
        PointerValue(){}

        std::vector<PtrGen> &getPtrGenHistory();
        PtrGen &getLatestPtrGen();
        bool isCellAligned();
        bool isRegionAligned();
        void disableCellAligned();
        void disableRegionAligned();
        void insertPtrGenAction(PtrGen, size_t);
        void addPointer(Interval, size_t, size_t);
        Interval& getRange();

        bool operator==(const PointerValue &) const;
        friend std::ostream &operator<<(std::ostream &os, const PointerValue &p);
};
bool hasSameStructure(PointerValue &ptr1, PointerValue &ptr2);
Interval flattenPtrGen(std::vector<PtrGen> &ptrgen);



class ValueDomain: public AbstractDomain{
    private:
        size_t bitWidth;
        Interval range;      // value range, top if it's a pointer
        uInterval urange;    // value urange, top if it's a pointer
        std::map<llvm::Value *, PointerValue> pointer_map;
        bool is_top = false; // true indicates the pointer can point to any place;
                     //  when is_top is true, mem2range is useless
                     // we settop is a pointer is computed without gete

    public:
        ValueDomain(bool top, size_t bit_width);
        ValueDomain(Interval, uInterval);
        ValueDomain(Interval, uInterval, bool);
        ValueDomain(llvm::Value *, size_t bit_width);
        ValueDomain(std::map<llvm::Value *, PointerValue>);
        ValueDomain(Interval, uInterval, std::map<llvm::Value *, PointerValue>);
        ValueDomain(int64_t n, size_t);
        ValueDomain(size_t);
        ValueDomain(){}

        std::map<llvm::Value *, PointerValue> &getPointerMap();
        Interval &getRange();
        uInterval &getuRange();
        void disableRegionAligned();
        void insertPtrGenAction(PtrGen, size_t);
        void addPointer(Interval v, size_t, size_t);
        bool isTop();
        bool isBot();
        void setTop();
        bool isPointer();
        size_t getSize();
        
        bool operator==(const ValueDomain &) const;
        friend std::ostream &operator<<(std::ostream &os, const ValueDomain &p);
};

typedef llvm::Value MemBase;



struct State{
    ValueDomain vd;
    TaintDomain td;
    size_t getSize() { return vd.getSize(); }
    State(ValueDomain v, TaintDomain t) {
        vd = v;
        td = t;
    }
    State(size_t size) {
        vd = ValueDomain(size);
        td = TaintDomain(size);
    }
    State(){}
    bool operator==(const State&) const;
    friend std::ostream &operator<<(std::ostream &os, const State &obj);
};

struct MemoryRegion{
    bool aligned;
    std::vector<State> offset2state; 
    // compresse array to first element, flatten strcut member selection imformation to compute index
    bool high_taint;

    MemoryRegion(std::vector<State> statevec){
        aligned = true;
        offset2state = statevec;
        high_taint = false;
    };
    MemoryRegion(bool high){
        aligned = false;
        high_taint = high;
    };
    MemoryRegion(MemBase *, const llvm::DataLayout* );
    MemoryRegion(){}
    bool isAligned() { return aligned; }
    void disableAligned() { aligned = false; }
    bool containHighLable();
    bool operator==(const MemoryRegion &) const;
    friend std::ostream &operator<<(std::ostream &os, const MemoryRegion &obj);
};

void createMemoryCell(std::vector<State> &, llvm::Type *,const llvm::DataLayout*);

typedef std::map<MemBase *, MemoryRegion> MemStateMap;


#endif