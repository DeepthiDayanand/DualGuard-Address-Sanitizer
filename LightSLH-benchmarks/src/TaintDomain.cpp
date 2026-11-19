
#include "AbstractDomain.h"
#include "util.h"
#include "llvm/IR/Instruction.h"
#include <cstdint>
#include <cassert>

using namespace llvm;

std::vector<TaintDomain::bitTaintLable> get_array_from_num(uint64_t num, size_t size){
    std::vector<TaintDomain::bitTaintLable> taintarray;
    for (int64_t i = 0; i < size; i++){
        taintarray.push_back(static_cast<TaintDomain::bitTaintLable>((num >> i) & 1));
    }
    return taintarray;
}

uint64_t get_num_from_taint_domain(TaintDomain& t)
{ // t.is_bot should be false;
    uint64_t num = 0;
    for (int64_t i = 0; i < t.getSize();i++){
        if(t.getTaintLVector()[i] ==TaintDomain::bitTaintLable::ONE){
            num |= (uint64_t)1 << i;
        } else if(t.getTaintLVector()[i]==TaintDomain::bitTaintLable::ZERO){
            continue;
        } else{
            break;
        }
    }
    return num;
}

void TaintDomain::update_information(){
    contain_bot = (std::find(taint_vector.begin(), taint_vector.end(), TaintDomain::bitTaintLable::BOT) == taint_vector.end()) ? false : true;
    auto first_high_it = std::find(taint_vector.begin(), taint_vector.end(), TaintDomain::bitTaintLable::HIGH);
    auto first_ge_low_it = std::find(taint_vector.begin(), first_high_it, TaintDomain::bitTaintLable::LOW);
    auto first_ge_one_it = std::find(taint_vector.begin(), first_ge_low_it, TaintDomain::bitTaintLable::ONE);

    first_high = (first_high_it == taint_vector.end()) ? taint_vector.size() : std::distance(taint_vector.begin(), first_high_it);
    first_ge_low = (first_ge_low_it == first_high_it) ? first_high : std::distance(taint_vector.begin(), first_ge_low_it);
    first_ge_one = (first_ge_one_it == first_ge_low_it) ? first_ge_low : std::distance(taint_vector.begin(), first_ge_one_it);
}

//TODO: constructor can update information of TaintDomain on-the-run for higher efiiciency, instead of calling update_information()
TaintDomain::TaintDomain(TaintDomain::bitTaintLable t, size_t size){
    taint_vector.assign(size, t);
    update_information();
}

TaintDomain::TaintDomain(std::vector<TaintDomain::bitTaintLable>& t_vector){
    taint_vector = t_vector;
    update_information();
}


TaintDomain::TaintDomain(int64_t num, size_t size){
    taint_vector = get_array_from_num(static_cast<uint64_t>(num),size);
    update_information();
}

TaintDomain::TaintDomain(size_t size){
    taint_vector.assign(size, TaintDomain::bitTaintLable::BOT);
    update_information();
}

std::vector<TaintDomain::bitTaintLable>& TaintDomain::getTaintLVector(){
    return taint_vector;
}

bool isValue(TaintDomain::bitTaintLable t){
    return t == TaintDomain::bitTaintLable::ZERO || t == TaintDomain::bitTaintLable::ONE;
}

void TaintDomain::maskLowBit(uint64_t mask){
    for (int64_t i = 1,j = 0; i < (int64_t)mask;i = i << 1){
        taint_vector[j++] = TaintDomain::bitTaintLable::ZERO;
    }
    update_information();
}

void TaintDomain::maskHighBit(uint64_t bound){
    for (int64_t i = 1; i < getSize();i++){
        if(((uint64_t)1 << i) > bound){
            taint_vector[i] = TaintDomain::bitTaintLable::ZERO;
        }
    }
    update_information();
}

template <>
TaintDomain::bitTaintLable AbstractDomain::greatest_lower_bound(TaintDomain::bitTaintLable& a, TaintDomain::bitTaintLable& b){
    if(a == TaintDomain::bitTaintLable::ZERO && b==TaintDomain::bitTaintLable::ONE){
        return TaintDomain::bitTaintLable::BOT;
    }
    if(a == TaintDomain::bitTaintLable::ONE && b==TaintDomain::bitTaintLable::ZERO){
        return TaintDomain::bitTaintLable::BOT;
    }
    return a < b ? a : b;
}

