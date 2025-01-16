#ifndef SAFEFD_H
#define SAFEFD_H

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

class SafeFD
{
  public:
    explicit SafeFD(int fd) noexcept : fd_{fd} {}
    explicit SafeFD() noexcept : fd_{-1} {}
    SafeFD(const SafeFD&) = delete;
    SafeFD& operator=(const SafeFD&) = delete;
    SafeFD(SafeFD&& other) noexcept : fd_{other.fd_} {
      other.fd_ = -1;
    }
    SafeFD& operator=(SafeFD&& other) noexcept 
    {
      if (this != &other && fd_ != other.fd_)
      {
        // Cerrar el descriptor de archivo actual
        close(fd_);
        // Mover el descriptor de archivo de 'other' a este objeto
        fd_ = other.fd_;
        other.fd_ = -1;
      }
      return *this;
    }
    ~SafeFD() noexcept
    {
      if (fd_ >= 0)
      {
        close(fd_);
      }
    }
    [[nodiscard]] bool is_valid() const noexcept 
    {
      return fd_ >= 0;
    }
    [[nodiscard]] int get() const noexcept
    {
      return fd_;
    }
  private:
    int fd_;
};

#endif