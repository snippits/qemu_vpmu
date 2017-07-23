#ifndef __VPMU_MATH_HPP_
#define __VPMU_MATH_HPP_
#pragma once

namespace vpmu
{
namespace math
{
    double l2_norm(const std::vector<double> &u);
    void normalize(const std::vector<double> &in_v, std::vector<double> &out_v);
    void normalize(std::vector<double> &vec);

    inline uint64_t simple_hash(uint64_t key, uint64_t m) { return (key % m); }

    // http://zimbry.blogspot.tw/2011/09/better-bit-mixing-improving-on.html
    inline uint64_t bitmix_hash(uint64_t key)
    {
        key = (key ^ (key >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        key = (key ^ (key >> 27)) * UINT64_C(0x94d049bb133111eb);
        key = key ^ (key >> 31);

        return key;
    }

    inline uint32_t ilog2(uint32_t x)
    {
        uint32_t i;
        for (i = -1; x != 0; i++) x >>= 1;
        return i;
    }

    inline uint64_t sum_cores(uint64_t value[])
    {
        uint64_t sum = 0;
        for (int i = 0; i < VPMU.platform.cpu.cores; i++) {
            sum += value[i];
        }
        return sum;
    }
} // End of namespace vpmu::math
}

#endif
