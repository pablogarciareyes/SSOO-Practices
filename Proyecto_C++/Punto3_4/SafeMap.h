#ifndef SAFEMAP_H
#define SAFEMAP_H

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <expected>
#include <format>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

class SafeMap {
  public:
    explicit SafeMap(std::string_view sv) noexcept : sv_{sv} {}
    explicit SafeMap() noexcept : sv_{""} {}
    SafeMap(const SafeMap&) = delete;
    SafeMap& operator=(const SafeMap&) = delete;
    SafeMap(SafeMap&& other) noexcept : sv_{other.sv_} {
      other.sv_ = "";
    }
    
    SafeMap& operator=(SafeMap&& other) noexcept 
    {
      if (this != &other && sv_ != other.sv_)
      {
        // Mover el descriptor de archivo de 'other' a este objeto
        sv_ = other.sv_;
        other.sv_ = "";
      }
      return *this;
    }

    ~SafeMap() noexcept {
      if (sv_.size() > 0) {
        munmap(const_cast<char*>(sv_.data()), sv_.size());
      }
    }

    [[nodiscard]] bool is_valid() const noexcept {
      return sv_.size() > 0;
    }

    [[nodiscard]] std::string_view get() const noexcept {
      return sv_;
    }
  private:
    std::string_view sv_;
};

#endif