template <>
TaintDomain AbstractDomain::greatest_lower_bound(TaintDomain& a, TaintDomain& b){
    assert(a.getSize() == b.getSize());
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    for (int64_t i = 0; i < a.getSize(); i++){
        new_taint_vector.push_back(AbstractDomain::greatest_lower_bound(a.getTaintLVector()[i], b.getTaintLVector()[i]));
    }
    return TaintDomain(new_taint_vector);
}

template <>
TaintDomain AbstractDomain::greatest_lower_bound(std::set<TaintDomain*>& elements) {
    assert(!(elements.empty()));
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    new_taint_vector.assign((*(elements.begin()))->getSize(),TaintDomain::bitTaintLable::HIGH);
    TaintDomain dst = TaintDomain(new_taint_vector);
    for (auto element : elements){
        dst = AbstractDomain::greatest_lower_bound(dst, *element);
    }
    return dst;
}

template <>
TaintDomain::bitTaintLable AbstractDomain::least_upper_bound(TaintDomain::bitTaintLable& a, TaintDomain::bitTaintLable& b){
    if(a == TaintDomain::bitTaintLable::ZERO && b==TaintDomain::bitTaintLable::ONE){
        return TaintDomain::bitTaintLable::LOW;
    }
    if(a == TaintDomain::bitTaintLable::ONE && b==TaintDomain::bitTaintLable::ZERO){
        return TaintDomain::bitTaintLable::LOW;
    }
    return a > b ? a : b;
}

template <>
TaintDomain AbstractDomain::least_upper_bound(TaintDomain& a, TaintDomain& b){
    assert(a.getSize() == b.getSize());
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    for (int64_t i = 0; i < a.getSize(); i++){
        new_taint_vector.push_back(AbstractDomain::least_upper_bound(a.getTaintLVector()[i], b.getTaintLVector()[i]));
    }
    return TaintDomain(new_taint_vector);
}

template <>
TaintDomain AbstractDomain::least_upper_bound(std::set<TaintDomain*>& elements){
    assert(!(elements.empty()));
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    new_taint_vector.assign((*(elements.begin()))->getSize(), TaintDomain::bitTaintLable::BOT);
    TaintDomain dst = TaintDomain(new_taint_vector);
    for (auto element : elements){
        dst = AbstractDomain::least_upper_bound(dst, *element);
    }
    return dst;
}

