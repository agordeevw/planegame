#pragma once
#include <random>

class Random {
public:
  template <class T>
  T next(T min, T max) {
    std::uniform_real_distribution<T> distr(min, max);
    return distr(m_generator);
  }

private:
  std::mt19937_64 m_generator;
};