template <>
TaintDomain AbstractDomain::NOT(TaintDomain& src){
    TaintDomain dst;
    dst.getTaintLVector().resize(src.getSize());
    for (int64_t i = 0; i < src.getSize(); i++)
    {
        switch (src.getTaintLVector()[i]){
        case TaintDomain::bitTaintLable::ZERO:
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ONE;
            break;
        case TaintDomain::bitTaintLable::ONE:
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ZERO;
            break;
        default:
            dst.getTaintLVector()[i] = src.getTaintLVector()[i];
        }
    }
    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::AND(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    for (int64_t i = 0; i < src1.getSize();i++){
        if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::BOT || src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::BOT){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::BOT;
        } else if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::ZERO || src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::ZERO){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ZERO;
        } else if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::ONE && src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::ONE){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ONE;
        } else {
            dst.getTaintLVector()[i] = least_upper_bound(src1.getTaintLVector()[i], src2.getTaintLVector()[i]);
        }
    }
    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::OR(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    for (int64_t i = 0; i < src1.getSize();i++){
        if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::BOT || src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::BOT){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::BOT;
        } else if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::ONE || src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::ONE){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ONE;
        } else if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::ZERO && src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::ZERO){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ZERO;
        } else {
            dst.getTaintLVector()[i] = least_upper_bound(src1.getTaintLVector()[i], src2.getTaintLVector()[i]);
        }
    }
    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::XOR(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    for (int64_t i = 0; i < src1.getSize();i++){
        if(src1.getTaintLVector()[i]==TaintDomain::bitTaintLable::BOT || src2.getTaintLVector()[i]==TaintDomain::bitTaintLable::BOT){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::BOT;
        } else if(src1.getTaintLVector()[i]>=TaintDomain::bitTaintLable::LOW || src2.getTaintLVector()[i]>=TaintDomain::bitTaintLable::LOW){
            dst.getTaintLVector()[i] = least_upper_bound(src1.getTaintLVector()[i], src2.getTaintLVector()[i]);
        } else if(src1.getTaintLVector()[i] == src2.getTaintLVector()[i]){
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ZERO;
        } else{
            dst.getTaintLVector()[i] = TaintDomain::bitTaintLable::ONE;
        }
    }
    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::PLUS(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return TaintDomain(TaintDomain::bitTaintLable::BOT,src1.getSize());
    }
    // src1 and src2 both contains no bot;
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());

    auto &src1_vec = src1.getTaintLVector();
    auto &src2_vec = src2.getTaintLVector();
    auto &dst_vec = dst.getTaintLVector();
    bool inhigh = false, inlow = false;
    int64_t carry = 0;
    for (int64_t i = 0; i < src1.getSize(); i++){
        if(inhigh || inlow){
            if(inhigh){
                dst_vec[i] = TaintDomain::bitTaintLable::HIGH;
            }else{
                dst_vec[i] =TaintDomain::bitTaintLable::LOW;
            }
            if(src1_vec[i] == TaintDomain::bitTaintLable::ZERO || src2_vec[i] == TaintDomain::bitTaintLable::ZERO){
                inhigh = false;
                inlow = false;
            }
        }else{
            if(src1_vec[i] == TaintDomain::bitTaintLable::HIGH || src2_vec[i] == TaintDomain::bitTaintLable::HIGH){
                dst_vec[i] = TaintDomain::bitTaintLable::HIGH;
                if((carry + src1_vec[i] + src2_vec[i]) <= TaintDomain::bitTaintLable::HIGH){
                    carry = 0;
                }else{
                    inhigh = true;
                }
            }else if(src1_vec[i] == TaintDomain::bitTaintLable::LOW || src2_vec[i] == TaintDomain::bitTaintLable::LOW){
                dst_vec[i] = TaintDomain::bitTaintLable::LOW;
                if((carry + src1_vec[i] + src2_vec[i]) <= TaintDomain::bitTaintLable::HIGH){
                    carry = 0;
                }else{
                    inlow = true;
                }
            }else{
                dst_vec[i] = ((src1_vec[i] + src2_vec[i] + carry) % 2) == 0 ? TaintDomain::bitTaintLable::ZERO :TaintDomain::bitTaintLable::ONE;
                carry = (src1_vec[i] + src2_vec[i] + carry) / 2;
            }
        }
    }

    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::MINUS(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    if(src1.isBot() || src2.isBot()){
        return TaintDomain(TaintDomain::bitTaintLable::BOT,src1.getSize());
    }
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    // src1 and src2 both contains no bot;

    auto &src1_vec = src1.getTaintLVector();
    auto &src2_vec = src2.getTaintLVector();
    auto &dst_vec = dst.getTaintLVector();
    bool inhigh = false, inlow = false;
    int64_t carry = 0;
    for (int64_t i = 0; i < src1.getSize(); i++){
        if(inhigh || inlow){
            if(inhigh){
                dst_vec[i] = TaintDomain::bitTaintLable::HIGH;
            }else{
                dst_vec[i] =TaintDomain::bitTaintLable::LOW;
            }
            if(src1_vec[i] == TaintDomain::bitTaintLable::ONE && src2_vec[i] == TaintDomain::bitTaintLable::ZERO){
                if(carry == 0){
                    inhigh = false;
                    inlow = false;
                }
                carry = 0;
            }else{
                carry = 1;
            }
        }else{
            if(src1_vec[i] == TaintDomain::bitTaintLable::HIGH || src2_vec[i] == TaintDomain::bitTaintLable::HIGH){
                inhigh = true;
                dst_vec[i] = TaintDomain::bitTaintLable::HIGH;
                if(src1_vec[i] == TaintDomain::bitTaintLable::HIGH){
                    carry = (carry == 0 && src2_vec[i] == 0) ? 0 : 1;
                }else{
                    carry = (carry == 0 && src1_vec[i] == 1) ? 0 : 1;
                }
            }else if(src1_vec[i] == TaintDomain::bitTaintLable::LOW || src2_vec[i] == TaintDomain::bitTaintLable::LOW){
                inlow = true;
                dst_vec[i] = TaintDomain::bitTaintLable::LOW;
                if(src1_vec[i] == TaintDomain::bitTaintLable::LOW){
                    carry = (carry == 0 && src2_vec[i] == 0) ? 0 : 1;
                }else{
                    carry = (carry == 0 && src1_vec[i] == 1) ? 0 : 1;
                }
            }else{
                dst_vec[i] = ((src1_vec[i] - src2_vec[i] - carry) % 2) == 0 ? TaintDomain::bitTaintLable::ZERO :TaintDomain::bitTaintLable::ONE;
                carry = (src1_vec[i] >= src2_vec[i] + carry) ? 0 : 1;
            }
        }
    }

    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::LSHIFT(TaintDomain& src1, TaintDomain& src2){
    // dst = src1 << src2;
    assert(src1.getSize() == src2.getSize());
    if (src1.isBot() || src2.isBot()){
        return TaintDomain(TaintDomain::bitTaintLable::BOT,src1.getSize());
    }
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    // src1 and src2 both contains no bot;
    uint64_t num = get_num_from_taint_domain(src2);
    if(num >= src1.getSize()){ // overflow
        return TaintDomain(TaintDomain::bitTaintLable::ZERO,src1.getSize());
    }
    if (!src2.isBot() && src2.get_first_ge_low() == src1.getSize()){
        // src2 is a pure number
        for (int64_t i = src1.getTaintLVector().size() - 1; i >= (int64_t)num;i--){
            dst.getTaintLVector()[i] = src1.getTaintLVector()[i - num];
        }
        std::fill(dst.getTaintLVector().begin(), dst.getTaintLVector().begin() + num, TaintDomain::bitTaintLable::ZERO);
        dst.update_information();
        return dst; 
    }
    if(src2.containHigh()){
        return TaintDomain(TaintDomain::bitTaintLable::HIGH,src1.getSize());
    }

    auto high_index = (num + src1.get_first_high()) > src1.getSize() ? src1.getSize() : (num + src1.get_first_high());
    auto low_index = (num + src1.get_first_ge_one()) > src1.getSize() ? src1.getSize() : (num + src1.get_first_ge_one());
    std::fill(dst.getTaintLVector().begin() + high_index, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::HIGH);
    std::fill(dst.getTaintLVector().begin() + low_index, dst.getTaintLVector().begin() + high_index, TaintDomain::bitTaintLable::LOW);
    std::fill(dst.getTaintLVector().begin(), dst.getTaintLVector().begin() + low_index, TaintDomain::bitTaintLable::ZERO);
    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::LOGIC_RSHIFT(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    // dst = src1 >> src2;
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    if (src1.isBot() || src2.isBot()){
        return TaintDomain(TaintDomain::bitTaintLable::BOT,src1.getSize());
    }
    // src1 and src2 both contains no bot;
    uint64_t num = get_num_from_taint_domain(src2);
    if(num >= src1.getSize()){ // overflow
        return TaintDomain(TaintDomain::bitTaintLable::ZERO,src1.getSize());
    }
    if (!src2.isBot() && src2.get_first_ge_low() == src1.getSize()){
        // src2 is a pure number
        for (int64_t i = 0; i < src1.getSize() - num ;i++){
            dst.getTaintLVector()[i] = src1.getTaintLVector()[i + num];
        }
        std::fill(dst.getTaintLVector().begin() + src1.getSize() - num, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::ZERO);
        dst.update_information();
        return dst; 
    }
    if(src2.containHigh()){
        return TaintDomain(TaintDomain::bitTaintLable::HIGH,src1.getSize());
    }

    auto high_it = std::find(src1.getTaintLVector().rbegin(), src1.getTaintLVector().rend(), TaintDomain::bitTaintLable::HIGH);
    auto low_it = std::find(src1.getTaintLVector().rbegin(), high_it, TaintDomain::bitTaintLable::LOW);
    auto one_it = std::find(src1.getTaintLVector().rbegin(), low_it, TaintDomain::bitTaintLable::ONE);

    int64_t last_high = (high_it == src1.getTaintLVector().rend()) ? -1 : std::distance(high_it, src1.getTaintLVector().rend()) - 1;
    int64_t last_ge_low = (low_it == high_it) ? last_high : std::distance(low_it, src1.getTaintLVector().rend()) - 1;
    int64_t last_ge_one = (one_it == low_it) ? last_ge_low : std::distance(one_it, src1.getTaintLVector().rend()) - 1;

    auto high_index = (last_high - num) > -1 ? 0 : (last_high - num + 1);
    auto low_index = (last_ge_one - num) > -1 ? 0 : (last_ge_one - num + 1);
    std::fill(dst.getTaintLVector().begin(), dst.getTaintLVector().begin() + high_index, TaintDomain::bitTaintLable::HIGH);
    std::fill(dst.getTaintLVector().begin() + high_index, dst.getTaintLVector().begin() + low_index, TaintDomain::bitTaintLable::LOW);
    std::fill(dst.getTaintLVector().begin() + low_index, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::ZERO);
    dst.update_information();
    return dst;
}

template <>
TaintDomain AbstractDomain::ARITH_RSHIFT(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    // dst = src1 >> src2;
    if (src1.isBot() || src2.isBot()){
        return TaintDomain(TaintDomain::bitTaintLable::BOT,src1.getSize());
    }
    // src1 and src2 both contains no bot;
    uint64_t num = get_num_from_taint_domain(src2);
    TaintDomain::bitTaintLable most_significant_bit = src1.getTaintLVector()[src1.getTaintLVector().size() - 1];
    if(num >= src1.getSize()){ // overflow
        return TaintDomain(most_significant_bit, src1.getSize());
    }
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    if (!src2.isBot() && src2.get_first_ge_low() == src1.getSize()){
        // src2 is a pure number
        for (int64_t i = 0; i < src1.getSize() - num ;i++){
            dst.getTaintLVector()[i] = src1.getTaintLVector()[i + num];
        }
        std::fill(dst.getTaintLVector().begin() + num, dst.getTaintLVector().end(), most_significant_bit);
        dst.update_information();
        return dst; 
    }
    // src2 is not a pure number
    if(most_significant_bit == TaintDomain::bitTaintLable::HIGH){
        dst.getTaintLVector().assign(src1.getSize(), TaintDomain::bitTaintLable::HIGH);
        dst.update_information();
        return dst;
    }
    if(src2.containHigh()){
        return TaintDomain(TaintDomain::bitTaintLable::HIGH,src1.getSize());
    }

    auto high_it = std::find(src1.getTaintLVector().rbegin(), src1.getTaintLVector().rend(), TaintDomain::bitTaintLable::HIGH);
    auto low_it = std::find(src1.getTaintLVector().rbegin(), high_it, TaintDomain::bitTaintLable::LOW);
    auto one_it = std::find(src1.getTaintLVector().rbegin(), low_it, TaintDomain::bitTaintLable::ONE);
    auto zero_it = std::find(src1.getTaintLVector().rbegin(), low_it, TaintDomain::bitTaintLable::ZERO);

    int8_t last_high = (high_it == src1.getTaintLVector().rend()) ? -1 : std::distance(high_it, src1.getTaintLVector().rend()) - 1;
    int8_t last_ge_low = (low_it == high_it) ? last_high : std::distance(low_it, src1.getTaintLVector().rend()) - 1;
    int8_t last_ge_one = (one_it == low_it) ? last_ge_low : std::distance(one_it, src1.getTaintLVector().rend()) - 1;
    int8_t last_ge_zero = (zero_it == low_it) ? last_ge_low : std::distance(zero_it, src1.getTaintLVector().rend()) - 1;

    auto high_index = (last_high - num) > -1 ? 0 : (last_high - num + 1);
    std::fill(dst.getTaintLVector().begin(), dst.getTaintLVector().begin() + high_index, TaintDomain::bitTaintLable::HIGH);
    if(most_significant_bit == TaintDomain::bitTaintLable::LOW){
        std::fill(dst.getTaintLVector().begin() + high_index, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::LOW);
    } else if(most_significant_bit == TaintDomain::bitTaintLable::ZERO){
        auto low_index = (last_ge_one - num) > -1 ? 0 : (last_ge_one - num + 1);
        std::fill(dst.getTaintLVector().begin() + high_index, dst.getTaintLVector().begin() + low_index, TaintDomain::bitTaintLable::LOW);
        std::fill(dst.getTaintLVector().begin() + low_index, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::ZERO);
    } else if(most_significant_bit == TaintDomain::bitTaintLable::ZERO){
        auto low_index = (last_ge_zero - num) > -1 ? 0 : (last_ge_zero - num + 1);
        std::fill(dst.getTaintLVector().begin() + high_index, dst.getTaintLVector().begin() + low_index, TaintDomain::bitTaintLable::LOW);
        std::fill(dst.getTaintLVector().begin() + low_index, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::ONE);
    }
    dst.update_information();
    return dst;
}

template <> 
TaintDomain AbstractDomain::TIME(TaintDomain& src1, TaintDomain& src2){
    assert(src1.getSize() == src2.getSize());
    if (src1.isBot() || src2.isBot()){
        return TaintDomain(TaintDomain::bitTaintLable::BOT,src1.getSize());
    }
    TaintDomain dst;
    dst.getTaintLVector().resize(src1.getSize());
    auto high_index = std::min(src1.getSize(), std::min(src1.get_first_high() + src2.get_first_ge_one(), src2.get_first_high() + src1.get_first_ge_one()));
    auto low_index = std::min(src1.getSize(), std::min(src1.get_first_ge_low() + src2.get_first_ge_one(), src2.get_first_ge_low() + src1.get_first_ge_one()));
    uint64_t num = static_cast<uint64_t>(
        static_cast<int64_t>(get_num_from_taint_domain(src1)) * static_cast<int64_t>(get_num_from_taint_domain(src2)));
    std::vector<TaintDomain::bitTaintLable> num_vector = get_array_from_num(num, src1.getSize());

    std::fill(dst.getTaintLVector().begin() + high_index, dst.getTaintLVector().end(), TaintDomain::bitTaintLable::HIGH);
    std::fill(dst.getTaintLVector().begin() + low_index, dst.getTaintLVector().begin() + high_index, TaintDomain::bitTaintLable::LOW);
    std::copy(num_vector.begin(), num_vector.begin() + low_index, dst.getTaintLVector().begin());

    dst.update_information();
    return dst;
}

template <> 
TaintDomain AbstractDomain::extract(TaintDomain& src, uint offset1, uint offset2){
    assert((offset1 >= 0) && (offset2 >= offset1) && (offset2 <= src.getSize()));
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    new_taint_vector.assign(src.getTaintLVector().begin() + offset1, src.getTaintLVector().begin() + offset2);
    return TaintDomain(new_taint_vector);
}

template <> 
TaintDomain AbstractDomain::concatenation(std::vector<TaintDomain*>& tdvector){
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    for (auto i = 0; i < tdvector.size();i++){
        new_taint_vector.insert(new_taint_vector.end(), tdvector[i]->getTaintLVector().begin(), tdvector[i]->getTaintLVector().end());
    }
    return TaintDomain(new_taint_vector);
}

template <> 
TaintDomain AbstractDomain::ZEXT(TaintDomain& src, uint size){
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    new_taint_vector.assign(size - src.getSize(), TaintDomain::bitTaintLable::ZERO);
    new_taint_vector.insert(new_taint_vector.begin(), src.getTaintLVector().begin(), src.getTaintLVector().end());
    return TaintDomain(new_taint_vector);
}

template <> 
TaintDomain AbstractDomain::SEXT(TaintDomain& src, uint size){
    std::vector<TaintDomain::bitTaintLable> new_taint_vector;
    new_taint_vector.assign(size - src.getSize(), src.getTaintLVector()[src.getTaintLVector().size() - 1]);
    new_taint_vector.insert(new_taint_vector.begin(), src.getTaintLVector().begin(), src.getTaintLVector().end());
    return TaintDomain(new_taint_vector);
}

bool TaintDomain::operator==(const TaintDomain& other) const{
    if(taint_vector.size()!= other.taint_vector.size()){
        return false;
    }
    for (int64_t i = 0; i < taint_vector.size();i++){
        if(taint_vector[i]!=other.taint_vector[i]){
            return false;
        }
    }
    if(contain_bot != other.contain_bot){
        return false;
    }
    if(first_ge_one != other.first_ge_one){
        return false;
    }
    if(first_ge_low != other.first_ge_low){
        return false;
    }
    if(first_high != other.first_high){
        return false;
    }
    return true;
}

std::optional<uint> OverTaintDetector::acceptNewVal(llvm::Instruction * inst){
    if(ds == DetectionState::Wait){
        if(isa<BitCastInst>(inst) || isa<PtrToIntInst>(inst) || isa<IntToPtrInst>(inst)
        || isa<ZExtInst>(inst) || isa<SExtInst>(inst) || isa<TruncInst>(inst)){
            if(waitlist.empty()){
                waitlist.insert(inst);
                waitlist.insert(inst->getOperand(0));
            }else{
                if(waitlist.find(inst)==waitlist.end()){
                    waitlist.clear();
                    waitlist.insert(inst);
                    waitlist.insert(inst->getOperand(0));
                }else{
                    waitlist.insert(inst);
                }
            }   
        }else if(isa<BinaryOperator>(inst) && inst->getOpcode() == llvm::Instruction::And){
            //DO_MASK
            Value *op0 = inst->getOperand(0);
            Value *op1 = inst->getOperand(1);
            if (isa<ConstantInt>(op0)){
                int64_t val = getConcreteValue(dyn_cast<ConstantInt>(op0));
                if(is_power_of_two(val + 1)){
                    mask = val + 1;
                    ds = DetectionState::DoneMask;
                    if(waitlist.find(op1)==waitlist.end()){
                        if(!waitlist.empty()){
                            waitlist.clear();
                        }
                        waitlist.insert(op1);
                    }
                }
            }else if (isa<ConstantInt>(op1)){
                int64_t val = getConcreteValue(dyn_cast<ConstantInt>(op1));
                if(is_power_of_two(val + 1)){
                    mask = val + 1;
                    ds = DetectionState::DoneMask;
                    masked_val = inst;
                    if(waitlist.find(op0)==waitlist.end()){
                        if(!waitlist.empty()){
                            waitlist.clear();
                        }
                        waitlist.insert(op0);
                    }
                }
            }
        }else{
            waitlist.clear();
        }
    }else if(ds == DetectionState::DoneMask){
        //DO_SUB
        bool transmition_flag = false;
        if(isa<BinaryOperator>(inst) && inst->getOpcode() == llvm::Instruction::Sub){
            Value *op0 = inst->getOperand(0);
            Value *op1 = inst->getOperand(1);
            if(isa<ConstantInt>(op0)){
                if((getConcreteValue(dyn_cast<ConstantInt>(op0)) == mask )
                && (op1 == masked_val)){
                    transmition_flag = true;
                    complement = inst;
                }
            }
        }
        if(transmition_flag){
            ds = DetectionState::DoneSub;
        }else{
            complement = nullptr;
            masked_val = nullptr;
            ds = DetectionState::Wait;
            waitlist.clear();
        }
    }else if(ds == DetectionState::DoneSub){
        bool transmition_flag = false;
        if(is_naive_add_inst(inst)){
            Value *op0 = inst->getOperand(0);
            Value *op1 = inst->getOperand(1);
            if(op0==complement && (waitlist.find(op1)!=waitlist.end())){
                transmition_flag = true;
            }
            if(op1==complement && (waitlist.find(op0)!=waitlist.end())){
                transmition_flag = true;
            }
        }
        if(transmition_flag){
            ds = DetectionState::DoneAdd;
        }else{
            ds = DetectionState::Wait;
            complement = nullptr;
            masked_val = nullptr;
            waitlist.clear();
        }
    } 

    if(ds == DetectionState::DoneAdd){
        ds = DetectionState::Wait;
        complement = nullptr;
        masked_val = nullptr;
        return mask;
    }else{
        return {};
    }
}